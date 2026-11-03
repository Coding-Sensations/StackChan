#include "sd_status_app.h"
#include "stackchan_sd_ir.h"
#include <apps/common/common.h>
#include <apps/common/tool_visual.hpp>
#include <assets/assets.h>
#include <lvgl.h>
#include <atomic>
#include <cstdio>
#include <string>
namespace stock_tools {
class SdStatusApp final:public mooncake::AppAbility {
 lv_obj_t* root_=nullptr;lv_obj_t* body_=nullptr;lv_obj_t* labels_[3]{};std::string rendered_[3];std::atomic<bool> back_{false};
 lv_obj_t* Section(const char* name,int y,int height){auto card=stock_ui::Card(body_,0,y,304,height);auto title=stock_ui::Text(card,name,8,8,288);lv_obj_set_style_text_color(title,lv_color_hex(stock_ui::Accent),0);auto value=stock_ui::Text(card,"",8,32,288);lv_obj_set_style_text_font(value,&lv_font_montserrat_14,0);return value;}
 void Set(unsigned index,const char* value){if(rendered_[index]==value)return;lv_label_set_text(labels_[index],value);rendered_[index]=value;}
 void Render(){const auto d=ReadStackChanSdDiagnostic();const auto& b=d.boot;const auto& c=b.last;const auto& f=b.last_failed;const auto& e=b.electrical;char status[256],boot[384],electrical[320];
  std::snprintf(status,sizeof(status),"%s\nStage %u / error 0x%08x / errno %d\nCached boot evidence; no retry",d.mounted?(d.display_fault?"Mounted / display fault":"Mounted / storage available"):d.mount_attempted?"Not mounted":"Initialization not attempted",unsigned(d.stage),unsigned(uint32_t(d.code)),d.filesystem_errno);
  std::snprintf(boot,sizeof(boot),"Phase: %s\nInit %s / result 0x%08x\nFAT %s / FRESULT %ld\nLast CMD %s%lu / timeout %lu ms\nTransport 0x%x / command 0x%x\nPrior fail CMD %s%lu / %lu ms\nError 0x%x; may be tolerated",SdPhaseText(b.phase),b.init_seen?"seen":"unseen",unsigned(uint32_t(b.init_result)),b.fat_seen?"seen":"unseen",long(b.fat_result),c.seen?"":"unknown ",static_cast<unsigned long>(c.opcode),static_cast<unsigned long>(c.timeout_ms),unsigned(uint32_t(c.transport_error)),unsigned(uint32_t(c.command_error)),f.seen?"":"unknown ",static_cast<unsigned long>(f.opcode),static_cast<unsigned long>(f.timeout_ms),unsigned(uint32_t(f.transport_error?f.transport_error:f.command_error)));
  if(!e.sampled)std::snprintf(electrical,sizeof(electrical),"TF switch: unknown\nAW P0: not sampled\nPMIC 0x90 / 0x95: not sampled\nDisplay bound %s / fault %s\nNo register query from this page\nNot a rail measurement",d.display_bound?"yes":"no",d.display_fault?"yes":"no");
  else std::snprintf(electrical,sizeof(electrical),"TF switch: %s (AW P0 bit 4)\nAW P0 0x%02x / read 0x%x\nPMIC 0x90: 0x%02x / read 0x%x\nPMIC 0x95: 0x%02x / read 0x%x\nDisplay bound %s / fault %s\nRaw register cache only\nNot a rail measurement",!e.sampled||e.switch_error?"unknown":(e.p0&0x10)?"high":"low",e.p0,unsigned(uint32_t(e.switch_error)),e.control,unsigned(uint32_t(e.control_error)),e.voltage,unsigned(uint32_t(e.voltage_error)),d.display_bound?"yes":"no",d.display_fault?"yes":"no");
  LvglLockGuard lock;Set(0,status);Set(1,boot);Set(2,electrical);
 }
public:
 SdStatusApp(){setAppInfo().name="SD STATUS";static auto icon=assets::get_image("icon_controller.bin");setAppInfo().icon=&icon;}
 void onOpen()override{{LvglLockGuard lock;root_=lv_obj_create(lv_screen_active());stock_ui::Page(root_);lv_obj_set_pos(root_,0,0);auto back=stock_ui::Button(root_,"Back",8,4,84,44);lv_obj_add_event_cb(back,[](lv_event_t* e){static_cast<SdStatusApp*>(lv_event_get_user_data(e))->back_=true;},LV_EVENT_CLICKED,this);stock_ui::Text(root_,"SD status",104,8,208);auto hint=stock_ui::Text(root_,"Scroll for all evidence",104,28,208,true);lv_obj_set_style_text_font(hint,&lv_font_montserrat_14,0);
   body_=lv_obj_create(root_);lv_obj_set_pos(body_,8,52);lv_obj_set_size(body_,304,180);lv_obj_set_style_pad_all(body_,0,0);lv_obj_set_style_border_width(body_,0,0);lv_obj_set_style_bg_color(body_,lv_color_hex(stock_ui::Canvas),0);lv_obj_add_flag(body_,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(body_,LV_DIR_VER);lv_obj_set_scrollbar_mode(body_,LV_SCROLLBAR_MODE_AUTO);
   labels_[0]=Section("CARD STATUS",0,104);labels_[1]=Section("BOOT / COMMANDS",112,176);labels_[2]=Section("CARD / POWER",296,176);for(auto& text:rendered_)text.clear();back_=false;}
  Render();
 }
 void onRunning()override{if(back_.exchange(false)){close();return;}Render();}
 void onClose()override{LvglLockGuard lock;if(root_)lv_obj_delete(root_);root_=body_=nullptr;for(auto& label:labels_)label=nullptr;for(auto& text:rendered_)text.clear();back_=false;}
};
std::unique_ptr<mooncake::AppAbility> MakeSdStatusApp(){return std::make_unique<SdStatusApp>();}
}
