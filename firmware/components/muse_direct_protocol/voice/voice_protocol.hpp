// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "muse_voice_call_core.h"
#include <array>
#include <string_view>
#include <cstdint>
#include <cstring>
#include <climits>
#include <cstdio>
namespace voice_prep {
enum class State{Idle,Opening,Live,Ending,AwaitTransportClose,Done};
enum class Kind{None,OpenMedia,Append,OpenEndCall,HalfClose,ResetMedia,ResetEndCall};
// Correlation and budget only: this model grants no transport or audio authority.
struct Scope{uint64_t generation;std::string_view call_id,device_id;int64_t media_stream,end_stream,expires_us;};
// Data/correlation only. These values are not permission or verified identity.
struct TargetSnapshot{uint64_t generation=0;std::array<char,MUSE_VC_ID_MAX> admitted_device_id{};std::array<char,MUSE_VC_CALL_ID_MAX> call_id{};std::array<char,MUSE_VC_ID_MAX> session_id{},thread_id{};};
struct Operation{Kind kind=Kind::None;uint64_t generation=0,sequence=0;int64_t stream=0;size_t length=0;std::array<char,128> path{};std::array<char,4096> body{};};
class Protocol {
 Operation op_{};State state_=State::Idle;
 muse_vc_call_t call_{};muse_vc_turns_t turns_{};muse_vc_uplink_t up_{};
 muse_vc_resampler_t down_{};muse_vc_reply_gate_t reply_{};muse_vc_gate_t duplex_{};
 std::array<char,MUSE_VC_ID_MAX> admitted_device_{};
 std::array<char,4096> line_{};size_t line_size_=0;
 std::array<uint8_t,3840> decoded_{};std::array<int16_t,1282> pcm_{};size_t pcm_size_=0;
 uint64_t generation_=0,sequence_=0;int64_t media_=0,end_=0,expiry_=0,last_=0,wall_start_=0,start_=0;
 uint32_t actions_=0;bool spent_=false,held_=false,media_sent_=false,end_sent_=false,confirmed_=false,flush_pending_=false;uint64_t flush_sequence_=0;
 static void Zero(void* p,size_t n){auto v=static_cast<volatile unsigned char*>(p);while(n--)*v++=0;}
 static bool Id(std::string_view s,size_t cap){if(s.empty()||s.size()>=cap)return false;for(unsigned char c:s)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c==':'))return false;return true;}
 void ClearOperation(){Zero(&op_,sizeof(op_));}
 void ClearMedia(){Zero(admitted_device_.data(),admitted_device_.size());Zero(call_.session_id,sizeof(call_.session_id));Zero(line_.data(),line_.size());line_size_=0;Zero(decoded_.data(),decoded_.size());Zero(pcm_.data(),pcm_.size()*sizeof(int16_t));pcm_size_=0;Zero(&up_,sizeof(up_));Zero(&down_,sizeof(down_));Zero(&reply_,sizeof(reply_));Zero(&duplex_,sizeof(duplex_));held_=flush_pending_=false;}
 void DiscardMic(){const auto sent=up_.sent;Zero(&up_,sizeof(up_));muse_vc_uplink_init(&up_);up_.sent=sent;if(op_.kind==Kind::Append)ClearOperation();}
 void RequestFlush(){Zero(pcm_.data(),pcm_.size()*sizeof(int16_t));pcm_size_=0;if(flush_sequence_==UINT64_MAX){RequireTransportClose();return;}++flush_sequence_;flush_pending_=true;muse_vc_gate_flush(&duplex_,last_);}
 void RequireTransportClose(){ClearOperation();ClearMedia();Zero(&turns_,sizeof(turns_));Zero(&call_,sizeof(call_));actions_=0;state_=State::AwaitTransportClose;}
 bool Identity(uint64_t g)const{return g&&g==generation_&&state_!=State::Idle&&state_!=State::Done&&state_!=State::AwaitTransportClose;}
 bool Advance(uint64_t g,int64_t now){
  if(!Identity(g))return false;
  if(now<last_||now<0||now-start_>240000000){RequireTransportClose();return false;}
  last_=now;
  if(now>=expiry_&&state_!=State::Ending)Stop(MUSE_VC_END_MAX,now);
  return state_!=State::AwaitTransportClose;
 }
 void Emit(Kind kind,int64_t stream,const char* body=nullptr,size_t n=0){
  if(sequence_==UINT64_MAX||n>=op_.body.size()){RequireTransportClose();return;}
  ClearOperation();op_.kind=kind;op_.stream=stream;op_.generation=generation_;op_.sequence=++sequence_;op_.length=n;
  if(n)std::memcpy(op_.body.data(),body,n);
 }
 void Pump(){
  if(op_.kind!=Kind::None||state_==State::AwaitTransportClose||state_==State::Done)return;
  // The SDK repeats END_CALL until the queued request is acknowledged sent.
  if(end_sent_)actions_&=~MUSE_VC_ACT_END_CALL;
  if(actions_&MUSE_VC_ACT_RESET_MEDIA){actions_&=~MUSE_VC_ACT_RESET_MEDIA;Emit(Kind::ResetMedia,media_);return;}
  if(actions_&MUSE_VC_ACT_RESET_END_CALL){RequireTransportClose();return;}
  if(actions_&MUSE_VC_ACT_END_CALL){
   actions_&=~MUSE_VC_ACT_END_CALL;std::array<char,MUSE_VC_END_CALL_MAX> body{};
   const auto wall=wall_start_+(last_-start_)/1000;
   if(!muse_vc_end_call_body(&call_,wall,last_,body.data(),body.size())){RequireTransportClose();return;}
   Emit(Kind::OpenEndCall,end_,body.data(),std::strlen(body.data()));std::snprintf(op_.path.data(),op_.path.size(),"/voice/end-call");Zero(body.data(),body.size());return;
  }
  if(actions_&MUSE_VC_ACT_HALF_CLOSE){actions_&=~MUSE_VC_ACT_HALF_CLOSE;Emit(Kind::HalfClose,media_);return;}
  if(actions_&MUSE_VC_ACT_ENDED)RequireTransportClose();
 }
 void Act(uint32_t actions){
  actions_|=actions;
  if(actions&MUSE_VC_ACT_READY)state_=State::Live;
  if(call_.state==MUSE_VC_ENDING){state_=State::Ending;if(op_.kind==Kind::Append)ClearOperation();ClearMedia();Zero(&turns_,sizeof(turns_));}
  if(actions&MUSE_VC_ACT_FLUSH)RequestFlush();
  if((actions&MUSE_VC_ACT_HOLD)&&call_.held)DiscardMic();
  Pump();
 }
 void Stop(muse_vc_end_t reason,int64_t now){
  ClearOperation();ClearMedia();Zero(&turns_,sizeof(turns_));
  if(!media_sent_){RequireTransportClose();return;}
  Act(muse_vc_call_hang_up(&call_,reason,now));state_=State::Ending;Pump();
 }
 bool Line(int64_t now){
  muse_vc_event_t ev{};
  if(!muse_vc_parse_event(line_.data(),line_size_,&ev)){Stop(MUSE_VC_END_FAILED,now);return false;}
  Act(muse_vc_call_event(&call_,&ev,now));
  // A parsed stop/error may validly enter Ending and queue graceful cleanup.
  // Opening advisory events are also valid input, not evidence of readiness.
  if(state_!=State::Live){Zero(&ev,sizeof(ev));return state_==State::Opening||state_==State::Ending;}
  muse_vc_reply_gate_event(&reply_,&ev);
  if(muse_vc_reply_gate_take_flush(&reply_))RequestFlush();
  if(muse_vc_reply_gate_full(&reply_)){Stop(MUSE_VC_END_FAILED,now);return false;}
  if(ev.kind==MUSE_VC_EV_AUDIO){
   if(!ev.audio_rate_ok){Stop(MUSE_VC_END_FAILED,now);return false;}
   const auto verdict=muse_vc_reply_gate_audio(&reply_,&ev);
   // Audio itself can replace an older reply and request downstream flushing.
   if(muse_vc_reply_gate_take_flush(&reply_))RequestFlush();
   if(verdict==MUSE_VC_GATE_PLAY){
    // No hidden playback queue: consumer must drain this single bounded chunk.
    if(pcm_size_){Stop(MUSE_VC_END_FAILED,now);return false;}
    bool audible=false;const auto n=muse_vc_decode_audio(&down_,ev.audio,decoded_.data(),decoded_.size(),pcm_.data(),&audible);
    Zero(decoded_.data(),decoded_.size());
    if(n<0){Stop(MUSE_VC_END_FAILED,now);return false;}pcm_size_=static_cast<size_t>(n);
   }
   // HOLD has no deferred-audio implementation in this preparation: drop it.
  }
  Zero(&ev,sizeof(ev));return true;
 }
