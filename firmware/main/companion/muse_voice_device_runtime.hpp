// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include "muse_voice_session_owner.hpp"
#include "muse_voice_device_ui.hpp"
#include <optional>
namespace companion::muse {
// Stable, single-use owner. Caller retains borrowed ports/scope callbacks and
// LVGL parent through Close completion; every call uses the same serialized
// LVGL owner lock. No task, timer, launcher or authority is registered here.
template<class Admission,class Reader,class Device>
class DeviceVoiceRuntime final {
 using Session=VoiceSessionOwner<Admission>;
 using Consent=DeviceVoiceConsent<Admission,Reader,Device>;
 using Ui=DeviceVoiceReviewUi<Session,Consent>;
 voice_prep::Protocol& protocol_;Reader& reader_;Device& device_;
 Consent consent_;
 std::unique_ptr<Session> session_;
 std::optional<Ui> ui_;
 lv_obj_t* parent_=nullptr;
 bool spent_=false,active_=false,busy_=false,closing_=false,closed_=false;
 bool ending_=false,ending_transport_=false,end_started_=false,ui_opened_=false,end_confirmed_=false;
 struct Busy {bool& flag;explicit Busy(bool& f):flag(f){flag=true;}~Busy(){flag=false;}};
 DeviceVoiceRuntime(voice_prep::Protocol&p,Admission&a,Reader&r,muse_native_review::Clock&k,Device&d):protocol_(p),reader_(r),device_(d),consent_(a,r,k,d){}
 bool AdmissionAllowed()const{return spent_&&!closing_&&!closed_&&(!ending_||ending_transport_);}
 bool NativeCurrent()const{return AdmissionAllowed()&&reader_.Current()&&device_.Foreground()&&AdmissionAllowed();}
 void RequestClose(){closing_=true;ending_transport_=false;consent_.Revoke();}
 void RequestEnd(){ending_=true;consent_.Revoke();}
 bool Cleanup(){
  // A UI callback may still be on the stack even when runtime is not busy.
  // Close then returns false; do not retire its backing session in that case.
  if(ui_&&!ui_->Close())return false;
  ui_opened_=false;
  if(session_&&!session_->Close())return false;
  parent_=nullptr;active_=false;closed_=true;return true;
 }
 bool StartEnd(){
  if(!ui_->Close())return false;
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
 static std::unique_ptr<DeviceVoiceRuntime> Create(voice_prep::Protocol&protocol,std::unique_ptr<Admission>&candidate,Reader&reader,voice_codec_prep::Codec&codec,muse_native_review::Clock&clock,Device&device){
  if(!candidate||!candidate->Unused()||!candidate->UsesProtocol(protocol)||!candidate->UsesClock(clock)||!candidate->UsesReader(reader))return {};
  auto result=std::unique_ptr<DeviceVoiceRuntime>(new(std::nothrow)DeviceVoiceRuntime(protocol,*candidate,reader,clock,device));
  if(!result)return {};
  result->session_=Session::Create(protocol,candidate,codec,result->consent_,clock);
  if(!result->session_)return {};
  result->ui_.emplace(*result->session_,result->consent_);return result;
 }
 DeviceVoiceRuntime(const DeviceVoiceRuntime&)=delete;DeviceVoiceRuntime&operator=(const DeviceVoiceRuntime&)=delete;
 ~DeviceVoiceRuntime(){assert(!spent_||closed_);} // no substitute cleanup IO
 bool Open(lv_obj_t*parent,const VoiceAdmissionScope&scope){
  if(!STACKCHAN_MUSE_RAM_ONLY||busy_||spent_||closing_||closed_||!parent)return false;
  Busy busy(busy_);spent_=true;parent_=parent;
  std::string node;lv_obj_update_layout(parent);
  const bool ready=lv_obj_get_width(parent)>=320&&lv_obj_get_height(parent)>=240&&NativeCurrent()&&device_.Node(node)&&node==scope.node&&NativeCurrent();
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
  if(closing_)return Cleanup();
  if(!active_)return false;
  if(ending_||ui_->TakeEndRequested()){RequestEnd();return EndTick();}
  if(!session_->Tick()||closing_||ending_){RequestClose();return Cleanup();}
  if(!ui_opened_&&protocol_.Status()==voice_prep::State::Live){
   if(!ui_->Open(parent_)||closing_||ending_){RequestClose();return Cleanup();}
   ui_opened_=true;
  }
  if(ui_opened_){
   if(!ui_->Poll()||closing_||ending_){RequestClose();return Cleanup();}
   const bool prepare=ui_->TakePrepareRequested();
   if(closing_||ending_){RequestClose();return Cleanup();}
   if(prepare&&!session_->PrepareDirections()){RequestClose();return Cleanup();}
  }
  if(closing_||ending_||session_->CloseRequested()){RequestClose();return Cleanup();}
  return true;
 }
 bool ShowReview(){
  if(busy_||!active_||closing_||ending_||!ui_opened_)return false;
  Busy busy(busy_);const bool shown=ui_->Show();
  if(!shown||closing_||ending_)RequestClose();
  return shown&&!closing_&&!ending_;
 }
 // Explicit bounded playback request, never automatic microphone/hold/capture.
 // Empty PCM is not a failed session; native failures retain original cleanup.
 bool Play(){
  if(busy_||!DirectionsBound())return false;
  Busy busy(busy_);const bool played=session_->Play();
  if(closing_||ending_||session_->CloseRequested())RequestClose();
  return played&&!closing_&&!ending_;
 }
 bool End(){
  if(!spent_||closed_||closing_)return false;
  RequestEnd();if(busy_)return false;
  Busy busy(busy_);return StartEnd();
 }
 bool Close(){RequestClose();if(busy_)return false;if(closed_)return true;Busy busy(busy_);return Cleanup();}
 bool CloseRequested()const{return closing_;}
 bool EndRequested()const{return ending_;}
 bool CleanupComplete()const{return closed_;}
 bool DirectionsBound()const{return active_&&!closing_&&!ending_&&session_->DirectionsBound();} // resource state only
 bool EndConfirmed()const{return end_confirmed_;} // only a validated SDK reply
};
}
#endif
