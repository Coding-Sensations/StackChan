#pragma once
#include <cstdint>
#include <cstddef>
namespace direct_usb_pcm {
// Exclusive selected legacy USB owner. No constructor or status call opens IO.
// Claim is the shared composite/legacy mode claim, not a private substitute.
template<class Driver,class Claim> class SerialOwner {
 Driver& driver_;Claim& claim_;bool claimed_=false,touched_=false,owned_=false;
public:
 SerialOwner(Driver& d,Claim& c):driver_(d),claim_(c){}
 bool Open(){
  if(claimed_||!driver_.OnOwnerCore()||driver_.Installed()||!claim_.Take())return false;
  claimed_=true;
  // Recheck after taking the shared claim; never uninstall a foreign driver.
  if(driver_.Installed()){claim_.Release();claimed_=false;return false;}
  touched_=true; // installation failure can retain partially initialized SDK state
  owned_=driver_.Install();return owned_;
 }
 bool Owned()const{return owned_&&driver_.OnOwnerCore()&&driver_.Installed();}
 bool HasCustody()const{return claimed_;}
 int Read(uint8_t* p,size_t n){if(!Owned()||!p||!n||n>4096)return -1;int r=driver_.Read(p,n);return r>=0&&size_t(r)<=n?r:-1;}
 int Write(const uint8_t* p,size_t n){if(!Owned()||!p||!n||n>4096)return -1;int r=driver_.Write(p,n);return r>=0&&size_t(r)<=n?r:-1;}
 bool Close(){
  if(!claimed_)return true;
  if(!driver_.OnOwnerCore())return false;
  // If install touched global state, retain claim until its exact closure.
  if(touched_&&driver_.Installed()&&!driver_.Uninstall())return false;
  if(driver_.Installed())return false;
  touched_=owned_=false;claim_.Release();claimed_=false;return true;
 }
};
}
