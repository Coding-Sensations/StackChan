#include "usb_authenticated_endpoint.h"
#include "usb_control_dispatcher.h"
namespace direct_usb_pcm {
bool AuthenticatedEndpoint::Queue(uint8_t kind,const uint8_t* p,size_t n){
 if(failed_||!tls_.Verified()||!p||!n||n>(kind==1?2048u:4120u)||queued_+5+n>outgoing_.size())return false;
 // Queue bytes are owned until BeginWrite copies them into retained TLS state.
 outgoing_[queued_]=kind;Put32(outgoing_.data()+queued_+1,n);std::copy_n(p,n,outgoing_.data()+queued_+5);queued_+=5+n;return true;
}
ClaimResult AuthenticatedEndpoint::Claim(const Admission& a){if(admission_||!tls_.Verified()||!bytes_.Owned()||!bytes_.ChannelId())return ClaimResult::Refused;failed_=false;admission_=&a;channel_=bytes_.ChannelId();return ClaimResult::Owned;}
bool AuthenticatedEndpoint::Current(const Admission& a)const{return !failed_&&admission_==&a&&channel_&&bytes_.ChannelId()==channel_&&tls_.Verified();}
bool AuthenticatedEndpoint::Close(){std::fill(outgoing_.begin(),outgoing_.end(),0);queued_=front_=0;if(!tls_.Stop())return false;admission_=nullptr;channel_=0;return true;}
bool AuthenticatedEndpoint::MicCapacity(size_t n)const{return n==640&&dispatcher_&&admission_&&Current(*admission_)&&dispatcher_->MicCapacity()&&queued_+669<=outgoing_.size();}
bool AuthenticatedEndpoint::Mic(const Identity& i,uint32_t s,const uint8_t* p,size_t n){return dispatcher_&&admission_&&Current(*admission_)&&dispatcher_->Mic(i,s,p,n);}
bool AuthenticatedEndpoint::SpeakerCredit(const Identity& i,size_t n){return dispatcher_&&admission_&&Current(*admission_)&&i.epoch==admission_->Bound().epoch&&i.stream==admission_->Bound().stream&&dispatcher_->Credit(n);}
bool AuthenticatedEndpoint::MediaReply(const uint8_t* p,size_t n){return admission_&&Current(*admission_)&&Queue(2,p,n);}
bool AuthenticatedEndpoint::PairReply(const std::string& s){return Queue(1,reinterpret_cast<const uint8_t*>(s.data()),s.size());}
int AuthenticatedEndpoint::Flush(){
 if(failed_||!tls_.Verified()){failed_=true;return -1;}
 if(front_){int r=tls_.PollWrite();if(r<0){failed_=true;return -1;}if(!r)return 0;std::move(outgoing_.begin()+front_,outgoing_.begin()+queued_,outgoing_.begin());queued_-=front_;front_=0;}
 if(!queued_)return 1;
 const auto n=5+Le32(outgoing_.data()+1);if(n>queued_||!tls_.BeginWrite(outgoing_.data(),n)){failed_=true;return -1;}front_=n;return 0;
}
}
