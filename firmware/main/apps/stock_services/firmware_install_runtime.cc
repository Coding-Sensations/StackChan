#include "firmware_install_policy.h"
#include "firmware_install_ui.h"
#include "policy.h"
#include <companion/audio_owner.hpp>
#include <ssid_manager.h>
#include <apps/network_setup/wifi_credentials.hpp>
#include <nvs.h>
#include <ota.h>
#include <hal/hal.h>
#include <application.h>
#include <apps/runtime_lifecycle/session.h>
#include <lvgl.h>
#include <atomic>
#include <memory>
namespace stock_services {
namespace {
std::atomic<bool> install_allowed{false},offer_pending{false};
lv_obj_t* install_panel=nullptr;
bool ContextAllowed(){
 if(runtime_lifecycle::HomeRequested())return false;
 auto owner=companion::CurrentAudioOwner();
 if(owner==companion::AudioOwner::Stock)return true;
 auto feature=CurrentFeature();
 return owner==companion::AudioOwner::Native&&feature&&(feature==Feature::AppCenter||feature==Feature::FirmwareCheck);
}
struct Offer {std::string url,version;lv_obj_t* panel=nullptr;bool stock=false;};
void NativeInstall(void* data){
 std::unique_ptr<Offer> request(static_cast<Offer*>(data));
 auto run=[&]{
 if(runtime_lifecycle::HomeRequested())return;
 ResetFirmwareInstallUi();
 lv_obj_t* box=nullptr;lv_obj_t* label=nullptr;
 {LvglLockGuard lock;
  box=lv_obj_create(lv_layer_top());lv_obj_set_size(box,320,140);lv_obj_set_pos(box,0,48);
  install_panel=box;
  label=lv_label_create(box);lv_obj_set_width(label,280);
  lv_label_set_text(label,"Downloading replacement firmware...\nStop / Home cancels before completion.");
 }
 bool success=false;
 {FirmwareInstallScope permission;
  success=Ota::Upgrade(request->url,[label](int progress,size_t){
    if(runtime_lifecycle::HomeRequested())return;
    LvglLockGuard lock;std::string message="Installing: "+std::to_string(progress)+"%\nDevice restarts after validation.";
    lv_label_set_text(label,message.c_str());
  });
 }
 if(success&&runtime_lifecycle::CommitFirmwareInstall([]{GetHAL().reboot();return true;}))return;
 // Failed download leaves the running app selected. A new attempt requires
 // another local confirmation; all temporary permission is already revoked.
 {LvglLockGuard lock;
  lv_obj_set_height(box,130);
  lv_label_set_text(label,"Firmware install failed.\nCurrent firmware is retained.\nStop / Home, then retry.");
 }
 };
 run();
 // Free request/callback targets and revoke scoped permission before publishing
 // join. FreeRTOS task deletion does not unwind C++ automatic objects.
 request.reset();
 offer_pending=false;
 runtime_lifecycle::InstallerExited();
 vTaskDelete(nullptr);
}
void Confirm(lv_event_t* event){
 auto request=static_cast<Offer*>(lv_event_get_user_data(event));
 std::unique_ptr<Offer> owned(request);
 bool accept=lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(event)))!=nullptr;
 lv_obj_delete(request->panel);
 if(!accept){offer_pending=false;return;}
 if(!ContextAllowed()){offer_pending=false;return;}
 if(request->stock){
  auto url=request->url,version=request->version;
  Application::GetInstance().Schedule([url,version](){
    {FirmwareInstallScope permission;Application::GetInstance().UpgradeFirmware(url,version);}
    offer_pending=false;
  });
 }else{
  if(!runtime_lifecycle::InstallerStarted()){offer_pending=false;return;}
  auto raw=owned.release();
  if(xTaskCreate(NativeInstall,"stock_install",8192,raw,2,nullptr)!=pdPASS){delete raw;offer_pending=false;runtime_lifecycle::InstallerExited();}
 }
}
}
void ResetFirmwareInstallUi(){
 LvglLockGuard lock;
 if(install_panel){lv_obj_delete(install_panel);install_panel=nullptr;}
}
bool FirmwareInstallAllowed(){return install_allowed.load()&&ContextAllowed();}
void SetFirmwareInstallAllowed(bool allowed){install_allowed=allowed&&ContextAllowed();}
bool OfferFirmwareInstall(const std::string& url,const std::string& version){
 if(!ContextAllowed()||!IsFirmwareImageUrl(url))return false;
 bool expected=false;if(!offer_pending.compare_exchange_strong(expected,true))return false;
 auto request=new Offer{url,version,nullptr,companion::CurrentAudioOwner()==companion::AudioOwner::Stock};
 LvglLockGuard lock;
 request->panel=lv_obj_create(lv_layer_top());lv_obj_set_size(request->panel,320,192);lv_obj_set_pos(request->panel,0,48);
 lv_obj_set_style_pad_all(request->panel,4,0);lv_obj_remove_flag(request->panel,LV_OBJ_FLAG_SCROLLABLE);
 auto text=lv_label_create(request->panel);lv_obj_set_size(text,300,106);lv_obj_set_pos(text,6,0);
 lv_obj_set_style_text_font(text,&lv_font_montserrat_14,0);lv_label_set_long_mode(text,LV_LABEL_LONG_WRAP);
 std::string message="Replace this custom firmware?"+(version.empty()?std::string{}:"\nVersion: "+version.substr(0,40))+"\nCompanion and Tools will be removed.\nRestarts after installation.";
 if(url.rfind("http://",0)==0)message+="\nUnencrypted download (HTTP).";
 lv_label_set_text(text,message.c_str());
 auto source=lv_label_create(request->panel);lv_obj_set_size(source,300,24);lv_obj_set_pos(source,6,110);
 lv_obj_set_style_text_font(source,&lv_font_montserrat_14,0);lv_label_set_long_mode(source,LV_LABEL_LONG_SCROLL_CIRCULAR);
 auto visible_url=url.substr(0,url.find('?'));lv_label_set_text(source,("Source: "+visible_url).c_str());
 for(int i=0;i<2;++i){auto button=lv_button_create(request->panel);lv_obj_set_size(button,146,44);lv_obj_set_pos(button,4+i*152,140);
  lv_obj_set_user_data(button,i?reinterpret_cast<void*>(1):nullptr);auto label=lv_label_create(button);lv_label_set_text(label,i?"Install & restart":"Cancel");lv_obj_center(label);
  lv_obj_add_event_cb(button,Confirm,LV_EVENT_CLICKED,request);
 }
 return true;
}
}
extern "C" bool stackchan_stock_install_firmware(const char* url,void(*progress)(int)){
 if(!url||!stock_services::FirmwareInstallAllowed()||!stock_services::IsFirmwareImageUrl(url))return false;
 bool result=Ota::Upgrade(url,[progress](int value,size_t){if(progress)progress(value);});
 if(result)runtime_lifecycle::CommitFirmwareInstall([]{GetHAL().reboot();return true;});
 return false;
}
