#include "usb_pcm_app.h"
#include "usb_pcm_actor.h"
#include "usb_pcm_selection.h"
#include <apps/common/tool_visual.hpp>
#include <apps/common/common.h>
#include <assets/assets.h>
#include <lvgl.h>
#include <atomic>
#include <mutex>
namespace direct_usb_pcm {
class UsbPcmApp final:public Page {
 std::function<void()> returned_;
 lv_obj_t* root_=nullptr;lv_obj_t* status_=nullptr;lv_obj_t* code_=nullptr;
 std::mutex click_lock_;std::string clicked_code_,clicked_stamp_;
 std::atomic<int> action_{0};std::string displayed_,stamp_;bool closing_=false;
 void Button(const char* name,int x,int y,int w,int id){auto b=stock_ui::Button(root_,name,x,y,w,40);lv_obj_set_user_data(b,reinterpret_cast<void*>(intptr_t(id)));lv_obj_add_event_cb(b,[](lv_event_t* e){auto self=static_cast<UsbPcmApp*>(lv_event_get_user_data(e));int id=int(intptr_t(lv_obj_get_user_data(lv_event_get_target_obj(e))));if(id==2){std::lock_guard<std::mutex> held(self->click_lock_);self->clicked_code_=self->displayed_;self->clicked_stamp_=self->stamp_;}self->action_=id;},LV_EVENT_CLICKED,this);}
public:
 explicit UsbPcmApp(std::function<void()> returned):returned_(std::move(returned)){setAppInfo().name="USB MIRA";static auto icon=assets::get_image("icon_dance.bin");setAppInfo().icon=&icon;}
 void onOpen()override{StageCompiledSelection();LvglLockGuard lock;root_=lv_obj_create(lv_screen_active());stock_ui::Page(root_);stock_ui::Text(root_,"USB Mira",8,10,210);Button("Back",250,2,62,4);auto scope=stock_ui::Text(root_,"Share mic & speaker for a 60s trial",8,50,304,true);lv_obj_set_style_text_font(scope,&lv_font_montserrat_14,0);auto call=stock_ui::Text(root_,"Keep original Mira call open on Mac",8,70,304,true);lv_obj_set_style_text_font(call,&lv_font_montserrat_14,0);code_=stock_ui::Text(root_,"No pairing code",8,94,300);status_=stock_ui::Text(root_,"USB trust unavailable; request a configured firmware build",8,132,300,true);lv_obj_set_style_text_font(status_,&lv_font_montserrat_14,0);Button("Connect",8,182,82,1);Button("Match + Listen",98,182,140,2);Button("Stop",246,182,66,3);closing_=false;}
 bool RequestClose()override{closing_=true;StopFromPage();return ReadView().closed;}
 void onRunning()override{
  auto v=ReadView();int a=action_.exchange(0);if(a==1&&!closing_)SelectFromPage();
  if(a==2&&!closing_){std::lock_guard<std::mutex> held(click_lock_);ConfirmFromPage(clicked_code_,clicked_stamp_);clicked_code_.clear();clicked_stamp_.clear();}
  if(a==4)closing_=true;
  if(a==3||closing_)StopFromPage();
  if(closing_&&ReadView().closed){if(returned_)returned_();else close();return;}
  const char* status=v.issue==SetupIssue::ClockNotReady?"Trusted device clock is not ready":v.issue==SetupIssue::TrustUnavailable?"Public trust is unavailable":!v.configured?"USB trust unavailable; request a configured firmware build":v.state==RuntimeState::Provisioning?"Waiting for computer USB request":v.state==RuntimeState::Tls?"Verifying trusted computer":v.state==RuntimeState::Challenge?"Match this code on your computer":v.state==RuntimeState::Confirmed||v.state==RuntimeState::WaitingReady?"Waiting for confirmed computer":v.state==RuntimeState::Media?"Original call must be open on Mac":v.state==RuntimeState::Closing?"Stop pending; ownership retained":v.state==RuntimeState::Fault?"Connection failed; no retry started":"Stopped; choose Connect";
  LvglLockGuard lock;displayed_=v.code;stamp_=v.stamp;lv_label_set_text(code_,displayed_.empty()?"No pairing code":displayed_.c_str());lv_label_set_text(status_,status);
 }
 void onClose()override{StopFromPage();LvglLockGuard lock;if(root_)lv_obj_delete(root_);root_=status_=code_=nullptr;displayed_.clear();stamp_.clear();action_=0;}
};
std::unique_ptr<Page> MakeApp(std::function<void()> returned){return std::make_unique<UsbPcmApp>(std::move(returned));}
}
