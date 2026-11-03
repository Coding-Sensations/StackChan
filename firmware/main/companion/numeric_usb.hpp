#pragma once
#include "provisioning_ack.hpp"
#include <algorithm>
#include <array>
#include <string>
namespace companion {
struct NumericUsbRequest {std::string request;uint32_t boot_nonce=0;};
enum class NumericUsbOpenError {None,CleanupFailed,InvalidWindow,WrongCore,ForeignDriver,InstallFailed,ReadFailed,StaleInput,DriverLost,ReadyWriteFailed,ReadyFlushFailed};
inline const char* NumericUsbOpenMessage(NumericUsbOpenError error){
 switch(error){
 case NumericUsbOpenError::CleanupFailed:return "Pair unavailable: USB cleanup failed";
 case NumericUsbOpenError::InvalidWindow:return "Pair unavailable: invalid boot state";
 case NumericUsbOpenError::WrongCore:return "Pair unavailable: wrong USB task";
 case NumericUsbOpenError::ForeignDriver:return "Pair unavailable: USB already owned";
 case NumericUsbOpenError::InstallFailed:return "Pair unavailable: USB install failed";
 case NumericUsbOpenError::ReadFailed:return "Pair unavailable: USB read failed";
 case NumericUsbOpenError::StaleInput:return "Pair unavailable: stale USB input";
 case NumericUsbOpenError::DriverLost:return "Pair unavailable: USB driver changed";
 case NumericUsbOpenError::ReadyWriteFailed:return "Pair unavailable: READY write failed";
 case NumericUsbOpenError::ReadyFlushFailed:return "Pair unavailable: READY flush failed";
 default:return "Pair unavailable: unspecified USB failure";
 }
}
enum class NumericUsbRead {Waiting,Accepted,Rejected};
inline bool DecodeNumericUsb(const uint8_t* p,size_t n,uint32_t boot,NumericUsbRequest& out){
 out={};if(!p||n!=25||!boot||p[0]!='S'||p[1]!='C'||p[2]!='N'||p[3]!='P'||p[4]!=1)return false;
 uint32_t value=0;for(unsigned i=21;i<25;++i)value=(value<<8)|p[i];if(value!=boot)return false;
 bool nonzero=false;const char* hex="0123456789abcdef";for(unsigned i=5;i<21;++i){nonzero|=p[i]!=0;out.request.push_back(hex[p[i]>>4]);out.request.push_back(hex[p[i]&15]);}
 if(!nonzero){out={};return false;}out.boot_nonce=boot;return true;
}
// Serialized CPU0 owner. SCNP contains only a request UUID and boot nonce.
template<class Driver> class NumericUsbOwner {
 NumericUsbOpenError error_=NumericUsbOpenError::None;
 bool Failed(NumericUsbOpenError error){error_=error;return false;}
 Driver& driver_;bool owned_=false,active_=false;uint32_t boot_=0;uint64_t deadline_=0;std::array<uint8_t,26> bytes_{};size_t used_=0;
 bool Emit(const AckFrame& a){return owned_&&driver_.Installed()&&driver_.OnOwnerCore()&&driver_.WriteAck(a.data(),a.size())==static_cast<int>(a.size())&&driver_.FlushAck();}
public:
 explicit NumericUsbOwner(Driver& d):driver_(d){}~NumericUsbOwner(){Close();}
 NumericUsbOwner(const NumericUsbOwner&)=delete;NumericUsbOwner& operator=(const NumericUsbOwner&)=delete;
 NumericUsbOpenError LastOpenError()const{return error_;}
 bool Active()const{return owned_&&active_;}
 bool OwnsDriver()const{return owned_;}
 bool Close(){active_=false;bytes_.fill(0);used_=0;deadline_=0;if(!owned_)return true;if(!driver_.Installed()){owned_=false;return true;}if(!driver_.OnOwnerCore()||!driver_.Uninstall())return false;owned_=false;return true;}
 bool Open(uint64_t now,uint32_t boot,uint32_t reason){
  error_=NumericUsbOpenError::None;
  if(!Close())return Failed(NumericUsbOpenError::CleanupFailed);
  if(!boot||reason>64||now>UINT64_MAX-60000)return Failed(NumericUsbOpenError::InvalidWindow);
  if(!driver_.OnOwnerCore())return Failed(NumericUsbOpenError::WrongCore);
  if(driver_.Installed())return Failed(NumericUsbOpenError::ForeignDriver);
  if(!driver_.Install())return Failed(NumericUsbOpenError::InstallFailed);
  owned_=true;
  std::array<uint8_t,128> stale{};bool found=false;
  for(unsigned i=0;i<8;++i){int n=driver_.Read(stale.data(),stale.size());stale.fill(0);if(n<0||n>128){Close();return Failed(NumericUsbOpenError::ReadFailed);}if(!n)break;found=true;}
  if(found){Emit(EncodeStatusAck(AckStatus::Rejected));Close();return Failed(NumericUsbOpenError::StaleInput);}
  boot_=boot;deadline_=now+60000;active_=true;
  // Retain Emit's late ownership/driver/core checks before the single write.
  if(!owned_||!driver_.Installed()){Close();return Failed(NumericUsbOpenError::DriverLost);}
  if(!driver_.OnOwnerCore()){Close();return Failed(NumericUsbOpenError::WrongCore);}
  const auto ready=EncodeReadyAck(boot,reason);
  if(driver_.WriteAck(ready.data(),ready.size())!=static_cast<int>(ready.size())){Close();return Failed(NumericUsbOpenError::ReadyWriteFailed);}
  if(!driver_.FlushAck()){Close();return Failed(NumericUsbOpenError::ReadyFlushFailed);}
  return true;
 }
 NumericUsbRead Poll(uint64_t now,NumericUsbRequest& out){
  out={};if(!Active())return NumericUsbRead::Rejected;
  if(!driver_.Installed()||!driver_.OnOwnerCore()){Close();return NumericUsbRead::Rejected;}
  if(now>=deadline_){Emit(EncodeStatusAck(AckStatus::Expired));Close();return NumericUsbRead::Rejected;}
  int n=driver_.Read(bytes_.data()+used_,bytes_.size()-used_);
  if(n<0||static_cast<size_t>(n)>bytes_.size()-used_){Close();return NumericUsbRead::Rejected;}used_+=n;
  if(used_<25)return NumericUsbRead::Waiting;
  bool valid=DecodeNumericUsb(bytes_.data(),used_,boot_,out);
  if(!valid){Emit(EncodeStatusAck(AckStatus::Rejected));}
  bool closed=Close();if(!valid||!closed){out={};return NumericUsbRead::Rejected;}return NumericUsbRead::Accepted;
 }
};
}
