// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include "muse_voice_session_owner.hpp"
#include "muse_voice_device_ui.hpp"
#include <optional>
namespace companion::muse {
// Explicit UI lock boundary; caller retains this port through cleanup. Native
// callbacks only queue private actions. This does not attest protocol ownership.
// VoiceUiAccess is shared with the held-input source in muse_voice_hold_input.hpp.
// Stable, single-use owner. Caller retains borrowed ports/scope callbacks and
// LVGL parent through Close completion. One protocol owner calls methods; an
// explicit UI access port selects queued callbacks and scopes only widget work.
// Legacy null-access mode requires the caller-held LVGL lock. No task/route.
template<class Admission,class Reader,class Device>
class DeviceVoiceRuntime final {
 using Session=VoiceSessionOwner<Admission>;
 using Consent=DeviceVoiceConsent<Admission,Reader,Device>;
 using Ui=DeviceVoiceReviewUi<Session,Consent>;
 voice_prep::Protocol& protocol_;Reader& reader_;Device& device_;VoiceUiAccess* ui_access_;
 voice_codec_prep::Codec&codec_;NativePointerHold hold_;Consent consent_;uint64_t generation_=0,talking_ticket_=0;
 std::unique_ptr<Session> session_;
 std::optional<Ui> ui_;
 lv_obj_t* parent_=nullptr;std::atomic<bool> stop_signal_{false};
 bool spent_=false,active_=false,busy_=false,closing_=false,closed_=false;
 bool ending_=false,ending_transport_=false,end_started_=false,ui_opened_=false,end_confirmed_=false;
 struct Busy {bool& flag;explicit Busy(bool& f):flag(f){flag=true;}~Busy(){flag=false;}};
 DeviceVoiceRuntime(voice_prep::Protocol&p,Admission&a,Reader&r,muse_native_review::Clock&k,Device&d,VoiceUiAccess*access,voice_codec_prep::Codec&codec):protocol_(p),reader_(r),device_(d),ui_access_(access),codec_(codec),hold_([&d]{return d.Input();},access),consent_(a,r,k,d,&hold_){}
 template<class Operation>bool WithUi(Operation operation){
  if(!ui_access_){operation();return true;} // legacy already-serialized inline owner
  if(!ui_access_->Enter())return false;
  struct Leave{VoiceUiAccess&port;~Leave(){port.Leave();}} leave{*ui_access_};operation();return true;
 }
 bool CloseUi(){bool done=false;return WithUi([&]{done=!ui_||ui_->Close();})&&done;}
 bool AdmissionAllowed()const{return spent_&&!closing_&&!closed_&&!stop_signal_.load()&&(!ending_||ending_transport_);}
 bool NativeCurrent()const{return AdmissionAllowed()&&(!consent_.RevocationRequested()||ending_transport_)&&reader_.Current()&&device_.Foreground()&&AdmissionAllowed();}
 void RequestClose(){talking_ticket_=0;closing_=true;ending_transport_=false;consent_.Revoke();}
 void RequestEnd(){talking_ticket_=0;ending_=true;consent_.Revoke();}
 bool Cleanup(){
  // A UI callback may still be on the stack even when runtime is not busy.
  // Close then returns false; do not retire its backing session in that case.
  if(!CloseUi())return false;
  ui_opened_=false;
  if(session_&&!session_->Close())return false;
  parent_=nullptr;active_=false;closed_=true;return true;
 }
 bool StartEnd(){
  if(!CloseUi())return false;
  ui_opened_=false;
  if(end_started_)return true;
  ending_transport_=true;
  if(!session_->End()){
   if(session_->CloseRequested())RequestClose();
   return false;
  }
  end_started_=true;return !closing_;
 }
 bool EndTick(){
  if(!StartEnd())return closing_?Cleanup():false;
  if(closing_)return Cleanup();
  const bool current=session_->Tick();
  end_confirmed_=end_confirmed_||session_->EndConfirmed();
  if(!current||closing_||end_confirmed_){RequestClose();return Cleanup();}
  return true;
 }
public:
 static std::unique_ptr<DeviceVoiceRuntime> Create(voice_prep::Protocol&protocol,std::unique_ptr<Admission>&candidate,Reader&reader,voice_codec_prep::Codec&codec,muse_native_review::Clock&clock,Device&device,VoiceUiAccess*access=nullptr){
  if(!candidate||!candidate->Unused()||!candidate->UsesProtocol(protocol)||!candidate->UsesClock(clock)||!candidate->UsesReader(reader))return {};
  auto result=std::unique_ptr<DeviceVoiceRuntime>(new(std::nothrow)DeviceVoiceRuntime(protocol,*candidate,reader,clock,device,access,codec));
  if(!result)return {};
  result->session_=Session::Create(protocol,candidate,codec,result->consent_,clock);
  if(!result->session_)return {};
  result->ui_.emplace(*result->session_,result->consent_,access?Ui::Dispatch::Queued:Ui::Dispatch::Inline);return result;
 }
 DeviceVoiceRuntime(const DeviceVoiceRuntime&)=delete;DeviceVoiceRuntime&operator=(const DeviceVoiceRuntime&)=delete;
 ~DeviceVoiceRuntime(){assert(!spent_||closed_);} // no substitute cleanup IO
 bool Open(lv_obj_t*parent,const VoiceAdmissionScope&scope){
  if(!STACKCHAN_MUSE_RAM_ONLY||busy_||spent_||closing_||closed_||!parent)return false;
  Busy busy(busy_);spent_=true;parent_=parent;generation_=scope.budget.generation;
  std::string node;bool geometry=false;
  const bool layout=WithUi([&]{lv_obj_update_layout(parent);geometry=lv_obj_get_width(parent)>=320&&lv_obj_get_height(parent)>=240;});
  const bool ready=layout&&geometry&&NativeCurrent()&&device_.Node(node)&&node==scope.node&&NativeCurrent();
  Wipe(node);if(!ready){RequestClose();return false;}
  auto captured=scope;
  const auto wrap=[this](std::function<bool()> original){return [this,original=std::move(original)]{return NativeCurrent()&&original&&original()&&NativeCurrent();};};
  if(scope.credential_current)captured.credential_current=wrap(scope.credential_current);
  if(scope.network_current)captured.network_current=wrap(scope.network_current);
  if(scope.voice_current)captured.voice_current=wrap(scope.voice_current);
  active_=session_->Open(captured)&&!closing_&&!ending_;
  if(!active_){RequestClose();return false;}return true;
 }
 // At most one existing SDK tick. Events only queue UI intents; consume End
 // before another transport tick, and prepare directions only after that tick.
 bool Poll(){
  if(busy_||closed_)return false;
  Busy busy(busy_);
  if(stop_signal_.load())RequestClose();
  if(closing_)return Cleanup();
  if(!active_)return false;
  bool end=false,prepare=false,ui_current=true;
  // Drain a native Close/End before another SDK operation. GUI callbacks only
  // signal atomic revocation; actual state changes happen on this owner.
  if(ui_opened_){
   if(!WithUi([&]{ui_current=ui_->Poll();end=ui_->TakeEndRequested();if(ui_current&&!end)prepare=ui_->TakePrepareRequested();})){RequestClose();return Cleanup();}
   if(end){RequestEnd();return EndTick();}
   if(!ui_current||closing_){RequestClose();return Cleanup();}
  }
  if(ending_){RequestEnd();return EndTick();}
  // Release a normal gesture before audio Poll would treat a lost Hold as a
  // revoked in-flight read. A release during IO instead retains safe cleanup.
  if(talking_ticket_&&!hold_.Current(generation_,talking_ticket_)){
   const auto original=talking_ticket_;talking_ticket_=0;
   if(!session_->Talking(original,false)){RequestClose();return Cleanup();}
  }
  if(!session_->Tick()||closing_||ending_){RequestClose();return Cleanup();}
  if(!ui_opened_&&protocol_.Status()==voice_prep::State::Live){
   bool opened=false;if(!WithUi([&]{opened=ui_->Open(parent_);})||!opened||closing_||ending_){RequestClose();return Cleanup();}
   ui_opened_=true;
  }
  if(prepare){
   if(!session_->PrepareDirections()){RequestClose();return Cleanup();}
   if(!WithUi([&]{ui_->PttReady(codec_.SupportsNativePtt());})){RequestClose();return Cleanup();}
  }
  if(codec_.SupportsNativePtt()&&session_->DirectionsBound()){
   const auto ticket=hold_.Ticket();
   if(ticket&&hold_.Current(generation_,ticket)){
    if(!talking_ticket_){if(session_->Talking(ticket,true))talking_ticket_=ticket;else hold_.CancelTicket(ticket);}
    if(talking_ticket_&&hold_.Current(generation_,talking_ticket_))(void)session_->Capture();
   }
  }
  if(closing_||ending_||session_->CloseRequested()){RequestClose();return Cleanup();}
  return true;
 }
 bool ShowReview(){
  if(busy_||!active_||closing_||ending_||!ui_opened_)return false;
  Busy busy(busy_);bool shown=false;const bool locked=WithUi([&]{shown=ui_->Show();});
  if(!locked||!shown||closing_||ending_)RequestClose();
  return shown&&!closing_&&!ending_;
 }
 // Explicit bounded playback request, never automatic microphone/hold/capture.
 // Empty PCM is not a failed session; native failures retain original cleanup.
 bool Play(){
  if(busy_||!DirectionsBound()||hold_.Ticket())return false;
  Busy busy(busy_);const bool played=session_->Play();
  if(closing_||ending_||session_->CloseRequested())RequestClose();
  return played&&!closing_&&!ending_;
 }
 bool End(){
  if(!spent_||closed_||closing_)return false;
  RequestEnd();if(busy_)return false;
  Busy busy(busy_);return StartEnd();
 }
 void SignalClose(){stop_signal_.store(true);consent_.SignalRevoke();} // other actor: signal only, never cleanup receipt
 bool Close(){RequestClose();if(busy_)return false;if(closed_)return true;Busy busy(busy_);return Cleanup();}
 bool CloseRequested()const{return closing_;}
 bool EndRequested()const{return ending_;}
 bool CleanupComplete()const{return closed_;}
 bool DirectionsBound()const{return active_&&!closing_&&!ending_&&session_->DirectionsBound();} // resource state only
 bool EndConfirmed()const{return end_confirmed_;} // only a validated SDK reply
};
}
#endif
