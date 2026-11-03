// SPDX-License-Identifier: Apache-2.0
#include "muse_local_diagnostic.h"
#if defined(STACKCHAN_MUSE_LOCAL_DIAGNOSTIC) && STACKCHAN_MUSE_LOCAL_DIAGNOSTIC
#include "muse_local_diagnostic_ui.hpp"
#include <hal/hal.h>
#include <hal/board/hal_bridge.h>
#include <apps/common/common.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
namespace companion::muse::local_diag {
namespace {
class NativePort final:public Driver {
public:
 lv_indev_t*Input()const override{return GetHAL().lvTouchpad;}
 uint32_t Tick()const override{return lv_tick_get();}
 CoreS3AudioCodec&Core()override{return CoreS3AudioCodec::MuseColdOutputOnly();}
 void*Bus()override{return hal_bridge::board_get_i2c_bus();}
};
}
void RunLocalDiagnostic(){
 // Diagnostic-only HAL does not initialize NVS, RTC/network, USB, normal audio
 // or install the ordinary launcher. Default-off build excludes this TU.
 if(!GetHAL().initMuseDiagnostic())for(;;)vTaskDelay(pdMS_TO_TICKS(1000));
 static NativePort port;static Owner owner(port);static Ui ui(port);
 bool opened=false;{LvglLockGuard lock;try{opened=ui.Open(lv_screen_active());}catch(...){ui.Close();}}
 if(!opened)for(;;)vTaskDelay(pdMS_TO_TICKS(1000));
 for(;;){
  auto grant=[&]{LvglLockGuard lock;return ui.TakeRun();}();
  if(grant){
   (void)owner.Run(std::move(*grant)); // SDK operation outside LVGL lock
   char text[1024];if(!owner.Format(text,sizeof text))std::snprintf(text,sizeof text,"Diagnostic report unavailable. Keep stopped; no retry.");
   {LvglLockGuard lock;ui.Report(text);}
  }
  vTaskDelay(pdMS_TO_TICKS(20));
 }
}
}
#endif
