#pragma once
#include "usb_serial_sdk.h"
#include <companion/usb_tls_client.h>
#include <algorithm>
namespace direct_usb_pcm {
// Explicit selection opens only the existing legacy Serial/JTAG peripheral.
// No automatic composite/host role fallback, polling task or Wi-Fi connection.
class SerialByteOwner final:public companion::UsbByteOwner {
 SdkSerial sdk_;SharedUsbClaim claim_;SerialOwner<SdkSerial,SharedUsbClaim> serial_{sdk_,claim_};
 uint64_t generation_=0;
public:
 bool OpenSelected(){if(serial_.HasCustody()||generation_==UINT64_MAX)return false;++generation_;return serial_.Open();}
 bool Owned()const override{return serial_.Owned();}
 uint64_t ChannelId()const override{return generation_;}
 int Read(uint8_t* p,size_t n)override{return serial_.Read(p,std::min(n,size_t(4096)));}
 int Write(const uint8_t* p,size_t n)override{return serial_.Write(p,std::min(n,size_t(4096)));}
 bool DiscardPending(uint64_t expected)override{
  // There is no independent application ciphertext queue in this adapter.
  // SDK TX ownership is resolved by checked driver uninstall below.
  return expected&&expected==generation_;
 }
 bool CloseAndConfirm(uint64_t expected)override{return expected&&expected==generation_&&serial_.Close();}
 bool HasCustody()const{return serial_.HasCustody();}
};
}