public:
 static constexpr size_t LineCapacity=4096;
 Protocol()=default;Protocol(const Protocol&)=delete;Protocol& operator=(const Protocol&)=delete;
 ~Protocol(){Zero(this,sizeof(*this));} // All members are fixed-size, trivial storage.
 State Status()const{return state_;}const Operation& Pending()const{return op_;}
 bool Begin(Scope s,int64_t now,int64_t wall_ms){
  if(spent_||!s.generation||!Id(s.call_id,MUSE_VC_CALL_ID_MAX)||!Id(s.device_id,MUSE_VC_ID_MAX)||s.media_stream<=0||s.end_stream<=0||s.media_stream==s.end_stream||!(s.media_stream&1)||!(s.end_stream&1)||now<0||now>INT64_MAX-240000000||s.expires_us<=now||s.expires_us-now>120000000||wall_ms<1704067200000LL||wall_ms>INT64_MAX-240000)return false;
  generation_=s.generation;media_=s.media_stream;end_=s.end_stream;expiry_=s.expires_us;last_=start_=now;wall_start_=wall_ms;spent_=true;state_=State::Opening;
  muse_vc_limits_t limits{5000000,5000000,30000000,120000000,5000000,2000000};
  muse_vc_call_init(&call_,&limits,&turns_);std::array<char,MUSE_VC_CALL_ID_MAX> id{};std::memcpy(id.data(),s.call_id.data(),s.call_id.size());
  std::array<char,MUSE_VC_ID_MAX> device{};std::memcpy(device.data(),s.device_id.data(),s.device_id.size());
  std::array<char,MUSE_VC_ADMISSION_MAX> admission{};
  bool okay=muse_vc_admission(id.data(),device.data(),false,admission.data(),admission.size());
  if(okay){std::memcpy(admitted_device_.data(),device.data(),device.size());muse_vc_call_begin(&call_,id.data(),now);Emit(Kind::OpenMedia,media_,admission.data(),std::strlen(admission.data()));std::snprintf(op_.path.data(),op_.path.size(),"/voice/realtime?call_id=%s",id.data());}
  Zero(admission.data(),admission.size());Zero(id.data(),id.size());Zero(device.data(),device.size());
  muse_vc_uplink_init(&up_);muse_vc_resampler_init(&down_,24000,16000);muse_vc_reply_gate_init(&reply_);muse_vc_gate_init(&duplex_,false,150000);
  if(!okay){RequireTransportClose();}
  return okay;
 }
 bool Sent(uint64_t g,uint64_t sequence,bool success,int64_t now){
  if(!Identity(g)||op_.kind==Kind::None||op_.sequence!=sequence)return false;
  if(!Advance(g,now)||op_.sequence!=sequence)return false;
  if(!success){RequireTransportClose();return false;}
  const auto kind=op_.kind;ClearOperation();
  if(kind==Kind::OpenMedia)media_sent_=true;
  if(kind==Kind::OpenEndCall){end_sent_=true;muse_vc_call_end_call_sent(&call_,now);}
  Pump();return true;
 }
 bool Head(uint64_t g,int64_t stream,int status,int64_t now){if(!Identity(g)||stream!=media_||!media_sent_||!Advance(g,now)||state_!=State::Opening)return false;Act(muse_vc_call_head(&call_,status,now));return call_.state==MUSE_VC_CONNECTING;}
 bool Media(uint64_t g,int64_t stream,const char* data,size_t n,int64_t now){
  if(!Identity(g)||stream!=media_||!media_sent_||!Advance(g,now)||(state_!=State::Opening&&state_!=State::Live)||(!data&&n))return false;
  if(n>LineCapacity){Stop(MUSE_VC_END_FAILED,now);return false;}
  for(size_t i=0;i<n;++i){
   if(data[i]=='\n'){if(line_size_&&!Line(now)){Zero(line_.data(),line_.size());line_size_=0;return false;}Zero(line_.data(),line_.size());line_size_=0;if(state_==State::Ending)return true;}
   else{if(data[i]=='\0'||line_size_>=line_.size()-1){Stop(MUSE_VC_END_FAILED,now);return false;}line_[line_size_++]=data[i];}
  }return true;
 }
 bool Hold(uint64_t g,bool held,int64_t now){if(!Advance(g,now)||state_!=State::Live||call_.held)return false;held_=held;DiscardMic();if(held){muse_vc_reply_gate_press(&reply_);RequestFlush();}return true;}
 size_t Microphone(uint64_t g,const int16_t* pcm,size_t n,int64_t now){
  if(!Advance(g,now)||state_!=State::Live||!held_||call_.held||flush_pending_||!pcm||!n||n>640||op_.kind!=Kind::None||!muse_vc_gate_mic_open(&duplex_,now)||!muse_vc_uplink_may_send(&up_,now,call_.live_us))return 0;
  std::array<char,MUSE_VC_APPEND_MAX> line{};size_t bytes=0;const auto consumed=muse_vc_uplink_push(&up_,pcm,n,line.data(),&bytes);
  if(bytes){Emit(Kind::Append,media_,line.data(),bytes);}
  Zero(line.data(),line.size());
  return consumed;
 }
 size_t ReadPcm(uint64_t g,int16_t* out,size_t cap,int64_t now){
  if(!out||!cap||cap>pcm_.size()){return 0;}
  Zero(out,cap*sizeof(int16_t));
  if(!Advance(g,now)||state_!=State::Live||flush_pending_||!pcm_size_||cap<pcm_size_)return 0;
  const auto n=pcm_size_;std::memcpy(out,pcm_.data(),n*sizeof(int16_t));muse_vc_gate_playout(&duplex_,out,n,last_,0);Zero(pcm_.data(),pcm_.size()*sizeof(int16_t));pcm_size_=0;return n;
 }
 bool Cancel(uint64_t g,int64_t now){if(!Advance(g,now))return false;if(state_!=State::Ending)Stop(MUSE_VC_END_LOCAL,now);return true;}
 bool EndReply(uint64_t g,int64_t stream,int status,const char* body,size_t n,int64_t now){
  if(!Identity(g)||stream!=end_||!end_sent_||!Advance(g,now)||state_!=State::Ending)return false;
  muse_vc_span_t v{};bool okay=body&&n&&n<=512&&status>=200&&status<300&&muse_vc_json_valid(body,n);
  if(okay){const auto outer=muse_vc_json_get(body,n,"ok",&v),inner=muse_vc_json_get(body,n,"result.ok",&v);okay=(outer==MUSE_VC_J_TRUE||inner==MUSE_VC_J_TRUE)&&outer!=MUSE_VC_J_FALSE&&inner!=MUSE_VC_J_FALSE;}
  if(!okay){RequireTransportClose();return false;}confirmed_=true;end_sent_=false;Act(muse_vc_call_end_call_done(&call_,now));return true;
 }
 bool EndConfirmed()const{return confirmed_;}
 bool MediaEnded(uint64_t g,int64_t stream,bool okay,int status,int64_t now){if(!Identity(g)||stream!=media_||!Advance(g,now))return false;Act(muse_vc_call_media_end(&call_,okay,status,now));return true;}
 // A failed/uncertain transport cannot be retried or claim end-call success.
 bool TransportFailed(uint64_t g){if(!g||g!=generation_||state_==State::Idle||state_==State::Done)return false;RequireTransportClose();return true;}
 bool TransportClosed(uint64_t g,bool joined){if(!g||g!=generation_||state_!=State::AwaitTransportClose)return false;if(!joined)return false;ClearOperation();ClearMedia();Zero(&call_,sizeof(call_));Zero(&turns_,sizeof(turns_));state_=State::Done;return true;}
 bool Poll(uint64_t g,int64_t now){if(!Advance(g,now))return false;Act(muse_vc_call_poll(&call_,now));return true;}
 unsigned ActiveTurns()const{return turns_.n_active;}
 uint64_t FlushTicket()const{return flush_pending_?flush_sequence_:0;}
 bool AckPlaybackFlush(uint64_t g,uint64_t ticket){if(!Identity(g)||state_!=State::Live||!flush_pending_||!ticket||ticket!=flush_sequence_)return false;flush_pending_=false;return true;}
 bool ReturnedTarget(uint64_t g,int64_t now,TargetSnapshot& out)const{
  Zero(&out,sizeof(out));
  // Projection only: an invalid/stale query must never drive current cleanup,
  // advance clocks, emit operations, grant permission or alter another call.
  if(!Identity(g)||state_!=State::Live||call_.state!=MUSE_VC_LIVE||now<last_||now<0||now>=expiry_||!admitted_device_[0]||!call_.call_id[0]||!call_.session_id[0])return false;
  out.generation=g;
  std::memcpy(out.admitted_device_id.data(),admitted_device_.data(),admitted_device_.size());
  std::memcpy(out.call_id.data(),call_.call_id,sizeof(call_.call_id));
  std::memcpy(out.session_id.data(),call_.session_id,sizeof(call_.session_id));
  std::memcpy(out.thread_id.data(),call_.thread_id,sizeof(call_.thread_id));
  return true;
 }
};
}
