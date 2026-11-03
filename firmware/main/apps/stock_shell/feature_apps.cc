#include "feature_apps.h"
#include <apps/common/home_indicator/home_indicator.h>
#include <companion/direct_usb_pcm/usb_pcm_app.h>
#include <apps/common/tool_visual.hpp>
#include <companion/feature_link.h>
#include <companion/boot.h>
#include <companion/station_authority.hpp>
#include <companion/public_status_owner.hpp>
#include <companion/numeric_usb.hpp>
#include <companion/diagnostics.h>
#include "feature_result_text.hpp"
#include "feature_ui_action.hpp"
#include "feature_usb_driver.hpp"
#include <usb_composite_v2/startup.h>
#include <driver/usb_serial_jtag.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <board.h>
#include <hal/hal.h>
#include <assets/assets.h>
#include <lvgl.h>
#include <cJSON.h>
#include <memory>
#include <atomic>
#include <vector>
#include <mutex>
#include <optional>
#include <cstring>
#ifndef STACKCHAN_COMPANION_TRUSTED_USB_PROVISIONING
#define STACKCHAN_COMPANION_TRUSTED_USB_PROVISIONING 0
#endif
namespace stock_tools {namespace {
uint64_t Now(){return static_cast<uint64_t>(esp_timer_get_time()/1000);}
struct Legacy {
 bool OnOwnerCore()const{return xPortGetCoreID()==0;}bool Installed()const{return usb_serial_jtag_is_driver_installed();}
 bool Install(){usb_serial_jtag_driver_config_t c={};c.rx_buffer_size=1024;c.tx_buffer_size=64;return usb_serial_jtag_driver_install(&c)==ESP_OK;}
 bool Uninstall(){return usb_serial_jtag_driver_uninstall()==ESP_OK;}
 int Read(uint8_t* p,size_t n){return usb_serial_jtag_read_bytes(p,n,0);}
 int WriteAck(const uint8_t* p,size_t n){return usb_serial_jtag_write_bytes(p,n,pdMS_TO_TICKS(100));}
 bool FlushAck(){return usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(100))==ESP_OK;}
};
struct Mode {bool Requested()const{return usb_composite_v2::CompositeRequested();}auto* BridgeNow(){return usb_composite_v2::CompositeBridge();}};
using Driver=companion::FeatureUsbDriver<Legacy,usb_composite_v2::TinyUsbBridge,Mode>;
struct Runtime {
 Driver driver;companion::NumericUsbOwner<Driver> usb{driver};companion::FeatureLink link;
 companion::network::StationBorrow network_use;
 bool pending_wifi=false,audio_selected=false,audio_stage_attempted=false;companion::NumericUsbRequest request;
 std::vector<std::string> scopes;uint64_t expires=0;
 bool CloseFeatures(){pending_wifi=false;request={};expires=0;audio_stage_attempted=false;link.Close();const bool closed=link.Stopped()&&usb.Close();if(closed)network_use.Reset();return closed;}
};
Runtime& State(){static auto* state=new Runtime;return *state;}
const std::vector<std::vector<std::string>> Groups={{"desktop.health"},{"network.wol"},{"apple.home.execute","apple.home.scenes","apple.now_playing"},{"mira.forwarding.interrupt","mira.forwarding.metadata","mira.forwarding.status"}};
const char* Titles[]={"PC health","Wake-on-LAN","Apple controls","Mira forwarding"};
std::string JsonArgs(std::initializer_list<std::pair<const char*,std::string>> fields,uint32_t epoch=0){auto p=cJSON_CreateObject();for(auto& f:fields)cJSON_AddStringToObject(p,f.first,f.second.c_str());if(epoch)cJSON_AddNumberToObject(p,"epoch",epoch);auto raw=cJSON_PrintUnformatted(p);std::string out=raw?raw:"";cJSON_free(raw);cJSON_Delete(p);return out;}
class JointApp final:public mooncake::AppAbility {
 struct Callback {JointApp* owner;companion::FeatureUiAction action;lv_obj_t* picker=nullptr;std::vector<std::string> options;};
 std::vector<std::unique_ptr<Callback>> callbacks_,native_callbacks_;
 std::mutex action_mutex_;std::optional<companion::FeatureUiAction> action_;
 lv_obj_t *frame_=nullptr,*native_root_=nullptr,*root_=nullptr,*native_controls_=nullptr,*native_actions_=nullptr,*status_=nullptr,*picker_=nullptr;
 lv_obj_t* pending_panel_=nullptr;lv_obj_t* pending_text_=nullptr;
 std::unique_ptr<direct_usb_pcm::Page> usb_view_;
 std::atomic<bool> usb_pending_{false},usb_active_{false};bool usb_return_=false;
 bool controls_visible_=false,closing_=false,ready_ui_=false;unsigned group_=0;uint32_t rendered_epoch_=UINT32_MAX;
 std::string shown_code_,shown_request_,shown_session_,shown_mira_,rendered_text_,formatted_raw_,formatted_text_,local_notice_,scenes_result_;
 struct Scene {std::string home,id,label;};std::vector<Scene> scenes_;
 static void Click(lv_event_t* event){auto* c=static_cast<Callback*>(lv_event_get_user_data(event));auto intent=c->action;if(c->picker){auto n=lv_dropdown_get_selected(c->picker);intent.arguments=n<c->options.size()?c->options[n]:"";}std::lock_guard<std::mutex> lock(c->owner->action_mutex_);if(!c->owner->action_)c->owner->action_=std::move(intent);}
 void Button(lv_obj_t* parent,const char* title,int x,int y,int w,int id,std::vector<std::unique_ptr<Callback>>& storage,std::vector<std::string> options={}){
  auto& link=State().link;auto b=stock_ui::Button(parent,title,x,y,w,40);
  auto c=std::make_unique<Callback>();c->owner=this;c->action.id=id;c->action.request=link.PairRequest();c->action.session=link.SessionId();c->action.code=link.Code();c->action.epoch=companion::FeatureAudioEpoch(link);c->options=std::move(options);c->picker=c->options.empty()?nullptr:picker_;
  if(id==15||id==16)c->action.arguments=link.MiraReference().empty()?"":JsonArgs({{"session_reference",link.MiraReference()}},link.MiraEpoch());
  if(id==12||id==13||id==17)c->action.arguments="{}";
  if(id==23&&!c->action.epoch)lv_obj_add_state(b,LV_STATE_DISABLED);
  if(id==4&&(link.Code().empty()||!link.Pairing(Now())))lv_obj_add_state(b,LV_STATE_DISABLED);
  if((id==15||id==16)&&link.MiraReference().empty())lv_obj_add_state(b,LV_STATE_DISABLED);
  lv_obj_add_event_cb(b,Click,LV_EVENT_CLICKED,c.get());storage.push_back(std::move(c));
 }
 void RenderNativeControls(){LvglLockGuard lock;lv_obj_clean(native_controls_);lv_obj_clean(native_actions_);native_callbacks_.clear();if(lv_obj_get_width(native_root_)<320||lv_obj_get_height(native_root_)<240)return;auto& link=State().link;rendered_epoch_=companion::FeatureAudioEpoch(link);
  Button(native_controls_,"More",0,0,70,25,native_callbacks_);Button(native_controls_,"Back",74,0,58,1,native_callbacks_);
  Button(native_actions_,"Mira",8,196,304,21,native_callbacks_);
  Button(native_actions_,"USB Mira",8,52,304,24,native_callbacks_);
  Button(native_actions_,"Feature controls",8,100,304,20,native_callbacks_);
  if(companion::FeatureAudioStaged(link)){Button(native_actions_,"Connect",8,148,148,22,native_callbacks_);Button(native_actions_,"Listen",164,148,148,23,native_callbacks_);}
 }
 void ShowControls(bool visible){controls_visible_=visible;LvglLockGuard lock;if(visible){lv_obj_add_flag(native_root_,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(native_controls_,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(native_actions_,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(root_,LV_OBJ_FLAG_HIDDEN);}else{lv_obj_add_flag(root_,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(native_root_,LV_OBJ_FLAG_HIDDEN);if(companion::CompanionCharacterVisible())lv_obj_add_flag(native_controls_,LV_OBJ_FLAG_HIDDEN);else lv_obj_remove_flag(native_controls_,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(native_actions_,LV_OBJ_FLAG_HIDDEN);}}
 void ShowRoutes(){ShowControls(false);LvglLockGuard lock;lv_obj_add_flag(native_root_,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(native_actions_,LV_OBJ_FLAG_HIDDEN);}
 void Render(){
  {LvglLockGuard lock;rendered_text_.clear();lv_obj_clean(root_);callbacks_.clear();picker_=nullptr;auto& s=State();shown_code_=s.link.Code();shown_request_=s.link.PairRequest();shown_session_=s.link.SessionId();shown_mira_=s.link.MiraReference()+std::to_string(s.link.MiraEpoch());
   Button(root_,"Back",4,4,60,1,callbacks_);Button(root_,"Mira",248,4,68,21,callbacks_);auto title=lv_label_create(root_);lv_label_set_text(title,Titles[group_]);lv_obj_set_pos(title,72,10);
   auto body=lv_obj_create(root_);lv_obj_set_pos(body,4,46);lv_obj_set_size(body,312,104);lv_obj_set_style_bg_color(body,lv_color_hex(stock_ui::Panel),0);lv_obj_set_style_border_width(body,0,0);lv_obj_set_style_pad_all(body,2,0);status_=lv_label_create(body);lv_obj_set_width(status_,280);lv_obj_set_height(status_,LV_SIZE_CONTENT);lv_label_set_long_mode(status_,LV_LABEL_LONG_WRAP);
   if(!ready_ui_){Button(root_,s.audio_selected?"Mic/speaker + features: ON":"Feature scopes only",4,152,312,5,callbacks_);Button(root_,"Choose",4,198,94,2,callbacks_);Button(root_,"Pair",104,198,94,3,callbacks_);Button(root_,"Confirm code",204,198,112,4,callbacks_);}
   else {
    std::string choices;std::vector<std::string> options;auto add=[&](std::string label,std::string args){if(!choices.empty())choices+='\n';choices+=label;options.push_back(args);};
    if(group_==0)for(auto& r:s.link.Volumes())add(r.label,JsonArgs({{"volume_id",r.id}}));
    if(group_==1)for(auto& r:s.link.Targets())add(r.label,JsonArgs({{"target_id",r.id},{"intent","send_once"}}));
    if(group_==2)for(auto& r:scenes_)add(r.label,JsonArgs({{"homeID",r.home},{"sceneID",r.id}}));
    if(!choices.empty()){picker_=lv_dropdown_create(root_);lv_obj_set_pos(picker_,8,152);lv_obj_set_size(picker_,300,40);lv_dropdown_set_options(picker_,choices.c_str());}
    if(group_==0)Button(root_,"Refresh health",8,200,300,10,callbacks_,options);
    if(group_==1)Button(root_,"Send wake once",8,200,300,11,callbacks_,options);
    if(group_==2){Button(root_,"Music",4,200,86,12,callbacks_);Button(root_,"Scenes",96,200,94,13,callbacks_);Button(root_,"Run scene",196,200,120,14,callbacks_,options);}
    if(group_==3){Button(root_,"Refresh",4,200,90,17,callbacks_);Button(root_,"Status",100,200,92,15,callbacks_);Button(root_,"Stop local",198,200,118,16,callbacks_);}
   }
  }Update();
 }
 void Update(){auto& s=State();std::string text;
  if(s.audio_selected)text+="Microphone/speaker scope; Listen is separate.\n";
  for(auto& cap:Groups[group_])text+=std::string(companion::FeatureScopeLabel(cap))+"\n";
  if(!s.link.Code().empty())text+="Code: "+s.link.Code()+"\nMatch host code and scopes before Confirm.\n";
  if(s.usb.Active())text+="Waiting for host numeric request.\n";else if(s.pending_wifi)text+="Connecting saved Wi-Fi.\n";
  if(companion::FeatureAudioStaged(s.link))text+="Audio staged muted. Mira: Connect, then Listen.\n";
  text+=local_notice_;if(!local_notice_.empty())text+='\n';text+=s.link.Notice();
  if(formatted_raw_!=s.link.Result()){formatted_raw_=s.link.Result();formatted_text_=companion::FeatureResultText(formatted_raw_);}if(!formatted_text_.empty())text+='\n'+formatted_text_;
  if(text==rendered_text_){return;
}rendered_text_=text;LvglLockGuard lock;if(status_)lv_label_set_text(status_,text.c_str());
 }
 void ReadScenes(){auto& raw=State().link.Result();if(raw==scenes_result_)return;
scenes_result_=raw;auto p=cJSON_Parse(raw.c_str());if(!p)return;
auto a=cJSON_GetObjectItemCaseSensitive(p,"scenes");if(cJSON_IsArray(a)&&cJSON_GetArraySize(a)<=16){std::vector<Scene> next;for(auto v=a->child;v;v=v->next){auto h=cJSON_GetObjectItemCaseSensitive(v,"homeID"),s=cJSON_GetObjectItemCaseSensitive(v,"sceneID"),name=cJSON_GetObjectItemCaseSensitive(v,"name");if(cJSON_IsString(h)&&cJSON_IsString(s)&&h->valuestring&&s->valuestring&&std::strlen(h->valuestring)==36&&std::strlen(s->valuestring)==36){std::string label=cJSON_IsString(name)&&name->valuestring?name->valuestring:"Unnamed scene";for(auto& c:label)if(static_cast<unsigned char>(c)<32||c==127)c=' ';next.push_back({h->valuestring,s->valuestring,label});}}scenes_=std::move(next);Render();}cJSON_Delete(p);}
 void ShowPending(const char* text){if(!frame_)return;LvglLockGuard lock;if(!pending_panel_){pending_panel_=stock_ui::Card(frame_,8,72,304,96);pending_text_=stock_ui::Text(pending_panel_,text,12,12,280);}else lv_label_set_text(pending_text_,text);}
 void ClearPending(){LvglLockGuard lock;if(pending_panel_)lv_obj_delete(pending_panel_);pending_panel_=pending_text_=nullptr;}
 bool CloseUsbView(){
  if(!usb_view_)return true;
  if(!usb_view_->RequestClose()){usb_view_->onRunning();return false;}
  usb_view_->onClose();usb_view_.reset();usb_active_=false;usb_return_=false;
  {LvglLockGuard lock;if(frame_)lv_obj_remove_flag(frame_,LV_OBJ_FLAG_HIDDEN);}
  return true;
 }
 bool CloseAll(){if(!companion::CloseView()||!State().CloseFeatures())return false;return companion::PollSavedNetworkStopped();}
 bool PublicStatusIdle(){if(usb_pending_.load()||usb_active_.load())return false;std::lock_guard<std::mutex> lock(action_mutex_);auto& s=State();return !closing_&&!action_&&!s.usb.OwnsDriver()&&!s.pending_wifi&&s.link.Stopped();}
 void Handle(const companion::FeatureUiAction& a,uint64_t now){auto& s=State();auto& link=s.link;
  if(a.id==1){closing_=true;return;
}if(a.id==25){ShowRoutes();return;
}if(a.id==20){ShowControls(true);Render();return;
}if(a.id==21){companion::ShowMiraCharacter();ShowControls(false);RenderNativeControls();return;
}
  if(a.id==24){usb_pending_=true;return;}
  if(a.id==22){if(!a.request.empty()&&a.request==link.PairRequest())companion::ConnectFeatureAudio(link,a.request);ShowControls(false);return;
}
  if(a.id==23){if(a.AudioMatches(link.PairRequest(),companion::FeatureAudioEpoch(link)))companion::ListenFeatureAudio(link,a.request,a.epoch);ShowControls(false);return;
}
  bool selectable=companion::FeatureAudioIdle()&&!s.usb.Active()&&!s.pending_wifi&&!link.Pairing(now)&&!link.Ready(now);
  if(a.id==2&&selectable){group_=(group_+1)%Groups.size();scenes_.clear();ready_ui_=false;Render();return;
}
  if(a.id==5&&selectable){s.audio_selected=!s.audio_selected;Render();return;
}
  if(a.id==3){
   if(!companion::FeatureAudioIdle()){local_notice_="Stop the existing session before pairing.";return;
}
   if(group_==3&&!s.audio_selected){local_notice_="Select microphone/speaker union scope for Mira.";return;
}
   if(!s.CloseFeatures()){local_notice_="USB cleanup pending.";return;
}
   if(!STACKCHAN_COMPANION_TRUSTED_USB_PROVISIONING){local_notice_="Trusted USB provisioning unavailable.";return;
}
   scenes_.clear();scenes_result_.clear();local_notice_.clear();s.scopes=Groups[group_];auto boot=companion::diagnostic::BootNonce();
   if(!s.usb.Open(now,boot?boot:1,static_cast<uint32_t>(esp_reset_reason())))local_notice_=companion::NumericUsbOpenMessage(s.usb.LastOpenError());
   ready_ui_=false;Render();return;

  }
  if(a.id==4){if(a.PairMatches(link.PairRequest(),link.Code()))link.Confirm(now);return;
}
  if(a.id>=10&&a.id<=17&&link.Ready(now)&&a.SessionMatches(link.PairRequest(),link.SessionId())&&!a.arguments.empty()){
   const char* cap=nullptr;switch(a.id){case 10:cap="desktop.health";break;case 11:cap="network.wol";break;case 12:cap="apple.now_playing";break;case 13:cap="apple.home.scenes";break;case 14:cap="apple.home.execute";break;case 15:cap="mira.forwarding.status";break;case 16:cap="mira.forwarding.interrupt";break;case 17:cap="mira.forwarding.metadata";break;}
   if(cap)link.Request(cap,a.arguments,now);
  }
 }
public:
 JointApp(){setAppInfo().name="Mira";static auto icon=assets::get_image("icon_ai_agent.bin");setAppInfo().icon=&icon;}
 ~JointApp()override{companion::PublicStatusOwner().Clear(this);}
 void onOpen()override{
  // Mooncake re-enters Running while an external close waits; retain this UI.
  if(closing_)return;
  usb_pending_=false;usb_active_=false;usb_return_=false;
  closing_=controls_visible_=ready_ui_=false;rendered_epoch_=UINT32_MAX;{std::lock_guard<std::mutex> lock(action_mutex_);action_.reset();}
  local_notice_=State().CloseFeatures()?"":"USB cleanup pending.";scenes_.clear();scenes_result_.clear();
  {LvglLockGuard lock;frame_=lv_obj_create(lv_screen_active());lv_obj_add_flag(frame_,LV_OBJ_FLAG_HIDDEN);lv_obj_set_pos(frame_,0,0);lv_obj_set_size(frame_,lv_obj_get_width(lv_screen_active()),lv_obj_get_height(lv_screen_active()));lv_obj_set_style_pad_all(frame_,0,0);lv_obj_set_style_border_width(frame_,0,0);lv_obj_remove_flag(frame_,LV_OBJ_FLAG_SCROLLABLE);
   native_root_=lv_obj_create(frame_);lv_obj_set_pos(native_root_,0,0);lv_obj_set_size(native_root_,lv_obj_get_width(frame_),lv_obj_get_height(frame_));lv_obj_set_style_pad_all(native_root_,0,0);lv_obj_set_style_border_width(native_root_,0,0);lv_obj_remove_flag(native_root_,LV_OBJ_FLAG_SCROLLABLE);
   root_=lv_obj_create(frame_);lv_obj_set_pos(root_,0,0);stock_ui::Page(root_);lv_obj_set_style_pad_all(root_,0,0);lv_obj_remove_flag(root_,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_text_font(root_,&lv_font_montserrat_16,0);
   native_actions_=lv_obj_create(frame_);lv_obj_set_pos(native_actions_,0,0);lv_obj_set_size(native_actions_,320,240);lv_obj_set_style_pad_all(native_actions_,0,0);lv_obj_set_style_bg_opa(native_actions_,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(native_actions_,0,0);lv_obj_remove_flag(native_actions_,LV_OBJ_FLAG_SCROLLABLE);
   native_controls_=lv_obj_create(frame_);lv_obj_set_pos(native_controls_,188,2);lv_obj_set_size(native_controls_,132,40);lv_obj_set_style_pad_all(native_controls_,0,0);lv_obj_set_style_bg_opa(native_controls_,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(native_controls_,0,0);lv_obj_remove_flag(native_controls_,LV_OBJ_FLAG_SCROLLABLE);
  }
  companion::OpenView(native_root_);companion::PublicStatusOwner().Set(this,[](void* owner){return static_cast<JointApp*>(owner)->PublicStatusIdle();});Render();RenderNativeControls();ShowControls(false);
  // Child builders release the LVGL lock independently. Publish only the final
  // companion mode, after the hidden feature/setup controls are ready.
  {LvglLockGuard lock;lv_obj_remove_flag(frame_,LV_OBJ_FLAG_HIDDEN);
   view::create_home_indicator([this]{closing_=true;usb_pending_=false;},
                               0xB8D3FD,0x26206A,lv_layer_top(),false);
  }
 }
 void onRunning()override{
  {LvglLockGuard lock;view::update_home_indicator();}
  if(closing_){
   if(CloseUsbView()&&CloseAll())close();
   else ShowPending("Stopping Mira and USB.\nCleanup pending; keep this page open.");
   return;
  }
  if(usb_view_){
   usb_view_->onRunning();
   if(usb_return_){
    usb_view_->onClose();usb_view_.reset();usb_return_=false;
    {LvglLockGuard lock;lv_obj_clean(native_root_);}
    companion::OpenView(native_root_);RenderNativeControls();ShowControls(false);
    {LvglLockGuard lock;lv_obj_remove_flag(frame_,LV_OBJ_FLAG_HIDDEN);}
    usb_active_=false;
   }
   return;
  }
  if(closing_){if(CloseAll())close();else ShowPending("Stopping Mira and Wi-Fi.\nCleanup pending; keep this page open.");return;}
  if(usb_pending_){
   // Join existing Mira/feature ownership before opening the USB transport.
   // Failed cleanup retains this page and retries; no second audio owner starts.
   if(!CloseAll()){ShowPending("Stopping Mira before USB.\nWaiting for confirmed cleanup.");return;}
   ClearPending();
   usb_active_=true;usb_pending_=false;
   {LvglLockGuard lock;lv_obj_add_flag(frame_,LV_OBJ_FLAG_HIDDEN);}
   usb_view_=direct_usb_pcm::MakeApp([this]{usb_return_=true;});usb_view_->onOpen();return;
  }
  auto& s=State();auto now=Now();s.link.Tick(now);std::optional<companion::FeatureUiAction> action;{std::lock_guard<std::mutex> lock(action_mutex_);action=std::move(action_);action_.reset();}if(action)Handle(*action,now);
  if(closing_){if(CloseAll())close();else ShowPending("Stopping Mira and Wi-Fi.\nCleanup pending; keep this page open.");return;
}
  if(s.usb.Active()){companion::NumericUsbRequest r;auto result=s.usb.Poll(now,r);if(result==companion::NumericUsbRead::Accepted){s.request=r;s.expires=now+120000;s.pending_wifi=companion::EnsureSavedNetwork();if(!s.pending_wifi)local_notice_="Saved Wi-Fi unavailable.";}else if(result==companion::NumericUsbRead::Rejected){s.CloseFeatures();local_notice_="Host request expired or rejected.";}}
  if(s.pending_wifi){if(now>=s.expires)s.CloseFeatures();else if(companion::IsWifiConnected()){s.pending_wifi=false;auto network=Board::GetInstance().GetNetwork();if(network){s.network_use=companion::network::Authority().BorrowSaved();if(s.network_use)s.link.StartPair(*network,s.request.request,s.request.boot_nonce,s.scopes,now,s.audio_selected);}}}
  if(s.link.Ready(now)&&s.link.AudioSelected()&&!s.audio_stage_attempted){s.audio_stage_attempted=true;if(!companion::StageFeatureAudioGrant(s.link,now))local_notice_="Audio grant could not be staged; no retry.";RenderNativeControls();}
  companion::TickView(); // Native route supervision and cleanup continue underneath overlay.
  {LvglLockGuard lock;if(!controls_visible_){if(companion::CompanionCharacterVisible())lv_obj_add_flag(native_controls_,LV_OBJ_FLAG_HIDDEN);else lv_obj_remove_flag(native_controls_,LV_OBJ_FLAG_HIDDEN);}}
  bool ready=s.link.Ready(now);if(ready!=ready_ui_||shown_code_!=s.link.Code()||shown_request_!=s.link.PairRequest()||shown_session_!=s.link.SessionId()||shown_mira_!=s.link.MiraReference()+std::to_string(s.link.MiraEpoch())){ready_ui_=ready;Render();}
  if(rendered_epoch_!=companion::FeatureAudioEpoch(s.link))RenderNativeControls();
  if(group_==2&&ready)ReadScenes();
  Update();
 }
 void onClose()override{
  closing_=true;usb_pending_=false;
  if(!CloseUsbView()||!CloseAll()){
   ShowPending("Stopping Mira and USB.\nCleanup pending; keep this page open.");
   // baseUpdate must not mark this app Sleeping and rebuild Home yet.
   open();return;
  }
  usb_active_=false;usb_return_=false;companion::PublicStatusOwner().Clear(this);
  LvglLockGuard lock;view::destroy_home_indicator();if(frame_)lv_obj_delete(frame_);
  frame_=root_=native_root_=native_controls_=native_actions_=status_=picker_=pending_panel_=pending_text_=nullptr;
  callbacks_.clear();native_callbacks_.clear();closing_=false;
 }
};
}std::unique_ptr<mooncake::AppAbility> MakeJointCompanionApp(){return std::make_unique<JointApp>();}}
