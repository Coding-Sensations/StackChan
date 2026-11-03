#include "companion/local_trust_binding.hpp"
#include "usb_pcm_runtime.h"
#include <esp_random.h>
#include <esp_system.h>
#include <mbedtls/build_info.h>
#include <ctime>
extern "C" const char* companion_local_ca_pem();
#include <companion/numeric_usb.hpp>
extern "C" const char* companion_local_tls_host();
namespace direct_usb_pcm {
Runtime::Runtime(std::function<uint64_t()> now,std::function<AudioCodec*()> codec):issuer_([this]()->const void*{return tls_.Verified()?&tls_:nullptr;}),endpoint_(tls_,bytes_),worker_(issuer_,endpoint_,now),now_(std::move(now)),codec_(std::move(codec)){}
bool Runtime::PrepareSelected(uint32_t boot,uint32_t epoch,uint16_t stream,uint64_t cutoff){
 if(!Closed()||!now_||!codec_||!epoch||!stream||cutoff<=now_())return false;
 issue_=SetupIssue::None;
#ifndef MBEDTLS_HAVE_TIME_DATE
 issue_=SetupIssue::ClockNotReady;state_=RuntimeState::Fault;return false;
#else
 if(std::time(nullptr)<1735689600){issue_=SetupIssue::ClockNotReady;state_=RuntimeState::Fault;return false;}
#endif
 const auto ca=companion_local_ca_pem();const auto host=companion_local_tls_host();if(!ca||!*ca||!host||!*host){issue_=SetupIssue::TrustUnavailable;state_=RuntimeState::Fault;return false;}
 if(!boot){do{esp_fill_random(&boot,sizeof(boot));}while(!boot);}
 boot_=boot;epoch_=epoch;stream_=stream;cutoff_=cutoff;failed_=staged_=false;ready_sent_=request_used_=0;request_bytes_.fill(0);parser_.Reset();dispatcher_.reset();
 if(!bytes_.OpenSelected()){Fail();return false;}
 // Refuse stale input before emitting READY. This is the existing public
 // SCPS/SCNP request handshake, not proof of TLS identity or audio authority.
 uint8_t stale[128]{};const auto n=bytes_.Read(stale,sizeof(stale));if(n!=0){Fail();return false;}
 ready_=companion::EncodeReadyAck(boot,uint32_t(esp_reset_reason()));state_=RuntimeState::Provisioning;return true;
}
bool Runtime::StartSelected(const std::string& request,uint32_t boot,uint32_t epoch,uint16_t stream,uint64_t cutoff){
 const bool handoff=state_==RuntimeState::Provisioning&&bytes_.Owned()&&boot==boot_&&epoch==epoch_&&stream==stream_&&cutoff==cutoff_;
 if((!Closed()&&!handoff)||!now_||!codec_||!epoch||!stream||!companion::PairHex(request)||cutoff<=now_())return false;
 uint8_t random[16];esp_fill_random(random,sizeof(random));std::string nonce;const char* hex="0123456789abcdef";for(auto b:random){nonce+=hex[b>>4];nonce+=hex[b&15];}
 if(!pair_.Start(request,nonce,boot,now_()))return false;
 epoch_=epoch;stream_=stream;cutoff_=cutoff;staged_=false;failed_=false;parser_.Reset();dispatcher_.reset();
 if(!handoff&&!bytes_.OpenSelected()){state_=RuntimeState::Closing;Stop();return false;}
 if(!tls_.Begin(bytes_,companion_local_tls_host())){state_=RuntimeState::Closing;Stop();return false;}
 state_=RuntimeState::Tls;return true;
}
void Runtime::Fail(){if(issue_==SetupIssue::None)issue_=SetupIssue::Protocol;failed_=true;if(worker_.Status().cleanup_failed){state_=RuntimeState::Closing;return;}state_=RuntimeState::Fault;Stop();}
void Runtime::Stop(){worker_.RequestStop();if(!worker_.Closed())worker_.Step();if(worker_.Closed()&&tls_.Stop()&&(!bytes_.HasCustody()||bytes_.CloseAndConfirm(bytes_.ChannelId()))){issuer_.Cancel();pair_.Cancel();dispatcher_.reset();parser_.Reset();state_=failed_?RuntimeState::Fault:RuntimeState::Closed;}else state_=RuntimeState::Closing;}
std::string Runtime::ConsentStamp()const{return issuer_.DisplayStamp();}
bool Runtime::ConfirmDisplayedCode(const std::string& code,const std::string& stamp){
 if(state_!=RuntimeState::Challenge||!tls_.Verified()||code!=issuer_.DisplayedCode()||stamp!=ConsentStamp())return false;
 if(!issuer_.ClickCaptured(code,stamp,LocalConsentIntent::MatchAndListen)||!issuer_.Confirm(now_())||!endpoint_.PairReply(UsbConfirm(pair_))){Fail();return false;}state_=RuntimeState::Confirmed;return true;
}
bool Runtime::Message(uint8_t kind,const uint8_t* p,size_t n){
 if(!tls_.Verified()||now_()>=cutoff_||!pair_.Active(now_()))return false;
 if(kind==2)return state_==RuntimeState::Media&&dispatcher_&&dispatcher_->Handle(p,n);
 const std::string text(reinterpret_cast<const char*>(p),n);
 if(state_==RuntimeState::Challenge){if(!companion::LocalTrustBindingMatches()||!ApplyUsbChallenge(text,pair_,epoch_,stream_,now_())||!issuer_.Stage(pair_,STACKCHAN_PIN_DEVICE,epoch_,stream_,cutoff_,&tls_,now_()))return false;return true;}
 if(state_==RuntimeState::Confirmed){if(!ApplyUsbGrant(text,pair_,issuer_.Selected(),now_())||!endpoint_.PairReply(UsbStaged(issuer_.Selected()))||!issuer_.Staged(issuer_.Selected(),&tls_,now_()))return false;staged_=true;state_=RuntimeState::WaitingReady;return true;}
 if(state_==RuntimeState::WaitingReady){if(!staged_||!UsbReady(text,pair_,issuer_.Selected(),now_())||!worker_.Select(codec_))return false;dispatcher_=std::make_unique<ControlDispatcher>(worker_,issuer_.Selected(),[this](const uint8_t* b,size_t size){return endpoint_.MediaReply(b,size);});endpoint_.Attach(*dispatcher_);state_=RuntimeState::Media;return true;}
 if(state_==RuntimeState::Media){if(!UsbHeartbeat(text,pair_,issuer_.Selected(),now_())||!worker_.Heartbeat(epoch_,stream_))return false;auto ack=text;auto at=ack.find("pair.usb-heartbeat");if(at==std::string::npos)return false;ack.replace(at,18,"pair.usb-heartbeat-ack");return endpoint_.PairReply(ack);}
 return false;
}
void Runtime::Tick(){
 if(state_==RuntimeState::Unselected||state_==RuntimeState::Closed)return;
 // A failed closure is retained. Only explicit Stop retries it.
 if(state_==RuntimeState::Closing||state_==RuntimeState::Fault)return;
 if(now_()>=cutoff_){Fail();return;}
 if(state_==RuntimeState::Provisioning){
  if(ready_sent_<ready_.size()){int n=bytes_.Write(ready_.data()+ready_sent_,ready_.size()-ready_sent_);if(n<0){Fail();return;}ready_sent_+=n;if(ready_sent_<ready_.size())return;}
  int n=bytes_.Read(request_bytes_.data()+request_used_,request_bytes_.size()-request_used_);if(n<0){Fail();return;}request_used_+=n;
  if(request_used_<25)return;
  companion::NumericUsbRequest request;if(!companion::DecodeNumericUsb(request_bytes_.data(),request_used_,boot_,request)||!StartSelected(request.request,boot_,epoch_,stream_,cutoff_)){Fail();return;}request_bytes_.fill(0);return;
 }
 if(!pair_.Active(now_())||pair_.ChallengeExpired(now_())){Fail();return;}
 if(state_==RuntimeState::Tls){int r=tls_.PollHandshake();if(r<0){Fail();return;}if(!r)return;if(!endpoint_.PairReply(UsbHello(pair_))){Fail();return;}state_=RuntimeState::Challenge;}
 const int n=tls_.Read(input_.data(),input_.size());if(n<0){Fail();return;}if(n>0&&!parser_.Feed(input_.data(),n,[this](uint8_t k,const uint8_t* p,size_t size){return Message(k,p,size);})){Fail();return;}
 if(state_==RuntimeState::Media){worker_.Step();if(worker_.Closed()||worker_.Status().cleanup_failed||!dispatcher_->Tick()){Fail();return;}}
 if(endpoint_.Flush()<0)Fail();
}
}
