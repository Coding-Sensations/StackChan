// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <voice/voice_protocol.hpp>
#include <voice/review_bridge.hpp>
#include "muse_direct_audio.hpp"
#include <array>
#include <cstdint>
#include <cassert>
#include <cstring>
namespace voice_codec_prep {
struct Observation{bool known=false,mic_open=false,speaker_open=false,capturing=false,playing=false;};
class Permit{public:virtual ~Permit()=default;// Native owner checks its original actor/account/node/VM and all selected
 // current rights against the exact returned target. Bounded local check;
 // borrow target only during this call, never retain it or derive rights from it.
 // A local review receipt cannot implement native authority on its own.
 virtual bool MatchesReviewedContext(const muse_review::Target&,companion::muse::Directions,int64_t)const{return false;}
 virtual bool VoiceContext(uint64_t,int64_t)const=0;virtual bool Microphone(uint64_t,int64_t)const=0;virtual bool Output(uint64_t,int64_t)const=0;virtual bool Provider(uint64_t,int64_t)const=0;virtual bool Hold(uint64_t,uint64_t,int64_t)const=0;};
class NativeUnavailablePermit final:public Permit{public:bool VoiceContext(uint64_t,int64_t)const override{return false;}bool Microphone(uint64_t,int64_t)const override{return false;}bool Output(uint64_t,int64_t)const override{return false;}bool Provider(uint64_t,int64_t)const override{return false;}bool Hold(uint64_t,uint64_t,int64_t)const override{return false;}};
enum class Operation{Bind,Input,Output};
enum class ReadResult{Complete,Pending,Failed};
class Codec{
public:
 using Gate=std::function<bool(uint64_t,int64_t,Operation)>;
 virtual ~Codec()=default;
 virtual bool Reserve(uint64_t)=0;
 // No generation-only fallback can bind native hardware custody.
 virtual bool ReserveExact(uint64_t,const companion::AudioLease&,companion::muse::Directions,int64_t,muse_native_review::Clock*,Gate){return false;}
 virtual Observation Observe(uint64_t)=0;
 virtual bool EnableMicrophone(uint64_t)=0;virtual bool EnableOutput(uint64_t)=0;
 virtual size_t Read16k(uint64_t,int16_t*,size_t)=0;
 virtual ReadResult ReadFrame(uint64_t g,int16_t* out,size_t cap,size_t& written){written=Read16k(g,out,cap);return written==cap?ReadResult::Complete:ReadResult::Failed;}
 virtual size_t Write16k(uint64_t,const int16_t*,size_t)=0;
 virtual bool StopMicrophone(uint64_t)=0;virtual bool StopOutput(uint64_t)=0;
 virtual bool FlushOutput(uint64_t,uint64_t)=0;virtual bool Join(uint64_t)=0;virtual bool Release(uint64_t)=0;
};
using Directions=companion::muse::Directions;
enum class State{Dormant,Prepared,Ready,Retained,Closed};
enum class Direction{Off,Current,Unknown};
struct Snapshot{State state=State::Dormant;Direction microphone=Direction::Off,output=Direction::Off;bool stop_pending=false;};
class Owner{
 voice_prep::Protocol& protocol_;Codec& codec_;Permit& permit_;muse_native_review::Clock* clock_=nullptr;
 companion::muse::VoiceLease lease_;State state_=State::Dormant;
 uint64_t generation_=0,ticket_=0,last_ticket_=0;int64_t now_=0,expires_=0;
 Directions selected_{};
 bool spent_=false,leased_=false,bound_=false,talking_=false,preparing_=false;
 bool input_attempted_=false,output_attempted_=false,input_closed_=true,output_closed_=true;
 std::array<int16_t,320> capture_{};std::array<int16_t,1282> playback_{};
 static void Zero(void* p,size_t n){auto v=static_cast<volatile unsigned char*>(p);while(n--)*v++=0;}
 void Wipe(){Zero(capture_.data(),capture_.size()*sizeof(int16_t));Zero(playback_.data(),playback_.size()*sizeof(int16_t));}
 bool AuthorizeAt(uint64_t g,int64_t now,Operation operation){
  const auto resource=lease_.ResourceLease();const auto selected=selected_;const auto ticket=ticket_;
  if(!Same(g)||!leased_||now<now_||now>=expires_||
     (state_!=State::Prepared&&state_!=State::Ready)||!companion::CommonAudioAuthority().Current(resource))return false;
  if(!permit_.VoiceContext(g,now)||!permit_.Provider(g,now)||
     (selected.microphone&&!permit_.Microphone(g,now))||(selected.output&&!permit_.Output(g,now)))return false;
  if(operation==Operation::Input&&(!selected.microphone||!talking_||!ticket||!permit_.Hold(g,ticket,now)))return false;
  if(operation==Operation::Output&&(!selected.output||talking_))return false;
  return Same(g)&&leased_&&now>=now_&&now<expires_&&
    (state_==State::Ready||(operation==Operation::Bind&&state_==State::Prepared))&&
    (operation!=Operation::Input||(talking_&&ticket_==ticket))&&
    (operation!=Operation::Output||!talking_)&&companion::CommonAudioAuthority().Current(resource)&&
    lease_.ResourceLease().generation()==resource.generation();
 }
 bool Context()const{return (state_==State::Prepared||state_==State::Ready)&&now_<expires_&&permit_.VoiceContext(generation_,now_)&&permit_.Provider(generation_,now_);}
 bool Same(uint64_t g)const{return generation_&&g==generation_;}
 bool Guard(uint64_t g,int64_t now,bool ready=true,bool check_hold=true){
  if(!Same(g)||!leased_||(ready&&state_!=State::Ready)||state_==State::Retained)return false;
  int64_t effective=now;
  if(clock_){muse_native_review::Stamp sample{};
   if(!clock_->Sample(sample)||!Same(g)||!leased_||state_==State::Retained||state_==State::Closed||sample.mono_us<now){Stop(g,now_);return false;}
   effective=sample.mono_us;
  }
  if(effective<now_||effective<0){Stop(g,now_);return false;}
  now_=effective;
  if(!lease_.Current()||!Same(g)||!leased_||state_==State::Retained||state_==State::Closed){Stop(g,now_);return false;}
  if(ready&&check_hold&&talking_&&!permit_.Hold(g,ticket_,now_)){Stop(g,now);return false;}
  if(ready&&(!protocol_.Poll(g,now_)||protocol_.Status()!=voice_prep::State::Live)){Stop(g,now);return false;}
  return true;
 }
 bool DirectionGuard(uint64_t g,int64_t now,bool microphone){
  if(!Guard(g,now))return false;
  if(microphone?!selected_.microphone:!selected_.output)return false;
  if(!(microphone?permit_.Microphone(g,now_):permit_.Output(g,now_))){Stop(g,now);return false;}
  return Guard(g,now);
 }
 bool Observe(Observation& out){
  out=codec_.Observe(generation_);
  // Successful Reserve owns this generation. Remember reported open directions
  // before rechecking permits so revocation cannot strand their cleanup.
  if(out.mic_open){input_attempted_=true;input_closed_=false;}
  if(out.speaker_open){output_attempted_=true;output_closed_=false;}
  return out.known&&(!out.capturing||out.mic_open)&&(!out.playing||out.speaker_open);
 }
 bool CloseInput(){if(!input_attempted_||input_closed_)return true;if(!codec_.StopMicrophone(generation_))return false;input_closed_=true;return true;}
 bool CloseOutput(){if(!output_attempted_||output_closed_)return true;if(!codec_.StopOutput(generation_))return false;output_closed_=true;return true;}
public:
 Owner(voice_prep::Protocol& p,Codec& c,Permit& permit):protocol_(p),codec_(c),permit_(permit){}
 Owner(voice_prep::Protocol& p,Codec& c,Permit& permit,muse_native_review::Clock& clock):Owner(p,c,permit){clock_=&clock;}
 Owner(const Owner&)=delete;Owner& operator=(const Owner&)=delete;
 // A caller must retain uncertain cleanup, as existing firmware workers do.
 ~Owner(){assert(!leased_&&!bound_);Wipe();}
 State Status()const{return state_;}
 bool Prepare(uint64_t g,int64_t expires,int64_t now){return Prepare(g,expires,now,{true,true});}
 bool Prepare(uint64_t g,int64_t expires,int64_t now,Directions selected){
  if(preparing_||(!selected.microphone&&!selected.output)||spent_||!g||now<0||expires<=now||expires-now>120000000)return false;
  // Clock/permission ports may synchronously call back. A nested preparation
  // must not replace generation/state before the original lease is latched.
  struct Preparing{bool& flag;explicit Preparing(bool& f):flag(f){flag=true;}~Preparing(){flag=false;}} preparing(preparing_);
  selected_=selected;generation_=g;now_=now;expires_=expires;state_=State::Prepared;
  if(clock_){muse_native_review::Stamp sample{};if(!clock_->Sample(sample)||sample.mono_us<now||sample.mono_us>=expires){state_=State::Dormant;generation_=0;return false;}now_=sample.mono_us;}
  if(!lease_.Reserve(g,[this]{return Context();},[this]{return permit_.Microphone(generation_,now_);},[this]{return permit_.Output(generation_,now_);},selected_)){state_=State::Dormant;generation_=0;return false;}
  leased_=spent_=true;
  if(!lease_.Current()||!Same(g)||!leased_||state_==State::Retained||state_==State::Closed){Stop(g,now_);return false;}return true;
 }
 bool Start(uint64_t g,int64_t now){
  if(state_!=State::Prepared||!Guard(g,now,false))return false;
  if(!protocol_.Poll(g,now_)||protocol_.Status()!=voice_prep::State::Live){Stop(g,now);return false;}
  // Reserve(false) must be a side-effect-free unavailable response.
  if(!codec_.ReserveExact(g,lease_.ResourceLease(),selected_,expires_,clock_,[this](uint64_t captured,int64_t stamp,Operation operation){return AuthorizeAt(captured,stamp,operation);})){Stop(g,now_);return false;}bound_=true;
  if(!Guard(g,now,false))return false;
  Observation observed;
  if(!Observe(observed)||observed.mic_open||observed.speaker_open){
   // A successful binding owns this generation's directions, even if its
   // supposedly idle state is inconsistent. Never claim an unknown stop.
   input_attempted_=output_attempted_=true;input_closed_=output_closed_=false;Stop(g,now);return false;
  }
  if(!Guard(g,now,false))return false;
  state_=State::Ready;return true;
 }
 bool Flush(uint64_t g,int64_t now){
  if(!Guard(g,now))return false;
  const auto flush=protocol_.FlushTicket();if(!flush)return true;
  Wipe();Observation observed;
  if(!Observe(observed)){Stop(g,now);return false;}
  if(!Guard(g,now))return false;
  if(observed.speaker_open){output_attempted_=true;output_closed_=false;}
  if(!CloseOutput()){Stop(g,now);return false;}
  if(!Guard(g,now))return false;
  if(!codec_.FlushOutput(g,flush)){Stop(g,now);return false;}
  if(!Guard(g,now)||!protocol_.AckPlaybackFlush(g,flush)){Stop(g,now);return false;}return true;
 }
 bool Talking(uint64_t g,uint64_t ticket,bool held,int64_t now){
  if(!Guard(g,now,true,held)||!ticket)return false;
  if(held){
   if(!DirectionGuard(g,now,true))return false;
   if(ticket<=last_ticket_||!permit_.Hold(g,ticket,now_)||!protocol_.Hold(g,true,now_))return false;
   ticket_=last_ticket_=ticket;talking_=true;
   if(!Flush(g,now)){Stop(g,now);return false;}return true;
  }
  if(!talking_||ticket!=ticket_)return false;
  talking_=false;ticket_=0;Wipe();
  protocol_.Hold(g,false,now_);
  if(!CloseInput()){Stop(g,now);return false;}
  return Guard(g,now,true,false);
 }
 bool Capture(uint64_t g,int64_t now){
  if(!DirectionGuard(g,now,true)||!talking_||protocol_.Pending().kind!=voice_prep::Kind::None)return false;
  if(!Flush(g,now))return false;
  Observation observed;if(!Observe(observed)||observed.speaker_open){Stop(g,now);return false;}
  // Observe is an external callout: it can revoke grants/hold before returning.
  // Revalidate immediately before opening a previously closed direction.
  if(!DirectionGuard(g,now,true))return false;
  if(input_closed_){input_attempted_=true;input_closed_=false;if(!codec_.EnableMicrophone(g)){Stop(g,now);return false;}}
  if(!DirectionGuard(g,now,true))return false;
  size_t n=0;const auto result=codec_.ReadFrame(g,capture_.data(),capture_.size(),n);
  if(!DirectionGuard(g,now,true)||result==ReadResult::Failed||(result==ReadResult::Complete&&n!=capture_.size())){Wipe();Stop(g,now_);return false;}
  if(result==ReadResult::Pending){Wipe();return false;}
  const auto consumed=protocol_.Microphone(g,capture_.data(),n,now_);Wipe();return consumed==n;
 }
 bool Play(uint64_t g,int64_t now){
  if(!DirectionGuard(g,now,false)||talking_)return false;
  if(!Flush(g,now))return false;
  Observation observed;if(!Observe(observed)){Stop(g,now);return false;}
  if(!DirectionGuard(g,now,false))return false;
  if(observed.mic_open){input_attempted_=true;input_closed_=false;}
  if(!CloseInput()){Stop(g,now);return false;}
  if(!DirectionGuard(g,now,false))return false;
  const auto n=protocol_.ReadPcm(g,playback_.data(),playback_.size(),now_);
  if(!n){Wipe();return false;}
  if(!DirectionGuard(g,now,false))return false;
  if(output_closed_){output_attempted_=true;output_closed_=false;if(!codec_.EnableOutput(g)){Stop(g,now);return false;}}
  if(!DirectionGuard(g,now,false)){Wipe();return false;}
  const auto wrote=codec_.Write16k(g,playback_.data(),n);Wipe();
  if(wrote!=n||!DirectionGuard(g,now,false)){Stop(g,now);return false;}return true;
 }
 bool Stop(uint64_t g,int64_t now){
  if(!Same(g))return false;
  if(!leased_)return state_==State::Closed;
  state_=State::Retained;talking_=false;ticket_=0;Wipe();lease_.Stop(false);
  // Protocol cancellation is an intent only: this owner never sends it or
  // claims transport completion. Its separate owner retains that obligation.
  protocol_.Cancel(g,now<now_?now_:now);
  if(bound_){
   // A binding owns digital directions too (paired TX and never-opened codecs).
   // Retry both checked stops; backend receipts must make them idempotent.
   input_attempted_=output_attempted_=true;input_closed_=output_closed_=false;
   const bool input=CloseInput();const bool output=CloseOutput();
   if(!input||!output)return false;
   Observation observed;if(!Observe(observed)||observed.mic_open||observed.speaker_open)return false;
   if(!codec_.Join(g)||!codec_.Release(g))return false;
   bound_=false;
  }
  if(!lease_.Stop(true))return false;
  leased_=false;state_=State::Closed;return true;
 }
 bool Poll(uint64_t g,int64_t now){return Guard(g,now);}
 Snapshot View(uint64_t g,int64_t now){
  if(!Same(g))return {state_,Direction::Unknown,Direction::Unknown,leased_};
  if(state_==State::Prepared)Guard(g,now,false);else if(state_==State::Ready)Guard(g,now);
  if(state_==State::Retained)return {state_,Direction::Unknown,Direction::Unknown,true};
  if(state_!=State::Ready)return {state_,Direction::Off,Direction::Off,false};
  Observation observed;const bool known=Observe(observed);
  if(!Guard(g,now))return {state_,state_==State::Retained?Direction::Unknown:Direction::Off,state_==State::Retained?Direction::Unknown:Direction::Off,state_==State::Retained};
  if(!known)return {state_,Direction::Unknown,Direction::Unknown,false};
  // Enabled direction is not evidence of actual capture or playback.
  const auto mic=selected_.microphone&&observed.mic_open&&observed.capturing&&talking_&&permit_.Hold(g,ticket_,now_)?Direction::Current:observed.mic_open?Direction::Unknown:Direction::Off;
  const auto output=selected_.output&&observed.speaker_open&&observed.playing?Direction::Current:observed.speaker_open?Direction::Unknown:Direction::Off;
  return {state_,mic,output,false};
 }
};
}
