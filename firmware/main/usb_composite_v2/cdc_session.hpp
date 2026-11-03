#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
namespace usb_composite_v2 {
inline std::atomic<void*> cdc_session_owner{nullptr};
// Logical CDC lease for unchanged provisioning/numeric/export owners.
// One serialized CPU0 caller; transport never starts USB or changes consent.
template<class Transport> class CdcSessionDriver {
 Transport& io_;bool owned_=false;uint64_t epoch_=0;size_t capacity_=64;
 bool Live()const{return owned_&&io_.CdcAlive()&&io_.CdcEpoch()==epoch_;}
 bool InstallCapacity(size_t capacity){
  if(!OnOwnerCore()||owned_||!io_.CdcAlive()||!io_.CdcTxIdle())return false;
  void* empty=nullptr;if(!cdc_session_owner.compare_exchange_strong(empty,this))return false;
  epoch_=io_.CdcEpoch();owned_=true;capacity_=capacity;
  if(!Live()){Uninstall();return false;}return true;
 }
public:
 explicit CdcSessionDriver(Transport& io):io_(io){}
 CdcSessionDriver(const CdcSessionDriver&)=delete;CdcSessionDriver& operator=(const CdcSessionDriver&)=delete;
 bool OnOwnerCore()const{return io_.OnOwnerCore();}
 bool Installed()const{return cdc_session_owner.load()!=nullptr;}
 bool Install(){return InstallCapacity(64);}bool InstallDiagnostic(){return InstallCapacity(256);}
 bool Uninstall(){
  if(!owned_)return true;
  if(!OnOwnerCore())return false;
  void* mine=this;if(!cdc_session_owner.compare_exchange_strong(mine,nullptr))return false;
  owned_=false;epoch_=0;return true; // Releases CDC session, never HID/MIDI/PHY.
 }
 int Read(uint8_t* bytes,size_t capacity){
  if(!OnOwnerCore()||!Live())return -1;
  const int count=io_.CdcRead(bytes,capacity);
  return Live()?count:-1; // A disconnect during the read cannot deliver a stale-session request.
 }
 int WriteAck(const uint8_t* bytes,size_t count){return OnOwnerCore()&&Live()&&count<=capacity_?io_.CdcWrite(bytes,count):-1;}
 bool FlushAck(){return OnOwnerCore()&&Live()&&io_.CdcFlushAck(epoch_,100);}
};
}
