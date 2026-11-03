#include "proximity_expression_app.h"
#include "proximity_avatar.hpp"
#include "expression_inputs.hpp"
#include "imu_expression_adapter.h"
#include <apps/led_expression/led_expression_service.h>
#include <apps/network_sensor/ltr553_idf_driver.hpp>
#include <apps/network_sensor/ltr553_proximity.hpp>
#include <apps/common/common.h>
#include <apps/common/tool_visual.hpp>
#include <assets/assets.h>
#include <esp_timer.h>
#include <lvgl.h>
#include <stackchan/avatar/skins/default/default.h>
#include <atomic>
#include <cstdio>
namespace stock_tools {
namespace {
uint64_t Now(){return static_cast<uint64_t>(esp_timer_get_time()/1000);}
using Sensor=stack_tools::Ltr553Proximity<stack_tools::Ltr553IdfDriver>;
// Only this replacement foreground factory is registered. The old sensor page
// remains uninstalled; the HAL bus and unchanged driver's restore rules remain.
Sensor& OwnedSensor(){struct Owner{stack_tools::Ltr553IdfDriver driver;Sensor sensor{driver,300,600};};static auto* owner=new Owner;return owner->sensor;}
const char* ErrorText(stack_tools::ProximityError e){using E=stack_tools::ProximityError;switch(e){case E::None:return "Ready";case E::BusUnavailable:return "Sensor unavailable";case E::ReadFailed:return "Read failed";case E::WrongDevice:return "Identity mismatch";case E::ForeignActive:return "Sensor already active";case E::ConfigureFailed:return "Setup failed";case E::RestoreFailed:return "Restore pending";}return "Unavailable";}
class ProximityExpressionApp final:public mooncake::AppAbility {
 Sensor& sensor_=OwnedSensor();proximity_expression::Gestures gestures_;proximity_expression::FaceCoordinator faces_;
 std::unique_ptr<stackchan::avatar::DefaultAvatar> avatar_;
 lv_obj_t* root_=nullptr,*info_=nullptr,*level_=nullptr,*start_=nullptr,*start_text_=nullptr;
 std::atomic<int> action_{0};bool running_=false,ir_running_=false,imu_started_=false,stop_pending_=false,closing_=false,details_=false;
 uint64_t poll_at_=0,last_event_=0;const char* event_="None";std::string shown_;proximity_expression::Face applied_=proximity_expression::Face::Neutral;bool face_applied_=false;
 void Button(const char* text,int x,int y,int w,int action){auto b=stock_ui::Button(root_,text,x,y,w,40);lv_obj_set_user_data(b,this);if(action==2){start_=b;start_text_=lv_obj_get_child(b,0);}
  lv_obj_add_event_cb(b,[](lv_event_t* e){auto* self=static_cast<ProximityExpressionApp*>(lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e))));const auto id=static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));if(id==1||id==2)self->action_=id;else{int empty=0;self->action_.compare_exchange_strong(empty,id);}},LV_EVENT_CLICKED,reinterpret_cast<void*>(intptr_t(action)));}
 void Build(){
  LvglLockGuard lock;avatar_.reset();lv_obj_clean(root_);stock_ui::Page(root_);level_=nullptr;shown_.clear();face_applied_=false;
  if(!details_){avatar_=std::make_unique<stackchan::avatar::DefaultAvatar>();avatar_->primaryColor=lv_color_hex(stock_ui::Ink);avatar_->secondaryColor=lv_color_hex(stock_ui::Canvas);avatar_->init(root_);proximity_expression::ApplyFace(*avatar_,proximity_expression::Face::Neutral);}
  Button("Back",4,4,68,1);stock_ui::Text(root_,details_?"Sensor details":"Expressions",82,12,154);Button(details_?"Face":"Details",238,4,76,4);
  if(details_){auto card=stock_ui::Card(root_,8,50,304,138);info_=stock_ui::Text(card,"",8,6,288);}
  else {level_=lv_bar_create(root_);lv_obj_set_pos(level_,8,154);lv_obj_set_size(level_,304,12);lv_bar_set_range(level_,0,100);lv_obj_set_style_bg_color(level_,lv_color_hex(stock_ui::Panel),LV_PART_MAIN);lv_obj_set_style_bg_color(level_,lv_color_hex(stock_ui::Accent),LV_PART_INDICATOR);info_=stock_ui::Text(root_,"",8,176,304,true);}
  Button(stop_pending_?"Retry Stop":running_?"Stop":"Start",4,198,108,2);Button("Calibrate clear area",118,198,196,3);
 }
 bool Stop(){running_=ir_running_=false;gestures_.Reset();faces_.Reset();led_expression::PublishExpression({},Now());event_="None";last_event_=0;stop_pending_=!sensor_.Close();return !stop_pending_;}
 void Display(){
  const auto& v=gestures_.View();char text[220];
  if(details_)std::snprintf(text,sizeof(text),"IR %s | %s\nRaw %u / smooth %u\nBaseline %u / delta %+d\nEnter +%u / leave +%u\nCalibration %u/16 | %s\nRelative IR; no cm or heading",ir_running_?"active":"stopped",ErrorText(sensor_.Error()),v.raw,v.filtered,v.baseline,v.delta,v.enter,v.leave,v.calibration_samples,imu_started_?"IMU started":"IMU unavailable");
  else if(stop_pending_)std::snprintf(text,sizeof(text),"Restore pending; Retry Stop");
  else if(!running_)std::snprintf(text,sizeof(text),"Stopped; Start then calibrate");
  else if(!ir_running_)std::snprintf(text,sizeof(text),"IR unavailable; motion cues only");
  else if(v.state==proximity_expression::State::Calibrating)std::snprintf(text,sizeof(text),"Clear area: calibration %u/16",v.calibration_samples);
  else if(!v.calibrated)std::snprintf(text,sizeof(text),"%s",v.unstable?"Unstable; recalibrate clear area":"Clear area, then Calibrate");
  else if(v.state==proximity_expression::State::NoSignal)std::snprintf(text,sizeof(text),"No fresh IR signal");
  else std::snprintf(text,sizeof(text),"%s; relative IR reflection",v.state==proximity_expression::State::Present||v.face!=proximity_expression::Face::Neutral?event_:"Clear area");
  const auto expression=running_&&!stop_pending_?faces_.Resolve(v,proximity_expression::ReadImuExpressionSample(Now()),Now()):proximity_expression::Expression{};
  if(!details_&&expression.source==proximity_expression::Source::Imu&&v.state!=proximity_expression::State::Calibrating)std::snprintf(text,sizeof(text),"Motion cue; relative IMU signal");
  led_expression::PublishExpression(expression,Now());
  LvglLockGuard lock;const auto face=expression.face;if(avatar_){if(!face_applied_||face!=applied_){proximity_expression::ApplyFace(*avatar_,face);applied_=face;face_applied_=true;}avatar_->update();}if(level_)lv_bar_set_value(level_,running_?int(proximity_expression::RelativeLevel(v)):0,LV_ANIM_OFF);if(shown_!=text){shown_=text;lv_label_set_text(info_,text);}stock_ui::Active(start_,running_&&!stop_pending_);lv_label_set_text(start_text_,stop_pending_?"Retry Stop":running_?"Stop":"Start");
 }
