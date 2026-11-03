// SPDX-License-Identifier: Apache-2.0
#pragma once
#if defined(STACKCHAN_MUSE_LOCAL_DIAGNOSTIC) && STACKCHAN_MUSE_LOCAL_DIAGNOSTIC
#include <hal/board/cores3_audio_codec.h>
#include <companion/muse_quiesce_diagnostic.hpp>
#include <lvgl.h>
#include <atomic>
#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <cstdio>
namespace companion::muse::local_diag {
class Ui;class Owner;
struct ViewLifetime {std::atomic<bool>live{true};};
class RunGrant final {
 std::shared_ptr<ViewLifetime>life_;void*input_=nullptr;uint32_t tick_=0;bool live_=true;
 RunGrant(std::shared_ptr<ViewLifetime>life,void*input,uint32_t tick):life_(std::move(life)),input_(input),tick_(tick){}
 bool Consume(void*input,uint32_t now){bool okay=live_&&life_&&life_->live.load()&&input&&input==input_&&uint32_t(now-tick_)<=10000;live_=false;return okay;}
 friend class Ui;friend class Owner;
public:
 RunGrant(const RunGrant&)=delete;RunGrant&operator=(const RunGrant&)=delete;
 RunGrant(RunGrant&&other)noexcept:life_(std::move(other.life_)),input_(other.input_),tick_(other.tick_),live_(std::exchange(other.live_,false)){}
};
class InputPort {
public:virtual ~InputPort()=default;virtual lv_indev_t*Input()const=0;
};
class Ui final {
 InputPort&port_;lv_obj_t*root_=nullptr,*notice_=nullptr;std::array<lv_obj_t*,3>buttons_{};
 struct Binding{Ui*ui=nullptr;unsigned action=0;uint64_t revision=0;uint32_t tick=0;lv_indev_t*input=nullptr;bool pressed=false;};std::array<Binding,3>bindings_{};
 std::shared_ptr<ViewLifetime>life_;std::optional<RunGrant>pending_;uint64_t revision_=1;uint32_t arm_tick_=0;lv_indev_t*armed_input_=nullptr;
 enum class Page{Idle,Confirm,Committed,Closed};Page page_=Page::Closed;
 bool Native(lv_event_t*e,Binding&b,bool down){auto*p=port_.Input();return p&&lv_event_get_indev(e)==p&&lv_indev_active()==p&&lv_indev_get_type(p)==LV_INDEV_TYPE_POINTER&&lv_indev_get_state(p)==(down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED)&&lv_indev_get_active_obj()==buttons_[b.action]&&lv_event_get_target_obj(e)==buttons_[b.action]&&lv_obj_is_visible(buttons_[b.action]);}
 static void Event(lv_event_t*e){auto&b=*static_cast<Binding*>(lv_event_get_user_data(e));auto&u=*b.ui;auto code=lv_event_get_code(e);if(code==LV_EVENT_PRESS_LOST){b.pressed=false;return;}if(!u.root_||u.page_==Page::Committed||u.page_==Page::Closed)return;if(code==LV_EVENT_PRESSED){b.pressed=u.Native(e,b,true);if(b.pressed){b.input=u.port_.Input();b.revision=u.revision_;b.tick=lv_tick_get();}return;}if(code!=LV_EVENT_CLICKED)return;const bool valid=b.pressed&&u.Native(e,b,false)&&b.input==u.port_.Input()&&b.revision==u.revision_&&uint32_t(lv_tick_get()-b.tick)<=10000;b.pressed=false;if(!valid)return;
  if(b.action==0&&u.page_==Page::Idle){u.armed_input_=b.input;u.arm_tick_=lv_tick_get();u.page_=Page::Confirm;u.Show();}
  else if(b.action==2&&u.page_==Page::Confirm){u.armed_input_=nullptr;u.page_=Page::Idle;u.Show();}
  else if(b.action==1&&u.page_==Page::Confirm){const bool current=u.armed_input_==b.input&&uint32_t(lv_tick_get()-u.arm_tick_)<=10000;u.page_=Page::Committed;if(current)u.pending_.emplace(RunGrant(u.life_,b.input,lv_tick_get()));u.Show();if(!current)lv_label_set_text(u.notice_,"Not run: local confirmation expired. No retry.");}
 }
 void Show(){++revision_;for(unsigned i=0;i<3;++i){bindings_[i].pressed=false;const bool visible=(page_==Page::Idle&&i==0)||(page_==Page::Confirm&&i!=0);if(visible)lv_obj_remove_flag(buttons_[i],LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(buttons_[i],LV_OBJ_FLAG_HIDDEN);}lv_label_set_text(notice_,page_==Page::Idle?"Cold diagnostic only. No capture, playback or network startup. Arm, then Run once. No physical-flush proof.":page_==Page::Confirm?"Run one TX/amplifier retirement check? No retry after an attempt. Cancel before Run.":"One attempt committed. Source report is not hardware/acoustic qualification.");}
public:
 explicit Ui(InputPort&p):port_(p){}~Ui(){Close();}Ui(const Ui&)=delete;Ui&operator=(const Ui&)=delete;
 bool Open(lv_obj_t*parent){if(root_||!parent)return false;life_=std::make_shared<ViewLifetime>();root_=lv_obj_create(parent);lv_obj_set_pos(root_,0,0);lv_obj_set_size(root_,320,240);lv_obj_set_style_pad_all(root_,0,0);notice_=lv_label_create(root_);lv_obj_set_pos(notice_,8,8);lv_obj_set_width(notice_,296);lv_label_set_long_mode(notice_,LV_LABEL_LONG_WRAP);
  const char*labels[]={"Arm diagnostic","Run once","Cancel"};for(unsigned i=0;i<3;++i){buttons_[i]=lv_button_create(root_);lv_obj_set_pos(buttons_[i],i==2?166:8,174);lv_obj_set_size(buttons_[i],146,52);auto*label=lv_label_create(buttons_[i]);lv_label_set_text(label,labels[i]);lv_obj_center(label);bindings_[i].ui=this;bindings_[i].action=i;for(auto event:{LV_EVENT_PRESSED,LV_EVENT_CLICKED,LV_EVENT_PRESS_LOST})lv_obj_add_event_cb(buttons_[i],Event,event,&bindings_[i]);}page_=Page::Idle;Show();return true;
 }
 std::optional<RunGrant>TakeRun(){auto result=std::move(pending_);pending_.reset();return result;}
 void Report(const char*text){if(root_&&page_==Page::Committed){for(auto*b:buttons_)lv_obj_add_flag(b,LV_OBJ_FLAG_HIDDEN);lv_label_set_text(notice_,text);}}
 void Close(){if(life_)life_->live.store(false);pending_.reset();if(root_)lv_obj_delete(root_);root_=notice_=nullptr;page_=Page::Closed;}
};
class Driver:public InputPort {
public:virtual uint32_t Tick()const=0;virtual CoreS3AudioCodec&Core()=0;virtual void*Bus()=0;
};
class Owner final {
public:enum class State{Idle,Running,Rejected,Busy,Retained,CustodyLost,Complete};
private:
 Driver&port_;std::atomic<State>state_{State::Idle};AudioLease lease_;CoreS3AudioCodec::MuseColdEvidence init_;QuiesceDiagnostic report_;
 State Retain(const AudioLease&original){RevokeAudio(original);auto s=CommonAudioAuthority().Holds(original)?State::Retained:State::CustodyLost;state_.store(s,std::memory_order_release);return s;}
public:
 explicit Owner(Driver&p):port_(p){}Owner(const Owner&)=delete;Owner&operator=(const Owner&)=delete;
 // Native dispatcher retains this object for program lifetime, even on failure.
 State Run(RunGrant&&grant){RunGrant owned(std::move(grant));auto idle=State::Idle;if(!state_.compare_exchange_strong(idle,State::Running))return idle;
  try{if(!owned.Consume(port_.Input(),port_.Tick())){state_.store(State::Rejected);return State::Rejected;}}catch(...){state_.store(State::Rejected);return State::Rejected;}
  if(!AcquireAudio(AudioOwner::Muse,AudioPurpose::MusePushToTalk,1,lease_)){state_.store(State::Busy);return State::Busy;}
  const auto original=lease_;try{auto&core=port_.Core();if(!CommonAudioAuthority().Holds(original))return Retain(original);
  CoreS3AudioCodec::MuseColdOutputConfig config;config.bus=port_.Bus();if(!CommonAudioAuthority().Holds(original))return Retain(original);
  init_=core.MuseColdInitialize(original,config);if(!init_.Complete()||!CommonAudioAuthority().Holds(original))return Retain(original);
  const auto result=core.MuseQuiesceForRetirementChecked(original,1,&report_);
  if(result.outcome!=CoreS3AudioCodec::MuseQuiesce::RegistersAndStoppedDma||!CommonAudioAuthority().Holds(original))return Retain(original);
  const bool checked=core.MuseCodecRelease(original)&&CommonAudioAuthority().Holds(original);
  // This owner creates no worker: all operations above returned synchronously.
  if(!checked||!ReleaseAudio(lease_,checked))return Retain(original);
  state_.store(State::Complete,std::memory_order_release);return State::Complete;}catch(...){return Retain(original);}
 }
 State Status()const{return state_.load(std::memory_order_acquire);}
 static const char*Name(State s){switch(s){case State::Complete:return "complete";case State::Retained:return "retained-no-retry";case State::CustodyLost:return "custody-lost-no-action";case State::Rejected:return "local-optin-rejected";case State::Busy:return "audio-busy-no-action";default:return "not-complete";}}
 bool Format(char*out,size_t capacity)const{if(!out||!capacity)return false;out[0]=0;const auto s=Status();if(s==State::Running||s==State::Idle)return false;char detail[QuiesceDiagnostic::TextCapacity];if(!report_.Format(detail,sizeof detail))return false;const int n=std::snprintf(out,capacity,"diagnostic owner=%s init-stage=%u init-fault=%u init-status=%d; %s",Name(s),unsigned(init_.stage),unsigned(init_.fault),init_.status,detail);if(n<0||size_t(n)>=capacity){out[0]=0;return false;}return true;}
};
}
#endif
