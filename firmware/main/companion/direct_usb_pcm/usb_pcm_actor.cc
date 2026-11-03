#include "usb_pcm_actor.h"
#include <board.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mutex>
namespace direct_usb_pcm {
namespace {
std::mutex lock;Selection selected;View view;enum class Command {None,Select,Confirm,Stop};Command command=Command::None;std::string code,stamp;TaskHandle_t task=nullptr;
// Retained boot lifetime. Uncertain driver/codec close never frees this state.
Runtime* runtime=nullptr;
void Run(void*){
 for(;;){Command action;Selection selection;std::string displayed,context;
  {std::lock_guard<std::mutex> held(lock);action=command;command=Command::None;selection=selected;displayed=code;context=stamp;}
  if(action==Command::Select)runtime->PrepareSelected(selection.boot,selection.epoch,selection.stream,selection.cutoff);
  if(action==Command::Confirm)runtime->ConfirmDisplayedCode(displayed,context);
  if(action==Command::Stop)runtime->Stop();
  runtime->Tick();View next;next.state=runtime->State();next.issue=runtime->Issue();next.code=runtime->Code();next.stamp=runtime->ConsentStamp();next.audio=runtime->Audio();next.closed=runtime->Closed();
  {std::lock_guard<std::mutex> held(lock);
   // Select may have arrived while this iteration stopped the previous session.
   // Its false receipt must survive until the sequential actor consumes it.
   next.closed=next.closed&&command==Command::None;
   next.configured=selected.epoch&&selected.stream;view=std::move(next);}
  vTaskDelay(pdMS_TO_TICKS(10));
 }
}
bool Ensure(){if(task)return true;if(!runtime)runtime=new Runtime([]{return uint64_t(esp_timer_get_time()/1000);},[]{return Board::GetInstance().GetAudioCodec();});return xTaskCreatePinnedToCore(Run,"usb_pcm_owner",8192,nullptr,1,&task,0)==pdPASS;}
}
bool StageSelection(const Selection& s){std::lock_guard<std::mutex> held(lock);if(command!=Command::None||!view.closed||!s.epoch||!s.stream||s.cutoff<=uint64_t(esp_timer_get_time()/1000))return false;selected=s;view.configured=true;view.issue=SetupIssue::None;return true;}
void ReportSetupIssue(SetupIssue issue){std::lock_guard<std::mutex> held(lock);if(!task&&view.closed)view.issue=issue;}
void SelectFromPage(){std::lock_guard<std::mutex> held(lock);if(view.configured&&command==Command::None&&Ensure()){view.closed=false;command=Command::Select;}}
void ConfirmFromPage(const std::string& displayed,const std::string& context){std::lock_guard<std::mutex> held(lock);if(command==Command::None&&task){code=displayed;stamp=context;command=Command::Confirm;}}
void StopFromPage(){std::lock_guard<std::mutex> held(lock);if(task)command=Command::Stop;}
View ReadView(){std::lock_guard<std::mutex> held(lock);return view;}
}
