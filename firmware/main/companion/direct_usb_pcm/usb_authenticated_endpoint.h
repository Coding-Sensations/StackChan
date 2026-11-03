#pragma once
#include "usb_pcm_worker.h"
#include "usb_records.hpp"
#include <companion/usb_tls_client.h>
namespace direct_usb_pcm {
class ControlDispatcher;
// Media endpoint takes the already verified pairing connection; it never opens
// a replacement USB/TLS connection after physical confirmation.
class AuthenticatedEndpoint final:public Endpoint {
 companion::UsbTlsClient& tls_;companion::UsbByteOwner& bytes_;
 const Admission* admission_=nullptr;uint64_t channel_=0;
 ControlDispatcher* dispatcher_=nullptr;
 std::array<uint8_t,8192> outgoing_{};size_t queued_=0,front_=0;bool failed_=false;
 bool Queue(uint8_t,const uint8_t*,size_t);
public:
 AuthenticatedEndpoint(companion::UsbTlsClient& t,companion::UsbByteOwner& b):tls_(t),bytes_(b){}
 void Attach(ControlDispatcher& d){dispatcher_=&d;}
 ClaimResult Claim(const Admission&)override;
 bool Current(const Admission&)const override;
 bool Close()override;
 bool MicCapacity(size_t)const override;
 bool Mic(const Identity&,uint32_t,const uint8_t*,size_t)override;
 bool SpeakerCredit(const Identity&,size_t)override;
 bool MediaReply(const uint8_t*,size_t);
 bool PairReply(const std::string&);
 size_t PendingBytes()const{return queued_;}
 int Flush();
};
}
