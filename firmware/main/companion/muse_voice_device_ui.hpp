// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include "muse_voice_device_consent.hpp"
#include <lvgl.h>
#include <array>
namespace companion::muse {
// All UI operations/callbacks hold the same LVGL lock. Inline mode is also
// serialized with the session; queued callbacks touch no protocol/model state.
// Close must finish before destruction; callbacks defer deletion to safe Poll.
template<class Session,class Consent>
class DeviceVoiceReviewUi final {
public:
 enum class Dispatch{Inline,Queued};
private:
 Session& session_;Consent& consent_;typename Session::ReviewBorrow borrow_;Dispatch dispatch_;
 lv_obj_t* root_=nullptr;lv_obj_t* title_=nullptr;lv_obj_t* value_=nullptr;lv_obj_t* status_=nullptr;
 std::array<lv_obj_t*,4> buttons_{},labels_{};
 struct Binding{DeviceVoiceReviewUi* owner=nullptr;unsigned action=0;uint64_t ui=0,model=0;int64_t time=0;lv_indev_t* input=nullptr;bool armed=false;uint32_t tick=0;};
 std::array<Binding,4> binding_{};uint64_t revision_=0;unsigned depth_=0,vm_page_=0;
 bool close_requested_=false,prepare_requested_=false,end_requested_=false;
 struct QueuedAction{unsigned action=0;uint64_t ui=0,model=0;uint32_t tick=0;lv_indev_t* input=nullptr;bool end=false;};
 std::array<QueuedAction,4> pending_{};unsigned pending_count_=0;bool overflow_=false,end_button_snapshot_=false;uint64_t rendered_model_revision_=0;
 void ClearQueued(){muse_review::Wipe(pending_.data(),sizeof pending_);pending_count_=0;}
 void QueueEvent(lv_event_t*e,Binding&b){
  const auto code=lv_event_get_code(e);
  if(code==LV_EVENT_PRESS_LOST){b.armed=false;return;}
  if(close_requested_||!root_)return;
  if(code==LV_EVENT_PRESSED){b.armed=Native(e,b,true);if(b.armed){b.ui=revision_;b.model=rendered_model_revision_;b.input=consent_.device_.Input();b.tick=lv_tick_get();}return;}
  if(code!=LV_EVENT_CLICKED)return;
  const bool valid=b.armed&&Native(e,b,false)&&b.input==consent_.device_.Input()&&b.ui==revision_&&b.model==rendered_model_revision_&&uint32_t(lv_tick_get()-b.tick)<=10000;
  b.armed=false;if(!valid)return;
  const bool end=b.action==3||(b.action==1&&end_button_snapshot_);
  if(end)consent_.SignalRevoke();
  if(pending_count_==pending_.size()){overflow_=true;consent_.SignalRevoke();return;}
  pending_[pending_count_++]={b.action,b.ui,b.model,b.tick,b.input,end};
 }
 void DrainQueued(){
  if(overflow_){RequestClose();return;}
  if(!pending_count_)return;
  const auto action=pending_[0];for(unsigned i=1;i<pending_count_;++i)pending_[i-1]=pending_[i];pending_[--pending_count_]={};
  auto*model=Model();
  if(action.ui!=revision_||!model||action.model!=model->Revision()||action.input!=consent_.device_.Input()||uint32_t(lv_tick_get()-action.tick)>10000)return;
  if(action.end){end_requested_=true;RequestClose();return;}
  if(!consent_.BoundCurrent()||close_requested_||!borrow_.Get()||model->Revision()!=action.model){RequestClose();return;}
  Act(action.action);
 }
 struct Depth{unsigned& n;explicit Depth(unsigned&v):n(v){++n;}~Depth(){--n;}};
 muse_review::Model* Model(){auto* b=borrow_.Get();return b?&b->Review():nullptr;}
 void Disarm(){for(auto&b:binding_)b.armed=false;}
 bool Native(lv_event_t* e,Binding&b,bool pressed){auto* expected=consent_.device_.Input();return expected&&lv_event_get_indev(e)==expected&&lv_indev_active()==expected&&lv_indev_get_type(expected)==LV_INDEV_TYPE_POINTER&&lv_indev_get_state(expected)==(pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED)&&lv_indev_get_active_obj()==buttons_[b.action]&&lv_event_get_target_obj(e)==buttons_[b.action]&&lv_obj_is_visible(buttons_[b.action]);}
 static void Event(lv_event_t*e){
  auto&b=*static_cast<Binding*>(lv_event_get_user_data(e));auto&u=*b.owner;Depth depth(u.depth_);const auto code=lv_event_get_code(e);
  if(u.dispatch_==Dispatch::Queued){u.QueueEvent(e,b);return;}
  if(code==LV_EVENT_PRESS_LOST){b.armed=false;return;}
  if(u.close_requested_||!u.root_)return;
  auto* model=u.Model();if(!model||!u.consent_.BoundCurrent()){u.RequestClose();return;}
  if(code==LV_EVENT_PRESSED){b.armed=u.Native(e,b,true);if(b.armed){b.ui=u.revision_;b.model=model->Revision();b.time=u.consent_.last_.mono_us;b.input=u.consent_.device_.Input();}return;}
  if(code!=LV_EVENT_CLICKED)return;
  const bool allowed=b.armed&&u.Native(e,b,false)&&b.input==u.consent_.device_.Input()&&b.ui==u.revision_&&b.model==model->Revision()&&u.consent_.last_.mono_us>=b.time&&u.consent_.last_.mono_us-b.time<=10000000;
  b.armed=false;if(allowed)u.Act(b.action);
 }
 void RequestClose(){close_requested_=true;prepare_requested_=false;consent_.Revoke();Disarm();ClearQueued();}
 lv_obj_t* Label(int y,int h){auto*l=lv_label_create(root_);lv_obj_set_pos(l,10,y);lv_obj_set_size(l,300,h);lv_label_set_long_mode(l,LV_LABEL_LONG_WRAP);lv_obj_set_style_text_font(l,&lv_font_montserrat_14,0);return l;}
 void Show(unsigned i,const char*name){lv_label_set_text(labels_[i],name);lv_obj_remove_flag(buttons_[i],LV_OBJ_FLAG_HIDDEN);}
 void Render(){
  Depth depth(depth_);if(!root_||close_requested_)return;
  if(revision_==UINT64_MAX){RequestClose();return;}
  ++revision_;Disarm();end_button_snapshot_=false;rendered_model_revision_=0;for(auto*b:buttons_)lv_obj_add_flag(b,LV_OBJ_FLAG_HIDDEN);Show(3,"Close");auto* model=Model();
  if(!model||!model->Active()){lv_label_set_text(title_,"Voice review unavailable");lv_label_set_text(value_,"");lv_label_set_text(status_,"Audio remains off");return;}
  rendered_model_revision_=model->Revision();
  const auto& vm=consent_.context_.vm;const unsigned pages=unsigned((vm.size()+17)/18);
  if(vm_page_<pages){char heading[48]{};std::snprintf(heading,sizeof heading,"VM (%u/%u)",vm_page_+1,pages);lv_label_set_text(title_,heading);const auto part=vm.substr(vm_page_*18,18);lv_label_set_text(value_,part.c_str());lv_label_set_text(status_,"Uses the enrolled RAM credential");Show(0,"Next");return;}
  if(!model->Reviewed()){char part[25]{},heading[64]{};unsigned field=0,page=0,count=0;model->PageText(part,field,page,count);const char*names[]={"Device","Call","Session","Thread","Generation","Expiry"};std::snprintf(heading,sizeof heading,"%s (%u/%u)",names[field],page,count);lv_label_set_text(title_,heading);lv_label_set_text(value_,part);lv_label_set_text(status_,"Read this full page before Next");Show(0,"Next");return;}
  auto receipt=model->Receipt();if(close_requested_)return;
  if(!receipt.valid){lv_label_set_text(title_,"Choose audio permissions");lv_label_set_text(value_,"Microphone and speaker are separate");lv_label_set_text(status_,"Both remain off until you approve");Show(0,chosen_mic_?"Mic: yes":"Mic: no");Show(1,chosen_output_?"Out: yes":"Out: no");Show(2,"Review");}
  else if(!receipt.microphone&&!receipt.speaker){lv_label_set_text(title_,"No audio selected");lv_label_set_text(value_,"");lv_label_set_text(status_,"Microphone off / speaker off");}
  else if(!consent_.granted_){lv_label_set_text(title_,"Allow selected audio?");lv_label_set_text(value_,receipt.microphone?(receipt.speaker?"Microphone + speaker":"Microphone only"):"Speaker only");lv_label_set_text(status_,"Only for this reviewed device session");Show(0,"Allow");}
  else{end_button_snapshot_=true;lv_label_set_text(title_,"Local permissions approved");lv_label_set_text(value_,"Audio readiness is not verified");lv_label_set_text(status_,"Push-to-talk is not available yet");Show(0,"Return");Show(1,"End session");}
  muse_review::Wipe(&receipt,sizeof receipt);
 }
 bool chosen_mic_=false,chosen_output_=false;
 void Act(unsigned action){
  if(action==3){end_requested_=true;RequestClose();return;}
  auto* model=Model();if(!model)return;
  const auto revision=model->Revision();const unsigned pages=unsigned((consent_.context_.vm.size()+17)/18);
  if(vm_page_<pages){if(action==0)++vm_page_;Render();return;}
  if(!model->Reviewed()){if(action==0)model->Acknowledge(revision,model->Page());Render();return;}
  auto receipt=model->Receipt();if(close_requested_||!borrow_.Get()||model->Revision()!=revision){muse_review::Wipe(&receipt,sizeof receipt);return;}
  if(!receipt.valid){bool mic=chosen_mic_,output=chosen_output_;if(action==0)mic=!mic;else if(action==1)output=!output;
   if(action<2){if(model->OptIn(revision,mic,output)){chosen_mic_=mic;chosen_output_=output;}}else if(action==2)model->Finish(revision);
  }else if(!consent_.granted_&&action==0){const bool granted=consent_.Allow(receipt,*model,revision);if(granted&&!close_requested_&&borrow_.Get()&&model->Revision()==revision)prepare_requested_=true;else consent_.Revoke();}
  else if(consent_.granted_&&action==0){lv_obj_add_flag(root_,LV_OBJ_FLAG_HIDDEN);Disarm();}
  else if(consent_.granted_&&action==1){end_requested_=true;RequestClose();}
  muse_review::Wipe(&receipt,sizeof receipt);Render();
 }
public:
 DeviceVoiceReviewUi(Session&s,Consent&c,Dispatch dispatch=Dispatch::Inline):session_(s),consent_(c),dispatch_(dispatch){}
 DeviceVoiceReviewUi(const DeviceVoiceReviewUi&)=delete;DeviceVoiceReviewUi&operator=(const DeviceVoiceReviewUi&)=delete;
 ~DeviceVoiceReviewUi(){assert(!root_&&!depth_);}
 bool Open(lv_obj_t*parent){
  if(root_||depth_||close_requested_||!parent)return false;
  if(!session_.UsesAdmission(consent_.admission_)||!session_.UsesNativeAuthority(consent_)||!session_.UsesClock(consent_.clock_))return false;
  lv_obj_update_layout(parent);if(lv_obj_get_width(parent)<320||lv_obj_get_height(parent)<240)return false;
  auto ready=session_.BorrowReview();if(!ready)return false; // no target yet is not revocation
  bool ok=false;{
   Depth depth(depth_);borrow_=std::move(ready);
   if(borrow_&&consent_.Bind()&&!close_requested_){root_=lv_obj_create(parent);lv_obj_set_pos(root_,0,0);lv_obj_set_size(root_,320,240);lv_obj_set_style_pad_all(root_,0,0);lv_obj_set_style_border_width(root_,0,0);lv_obj_remove_flag(root_,LV_OBJ_FLAG_SCROLLABLE);lv_obj_update_layout(root_);
    title_=Label(8,30);value_=Label(42,40);status_=Label(86,42);
    for(unsigned i=0;i<4;++i){buttons_[i]=lv_button_create(root_);lv_obj_set_pos(buttons_[i],i==3?112:8+104*int(i),i==3?190:136);lv_obj_set_size(buttons_[i],96,40);labels_[i]=lv_label_create(buttons_[i]);lv_obj_set_width(labels_[i],88);lv_label_set_long_mode(labels_[i],LV_LABEL_LONG_WRAP);lv_obj_set_style_text_font(labels_[i],&lv_font_montserrat_14,0);lv_obj_center(labels_[i]);binding_[i]={this,i};lv_obj_add_event_cb(buttons_[i],Event,LV_EVENT_PRESSED,&binding_[i]);lv_obj_add_event_cb(buttons_[i],Event,LV_EVENT_CLICKED,&binding_[i]);lv_obj_add_event_cb(buttons_[i],Event,LV_EVENT_PRESS_LOST,&binding_[i]);}
    auto*b=borrow_.Get();ok=b&&b->Open()&&!close_requested_;
   }
  }
  if(!ok){Close();return false;}Render();return true;
 }
 bool Show(){
  if(depth_||!root_||close_requested_)return false;
  bool current=false;{Depth depth(depth_);current=borrow_.Get()&&consent_.BoundCurrent()&&!close_requested_;}
  if(!current){Close();return false;}lv_obj_remove_flag(root_,LV_OBJ_FLAG_HIDDEN);Render();return !close_requested_;
 }
 bool Poll(){
  if(depth_)return false;
  if(close_requested_){Close();return false;}
  if(!root_)return false;
  {Depth depth(depth_);if(dispatch_==Dispatch::Queued)DrainQueued();if(!close_requested_){auto*b=borrow_.Get();if(!b||!consent_.BoundCurrent()||!b->Poll())RequestClose();}}
  if(close_requested_){Close();return false;}return true;
 }
 bool Close(){RequestClose();if(depth_)return false;if(root_){lv_obj_delete(root_);root_=nullptr;}borrow_.Reset();return true;}
 bool TakePrepareRequested(){const bool value=prepare_requested_&&!close_requested_&&consent_.BoundCurrent();prepare_requested_=false;return value;}
 bool TakeEndRequested(){const bool value=end_requested_;end_requested_=false;return value;}
 lv_obj_t* Action(unsigned i)const{return root_&&i<4?buttons_[i]:nullptr;}
};
}
#endif