public:
 ProximityExpressionApp(){setAppInfo().name="PROXIMITY";static auto icon=assets::get_image("icon_controller.bin");setAppInfo().icon=&icon;}
 void onOpen()override{
  running_=ir_running_=imu_started_=false;closing_=details_=false;stop_pending_=sensor_.HasCustody();gestures_.Reset();faces_.Reset();action_=0;event_="None";last_event_=0;shown_.clear();face_applied_=false;
  {LvglLockGuard lock;root_=lv_obj_create(lv_screen_active());lv_obj_set_pos(root_,0,0);lv_obj_set_size(root_,320,240);lv_obj_set_style_pad_all(root_,0,0);lv_obj_remove_flag(root_,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_bg_color(root_,lv_color_black(),0);lv_obj_set_style_text_color(root_,lv_color_white(),0);
  }Build();Display();
 }
 void onRunning()override{
  const auto action=action_.exchange(0);const auto now=Now();
  if(action==1){closing_=true;Stop();}
  if(action==2){if(running_||stop_pending_)Stop();else{gestures_.Reset();faces_.Reset();ir_running_=sensor_.Start(now);stop_pending_=!ir_running_&&sensor_.HasCustody();imu_started_=!stop_pending_&&proximity_expression::StartImuExpressionOwner();running_=!stop_pending_&&(ir_running_||imu_started_);poll_at_=now;}}
  if(action==3&&ir_running_&&!stop_pending_){gestures_.Calibrate(now);event_="None";last_event_=0;}
  if(action==4){details_=!details_;Build();}
  if(closing_){if(!stop_pending_){close();return;}Display();return;}
  if(ir_running_&&now>=poll_at_){poll_at_=now+100;sensor_.Poll(now);const auto s=sensor_.FreshTelemetry(now,500);gestures_.Feed(s.raw,s.valid,s.saturated,s.observed_ms,now);
   const auto& v=gestures_.View();if(v.event_number!=last_event_){last_event_=v.event_number;event_=proximity_expression::EventText(v.event);}}
  gestures_.Tick(now);Display();
 }
 void onClose()override{Stop();LvglLockGuard lock;avatar_.reset();if(root_)lv_obj_delete(root_);root_=info_=level_=start_=start_text_=nullptr;action_=0;}
};
}
std::unique_ptr<mooncake::AppAbility> MakeProximityExpressionApp(){return std::make_unique<ProximityExpressionApp>();}
}
