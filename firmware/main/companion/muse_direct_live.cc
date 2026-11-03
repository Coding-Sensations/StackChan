// SPDX-License-Identifier: Apache-2.0
#include "muse_direct_live.h"
#include "muse_direct_activity.hpp"
#include "muse_direct_credential_custody.hpp"
#include "muse_direct_native_api.h"
#include "muse_direct_native_socket.h"
#include "provider_room_runtime.h"
#include "muse_room_transport.h"
#include <xplat/noise/core/MbedtlsCryptoBackend.h>
#include <freertos/queue.h>
#include <esp_timer.h>
#include <memory>
#include <mutex>
namespace companion::muse {
namespace {
uint64_t Now(){return uint64_t(esp_timer_get_time()/1000);}
std::atomic<bool> visible{false};
struct TextPacket{char request[65]{},text[257]{};void Clear(){for(volatile char& c:request)c=0;for(volatile char& c:text)c=0;}};
struct LiveWindow {
 ActivityCustody custody;CredentialLease credentials;SessionGrant grant;QueueHandle_t queue=nullptr;TaskHandle_t worker=nullptr;
 std::atomic<bool> revoked{false},started{false},done{false},reaping{false},text_accepted{false},reply_ended{false},communicating{false},response_complete{false};
 std::atomic<SessionStage> stage{SessionStage::Prepared};std::mutex caption_mutex;std::string caption;
 bool Current(){return credentials.Current()&&visible&&!revoked&&Now()<grant.expires_ms&&room::ProviderSelected(room::Provider::Muse)&&room::MuseRoomCleanupComplete()&&grant.credential_current&&grant.network_current&&grant.chat_current&&grant.credential_current()&&grant.network_current()&&grant.chat_current();}
 void Caption(const std::string& value,bool ended){std::lock_guard<std::mutex> lock(caption_mutex);Wipe(caption);if(value.empty()){reply_ended=false;return;}if(!Current())return;caption=value.substr(0,160);reply_ended=ended;}
 ~LiveWindow(){Wipe(caption);if(queue)vQueueDelete(queue);}
};
std::mutex mutex;std::shared_ptr<LiveWindow> window;
struct WorkingSet {
 LiveWindow& window;NativeVmLookup lookup;DirectMetaSocket socket;wire::MbedtlsCryptoBackend crypto;NoiseChatOwner chat;AdmittedSession admitted;
 explicit WorkingSet(LiveWindow& value):window(value),lookup([&value]{return value.Current();}),socket([&value]{return value.Current();}),chat(socket),admitted(value.credentials,lookup,chat,crypto){}
 bool Close(){const bool chat_joined=admitted.Close();const bool lookup_joined=lookup.CloseAndJoin(20);return chat_joined&&lookup_joined;}
};
void Run(void* context){
 auto* owner=static_cast<LiveWindow*>(context);std::string finished;bool display=false;
 {
  // The working set contains >160KiB of fixed protocol/frame storage. It must
  // never be a local on the 8KiB worker stack. No I/O occurs in construction.
  std::unique_ptr<WorkingSet> work(new(std::nothrow)WorkingSet(*owner));
  const bool opened=work&&work->admitted.Open(owner->grant,Now,[owner]{return owner->Current();},[owner](const std::string& text,bool ended){owner->Caption(text,ended);});
  while(opened&&owner->Current()&&work->admitted.Poll()){
   if(work->admitted.Ready()){
    owner->stage=SessionStage::Ready;TextPacket packet{};
    if(xQueueReceive(owner->queue,&packet,0)==pdTRUE){
     std::string request(packet.request),text(packet.text);packet.Clear();
     const bool sent=work->admitted.Submit(owner->grant.generation,request,text);Wipe(request);Wipe(text);if(!sent)break;owner->communicating=true;
    }
   }
   if(owner->reply_ended)break;
   vTaskDelay(pdMS_TO_TICKS(5));
  }
  display=owner->reply_ended&&owner->Current();if(display){std::lock_guard<std::mutex> lock(owner->caption_mutex);finished=owner->caption;}
  if(!display)owner->revoked=true;
  owner->response_complete=display;owner->communicating=false;owner->stage=SessionStage::Closing;owner->Caption("",false);
  // Revoke before waiting. A slow resolver/uncertain socket close keeps the
  // worker, grant context and activity custody retained; no false receipt.
  if(work)while(!work->Close())vTaskDelay(pdMS_TO_TICKS(20));
  TextPacket empty{};while(xQueueReceive(owner->queue,&empty,0)==pdTRUE)empty.Clear();
  // One-slot queue: overwrite its retained backing bytes with zeros before it
  // is deleted by the reaper. Draining alone would leave old text in storage.
  xQueueSend(owner->queue,&empty,0);xQueueReceive(owner->queue,&empty,0);empty.Clear();
 } // Join and wipe protocol/lookup/socket locals before displaying a result.
 if(display&&owner->Current()){owner->Caption(finished,true);owner->stage=SessionStage::ReplyEnded;Wipe(finished);while(owner->Current())vTaskDelay(pdMS_TO_TICKS(20));}
 Wipe(finished);owner->revoked=true;owner->Caption("",false);
 owner->done=true;for(;;)vTaskSuspend(nullptr);
}
void Reap(void* context){
 std::unique_ptr<std::shared_ptr<LiveWindow>> retained(static_cast<std::shared_ptr<LiveWindow>*>(context));auto owner=*retained;
 while(!owner->done)vTaskDelay(pdMS_TO_TICKS(20));
 if(owner->worker)vTaskDelete(owner->worker);
 owner->worker=nullptr;{std::lock_guard<std::mutex> lock(mutex);if(window==owner)window.reset();}
 retained.reset();owner.reset();vTaskDelete(nullptr);
}
bool ReapLocked(){
 if(!window||window->reaping)return bool(window);
 auto* retained=new(std::nothrow)std::shared_ptr<LiveWindow>(window);if(!retained)return false;window->reaping=true;
 if(xTaskCreate(Reap,"muse_chat_join",3072,retained,3,nullptr)!=pdPASS){window->reaping=false;delete retained;return false;}return true;
}
}
void SessionWorkerProviderVisible(bool value){visible=value;if(!value)StopNativeSession();}
bool PrepareNativeSession(const SessionGrant& grant){
 const auto now=Now();if(!visible||!room::ProviderSelected(room::Provider::Muse)||!room::MuseRoomCleanupComplete()||room::MuseRoomPrepared()||!Id(grant.node)||!Id(grant.vm)||!Id(grant.chat)||!Id(grant.generation,64)||grant.expires_ms<=now||grant.expires_ms-now>120000||!grant.credential_current||!grant.network_current||!grant.chat_current||!grant.credential_current()||!grant.network_current()||!grant.chat_current())return false;
 std::lock_guard<std::mutex> lock(mutex);if(window)return false;auto prepared=std::make_shared<LiveWindow>();if(!prepared->custody.Acquire()||!prepared->credentials.Acquire())return false;prepared->grant=grant;window=std::move(prepared);return true;
}
bool NativeSessionPrepared(){std::lock_guard<std::mutex> lock(mutex);if(window&&!window->started&&!window->Current())window.reset();return window&&!window->started;}
bool StartPreparedNativeSession(){
 std::lock_guard<std::mutex> lock(mutex);if(!window||window->started||!window->Current()||room::MuseRoomPrepared())return false;
 window->queue=xQueueCreate(1,sizeof(TextPacket));if(!window->queue)return false;window->started=true;window->stage=SessionStage::Starting;
 if(xTaskCreate(Run,"muse_chat",8192,window.get(),4,&window->worker)!=pdPASS){window->started=false;window->stage=SessionStage::Prepared;vQueueDelete(window->queue);window->queue=nullptr;return false;}
 if(!ReapLocked()){window->revoked=true;return false;}return true;
}
bool QueueNativeText(const std::string& captured,const std::string& request,const std::string& text){
 if(!Id(request,64)||text.empty()||text.size()>256)return false;
 for(unsigned char c:text)if(c<32||c>126)return false;
 std::lock_guard<std::mutex> lock(mutex);if(!window||!window->started||!window->Current()||captured!=window->grant.generation||window->text_accepted.exchange(true))return false;
 TextPacket packet{};std::memcpy(packet.request,request.data(),request.size());std::memcpy(packet.text,text.data(),text.size());const bool sent=xQueueSend(window->queue,&packet,0)==pdTRUE;packet.Clear();return sent;
}
SessionSnapshot ViewNativeSession(){
 std::lock_guard<std::mutex> lock(mutex);SessionSnapshot result;if(!window)return result;if(!window->Current()){result.stage=SessionStage::Closing;return result;}result.stage=window->stage;std::lock_guard<std::mutex> caption_lock(window->caption_mutex);result.caption=window->caption;result.reply_ended=window->reply_ended;result.communicating=window->communicating&&!result.reply_ended;result.response_complete=window->response_complete;return result;
}
void StopNativeSession(){std::lock_guard<std::mutex> lock(mutex);if(!window)return;window->revoked=true;if(!window->started)window.reset();else if(!window->reaping)ReapLocked();}
bool NativeSessionCleanupComplete(){std::lock_guard<std::mutex> lock(mutex);return !window;}
}
namespace companion::muse {
static const SessionWorkerApi worker_api{PrepareNativeSession,StartPreparedNativeSession,QueueNativeText,ViewNativeSession,StopNativeSession,NativeSessionCleanupComplete,sizeof(WorkingSet),sizeof(LiveWindow),sizeof(TextPacket)};
}
extern "C" const companion::muse::SessionWorkerApi* stackchan_muse_direct_session_worker(){return &companion::muse::worker_api;}
