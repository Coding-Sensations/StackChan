// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include "muse_direct_local_model.hpp"
#include "muse_direct_credential_policy.hpp"
#include "muse_voice_admission.hpp"
#include <lvgl.h>
#include <mutex>
#include <vector>
#include <cstdio>
namespace companion::muse {
// GUI calls hold the LVGL lock. Worker callbacks capture Scope only, never UI.
// Caller finishes Close before destruction; reentrant Close defers deletion.
template<class Port>class VoiceAdmissionSetupUi final {
 struct Scope {
  Port&port;LocalNetwork expected;std::unique_ptr<LocalNetworkUse> network;mutable std::atomic<bool> active{true};
  muse_native_review::Budget budget;mutable std::recursive_mutex mutex;mutable bool checking=false;mutable muse_native_review::Stamp last;
  Scope(Port&p,const LocalNetwork&n,muse_native_review::Budget b,muse_native_review::Stamp stamp):port(p),expected(n),budget(b),last(stamp){}
  bool Current()const{
   std::lock_guard<std::recursive_mutex> lock(mutex);if(checking)return false;
   struct Busy{bool&b;explicit Busy(bool&v):b(v){b=true;}~Busy(){b=false;}} busy(checking);
   muse_native_review::Stamp now{},after{};const bool ok=active.load()&&port.Foreground()&&port.VoiceClock(now)&&active.load()&&now.mono_us>=last.mono_us&&now.wall_ms>=last.wall_ms&&now.mono_us>=0&&now.wall_ms>=1704067200000LL&&now.mono_us<budget.expires_us&&now.wall_ms<budget.expires_wall_ms&&port.NetworkMatches(expected)&&network&&network->Current()&&active.load()&&port.Foreground()&&port.VoiceClock(after)&&after.mono_us>=now.mono_us&&after.wall_ms>=now.wall_ms&&after.mono_us<budget.expires_us&&after.wall_ms<budget.expires_wall_ms&&active.load();
   if(!ok){active.store(false);return false;}last=after;return true;
  }
 };
 enum class Page{Mode,Keys,Review,Permissions,Confirm,Prepared};Page page_=Page::Mode;
 Port&port_;lv_obj_t*root_=nullptr,*title_=nullptr,*value_=nullptr,*notice_=nullptr;
 std::array<lv_obj_t*,20>buttons_{},labels_{};
 struct Binding{VoiceAdmissionSetupUi*ui=nullptr;unsigned action=0;uint64_t revision=0;uint32_t tick=0;lv_indev_t*input=nullptr;bool armed=false;};std::array<Binding,20>bindings_{};
 uint64_t revision_=0;unsigned depth_=0,key_page_=0,review_=0,permission_=0;
 bool close_requested_=false,return_requested_=false,reviewed_=false;std::array<bool,3>allowed_{};
 std::string vm_;LocalIdentity identity_;LocalNetwork expected_;std::vector<std::pair<std::string,std::string>>pages_;std::shared_ptr<Scope>scope_,pending_;uint64_t scope_generation_=0,pending_generation_=0;
 muse_native_review::Stamp last_{};bool sampled_=false;const char*message_=nullptr;
 struct Depth{unsigned&n;explicit Depth(unsigned&v):n(v){++n;}~Depth(){--n;}};
 void Disarm(){for(auto&b:bindings_)b.armed=false;}
 void Cancel(){auto original=std::move(scope_),pending=std::move(pending_);auto generation=scope_generation_,preparing=pending_generation_;scope_generation_=pending_generation_=0;if(original)original->active.store(false);if(pending)pending->active.store(false);if(original)port_.StopVoice(generation);if(pending)port_.StopVoice(preparing);}
 void Invalidate(){Cancel();reviewed_=false;allowed_.fill(false);pages_.clear();review_=0;Disarm();}
 bool Time(muse_native_review::Stamp&now){const bool ok=port_.Foreground()&&port_.VoiceClock(now)&&!close_requested_&&now.mono_us>=0&&now.wall_ms>=1704067200000LL&&(!sampled_||(now.mono_us>=last_.mono_us&&now.wall_ms>=last_.wall_ms));if(!ok){Cancel();return false;}last_=now;sampled_=true;return true;}
 bool Native(lv_event_t*e,Binding&b,bool down){auto*p=static_cast<lv_indev_t*>(port_.VoiceInput());return p&&lv_event_get_indev(e)==p&&lv_indev_active()==p&&lv_indev_get_type(p)==LV_INDEV_TYPE_POINTER&&lv_indev_get_state(p)==(down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED)&&lv_indev_get_active_obj()==buttons_[b.action]&&lv_event_get_target_obj(e)==buttons_[b.action]&&lv_obj_is_visible(buttons_[b.action]);}
 static void Event(lv_event_t*e){auto&b=*static_cast<Binding*>(lv_event_get_user_data(e));auto&u=*b.ui;Depth depth(u.depth_);auto code=lv_event_get_code(e);if(code==LV_EVENT_PRESS_LOST){b.armed=false;return;}if(u.close_requested_||!u.root_)return;if(code==LV_EVENT_PRESSED){b.armed=u.Native(e,b,true);if(b.armed){b.revision=u.revision_;b.input=static_cast<lv_indev_t*>(u.port_.VoiceInput());b.tick=lv_tick_get();}return;}if(code!=LV_EVENT_CLICKED)return;bool valid=b.armed&&u.Native(e,b,false)&&b.input==u.port_.VoiceInput()&&b.revision==u.revision_&&uint32_t(lv_tick_get()-b.tick)<=10000;b.armed=false;if(valid)u.Act(b.action);}
 lv_obj_t* Label(int y,int h){auto*l=lv_label_create(root_);lv_obj_set_pos(l,8,y);lv_obj_set_size(l,304,h);lv_label_set_long_mode(l,LV_LABEL_LONG_WRAP);lv_obj_set_style_text_font(l,&lv_font_montserrat_14,0);return l;}
 void Button(unsigned i,const char*text,int x=8,int y=-1,int w=304,int h=32){lv_obj_remove_flag(buttons_[i],LV_OBJ_FLAG_HIDDEN);lv_obj_set_pos(buttons_[i],x,y<0?132+36*int(i):y);lv_obj_set_size(buttons_[i],w,h);lv_label_set_text(labels_[i],text);}
 void Render(){
  if(!root_||close_requested_)return;
  if(revision_==UINT64_MAX){close_requested_=true;Cancel();return;}++revision_;Disarm();for(auto*b:buttons_)lv_obj_add_flag(b,LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_pos(notice_,8,88);lv_obj_set_height(notice_,40);lv_obj_set_height(value_,50);
  lv_label_set_text(notice_,message_?message_:"Admission only, up to 2 min. Mic/speaker stay off.");
  if(page_==Page::Mode){lv_label_set_text(title_,"Prepare Muse voice");lv_label_set_text(value_,vm_.empty()?"Enter the exact VM ID":vm_.c_str());Button(0,"Enter VM");Button(1,"Review device/Wi-Fi");Button(2,"Close");return;}
  if(page_==Page::Keys){lv_label_set_text(title_,"Exact VM ID");auto tail=vm_.substr(vm_.size()>20?vm_.size()-20:0);lv_label_set_text(value_,tail.c_str());lv_obj_set_height(value_,22);lv_obj_set_pos(notice_,8,58);lv_obj_set_height(notice_,18);lv_label_set_text(notice_,"VM ID only; no SDK token.");static constexpr char keys[]="abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-_.:";unsigned first=key_page_*16;for(unsigned i=0;i<16&&first+i<sizeof(keys)-1;++i){char text[2]={keys[first+i],0};Button(i,text,8+76*int(i%4),82+30*int(i/4),70,28);}Button(16,"Prev keys",8,206,70,28);Button(17,"Next keys",84,206,70,28);Button(18,"Delete",160,206,70,28);Button(19,"Done",236,206,70,28);return;}
  if(page_==Page::Review){lv_label_set_text(title_,pages_[review_].first.c_str());lv_label_set_text(value_,pages_[review_].second.c_str());Button(0,"Next");Button(2,"Cancel");return;}
  if(page_==Page::Permissions){static const char* questions[]={"Allow existing RAM credential read?","Allow this saved Wi-Fi connection?","Allow contact with this exact VM for voice admission?"};lv_label_set_text(title_,"Separate admission permissions");lv_label_set_text(value_,questions[permission_]);Button(0,allowed_[permission_]?"Revoke":"Allow");Button(1,permission_==2?"Continue":"Next permission");Button(2,"Cancel");return;}
  if(page_==Page::Confirm){lv_label_set_text(title_,"Prepare admission only");lv_label_set_text(value_,"Start separately in Settings. Review the returned session before audio.");Button(0,"Prepare admission");Button(1,"Back");Button(2,"Cancel");return;}
  lv_label_set_text(title_,"Voice admission prepared");lv_label_set_text(value_,"Return to Settings, then Start prepared voice. RAM credential stays on device.");Button(0,"Return to Settings");Button(2,"Cancel voice");
 }
 void Add(const char*name,const std::string&text){for(size_t i=0;i<text.size();i+=18){char heading[64]{};std::snprintf(heading,sizeof heading,"%s (%u/%u)",name,unsigned(i/18+1),unsigned((text.size()+17)/18));pages_.emplace_back(heading,text.substr(i,18));}}
 bool Capture(){
  Invalidate();if(!Id(vm_)||!port_.PairIdle()||!port_.ChatIdle()||!port_.VoiceIdle())return false;
  identity_={};expected_={};if(!port_.Identity(identity_)||!Id(identity_.node)||!Id(identity_.device)||!Id(identity_.mac,17)||!port_.Network(expected_)||!expected_.valid||!port_.NetworkMatches(expected_)||close_requested_)return false;
  std::string ssid;static constexpr char hex[]="0123456789abcdef";for(auto c:expected_.ssid){if(!c)break;if(c>=32&&c<=126&&c!='\\')ssid+=char(c);else{ssid+="\\x";ssid+=hex[c>>4];ssid+=hex[c&15];}}
  char bssid[18]{};auto&b=expected_.bssid;std::snprintf(bssid,sizeof bssid,"%02x:%02x:%02x:%02x:%02x:%02x",b[0],b[1],b[2],b[3],b[4],b[5]);Add("Device",identity_.device);Add("Node",identity_.node);Add("VM",vm_);Add("Wi-Fi",ssid.empty()?"(empty)":ssid);Add("BSSID",bssid);return !pages_.empty();
 }
 bool Prepare(){
  if(!reviewed_||!allowed_[0]||!allowed_[1]||!allowed_[2]||!port_.VoiceReady()||!port_.VoiceIdle()||!port_.PairIdle()||!port_.ChatIdle()||!port_.NetworkMatches(expected_))return false;
  muse_native_review::Stamp now{};LocalIdentity actual;uint64_t generation=0;std::string call;
  if(!Time(now)||now.mono_us>INT64_MAX-120000000||now.wall_ms>INT64_MAX-120000||!port_.Identity(actual)||actual.node!=identity_.node||actual.device!=identity_.device||!port_.VoiceIdentity(generation,call)||!generation||!voice_prep::Protocol::ValidAdmissionIds(call,identity_.node)||close_requested_)return false;
  auto context=std::make_shared<Scope>(port_,expected_,muse_native_review::Budget{generation,now.mono_us+120000000,now.wall_ms+120000},now);pending_=context;pending_generation_=generation;context->network=port_.Borrow(expected_);if(!context->network||!context->Current()||close_requested_){context->active.store(false);pending_.reset();pending_generation_=0;return false;}
  VoiceAdmissionScope grant;grant.node=identity_.node;grant.vm=vm_;grant.call=std::move(call);grant.budget=context->budget;grant.credential_current=[context]{return context->Current();};grant.network_current=[context]{return context->Current();};grant.voice_current=[context]{return context->Current();};
  const bool accepted=port_.PrepareVoice(grant);if(!accepted||close_requested_||pending_!=context||!context->Current()){context->active.store(false);if(accepted)port_.StopVoice(generation);pending_.reset();pending_generation_=0;return false;}
  pending_.reset();pending_generation_=0;scope_=std::move(context);scope_generation_=generation;return true;
 }
 void Act(unsigned action){
  muse_native_review::Stamp now{};if(!Time(now)||close_requested_){close_requested_=true;return;}message_=nullptr;
  if(page_==Page::Mode){if(action==0){Invalidate();page_=Page::Keys;key_page_=0;}else if(action==1){if(Capture())page_=Page::Review;else message_="Check VM/device/saved Wi-Fi; wait for prior cleanup.";}else if(action==2){Close();return;}}
  else if(page_==Page::Keys){static constexpr char keys[]="abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-_.:";if(action<16&&key_page_*16+action<sizeof(keys)-1&&vm_.size()<128)vm_+=keys[key_page_*16+action];else if(action==16&&key_page_)--key_page_;else if(action==17)key_page_=(key_page_+1)%5;else if(action==18&&!vm_.empty())vm_.pop_back();else if(action==19)page_=Page::Mode;}
  else if(page_==Page::Review){if(action==0){if(++review_==pages_.size()){reviewed_=true;permission_=0;page_=Page::Permissions;}}else if(action==2){Invalidate();page_=Page::Mode;}}
  else if(page_==Page::Permissions){if(action==0)allowed_[permission_]=!allowed_[permission_];else if(action==1){if(permission_<2)++permission_;else if(allowed_[0]&&allowed_[1]&&allowed_[2])page_=Page::Confirm;else message_="All three separate permissions are required.";}else if(action==2){Invalidate();page_=Page::Mode;}}
  else if(page_==Page::Confirm){if(action==0){if(Prepare())page_=Page::Prepared;else message_="Preparation refused: scope, RAM profile, clock or initialized audio unavailable.";}else if(action==1){permission_=0;page_=Page::Permissions;}else if(action==2){Invalidate();page_=Page::Mode;}}
  else if(page_==Page::Prepared){if(action==0){return_requested_=true;lv_obj_add_flag(root_,LV_OBJ_FLAG_HIDDEN);Disarm();return;}if(action==2){Close();return;}}
  if(!close_requested_)Render();
 }
public:
 explicit VoiceAdmissionSetupUi(Port&p):port_(p){}
 VoiceAdmissionSetupUi(const VoiceAdmissionSetupUi&)=delete;VoiceAdmissionSetupUi&operator=(const VoiceAdmissionSetupUi&)=delete;
 ~VoiceAdmissionSetupUi(){assert(!root_&&!depth_);Cancel();}
 bool Open(lv_obj_t*parent){if(!STACKCHAN_MUSE_RAM_ONLY||depth_||close_requested_||!parent||!port_.VoiceInput())return false;if(root_){lv_obj_remove_flag(root_,LV_OBJ_FLAG_HIDDEN);return true;}lv_obj_update_layout(parent);if(lv_obj_get_width(parent)<320||lv_obj_get_height(parent)<240)return false;root_=lv_obj_create(parent);lv_obj_set_size(root_,320,240);lv_obj_set_pos(root_,0,0);lv_obj_set_style_pad_all(root_,0,0);lv_obj_set_style_border_width(root_,0,0);lv_obj_remove_flag(root_,LV_OBJ_FLAG_SCROLLABLE);lv_obj_update_layout(root_);title_=Label(8,24);value_=Label(34,50);notice_=Label(88,40);for(unsigned i=0;i<20;++i){buttons_[i]=lv_button_create(root_);labels_[i]=lv_label_create(buttons_[i]);lv_obj_set_style_text_font(labels_[i],&lv_font_montserrat_14,0);lv_obj_center(labels_[i]);bindings_[i]={this,i};for(auto code:{LV_EVENT_PRESSED,LV_EVENT_CLICKED,LV_EVENT_PRESS_LOST})lv_obj_add_event_cb(buttons_[i],Event,code,&bindings_[i]);}Render();return true;}
 bool Poll(){if(depth_)return false;if(close_requested_)return Close();if(!root_)return false;Depth depth(depth_);muse_native_review::Stamp now{};auto captured=scope_;if(!Time(now)||(captured&&(!captured->Current()||port_.VoiceIdle()))){Invalidate();page_=Page::Mode;message_="Voice scope finished or revoked.";Render();}return !close_requested_;}
 bool Close(){close_requested_=true;Cancel();Disarm();if(depth_)return false;if(root_){lv_obj_delete(root_);root_=nullptr;}Wipe(vm_);pages_.clear();return true;}
 bool Closed()const{return !root_&&close_requested_&&!depth_;}
 bool TakeReturnRequested(){bool value=return_requested_;return_requested_=false;return value;}
};
}
#endif
