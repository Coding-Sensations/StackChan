// SPDX-License-Identifier: Apache-2.0
#include "muse_voice_task.h"
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include "muse_voice_task_owner.hpp"
#include "muse_voice_device_runtime_native.hpp"
#include "muse_direct_activity.hpp"
#include "muse_direct_live.h"
#include "muse_direct_device.h"
#include "muse_room_transport.h"
#include "provider_room_runtime.h"
#include <xplat/noise/core/MbedtlsCryptoBackend.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <sys/time.h>
#include <mutex>
namespace companion::muse {
namespace {
std::atomic<bool> visible{false},staging_broken{false};std::atomic<uint32_t> lifecycle{1};
void InvalidateStaging(){auto previous=lifecycle.load();for(;;){if(previous==UINT32_MAX){staging_broken.store(true);return;}if(lifecycle.compare_exchange_weak(previous,previous+1))return;}}
class NativeClock final:public muse_native_review::Clock {
public:
 bool Sample(muse_native_review::Stamp&out)override{timeval wall{};if(gettimeofday(&wall,nullptr)||wall.tv_sec<1704067200LL||wall.tv_sec>INT64_MAX/1000-1000)return false;out={esp_timer_get_time(),int64_t(wall.tv_sec)*1000+wall.tv_usec/1000};return out.mono_us>=0;}
};
struct Kernel {
 using Handle=TaskHandle_t;
 bool Create(void(*entry)(void*),void*context,unsigned stack,Handle&out){out=nullptr;return xTaskCreate(entry,"muse_voice",stack,context,4,&out)==pdPASS;}
 void Delay(unsigned ms){vTaskDelay(pdMS_TO_TICKS(ms));}
 void SuspendSelf(){vTaskSuspend(nullptr);}
 bool Suspended(Handle handle){
#if INCLUDE_eTaskGetState == 1
  return eTaskGetState(handle)==eSuspended;
#else
  (void)handle;return false; // no synthetic join without the real state API
#endif
 }
 void Delete(Handle handle){vTaskDelete(handle);}
};
struct Window;
class Work;
using Task=RetainedVoiceTask<Work,Kernel>;
std::mutex mutex;std::shared_ptr<Window> window;uint64_t last_generation=0;uint32_t preparing=0;
struct Preparation {
 bool active=false;
 Preparation(){std::lock_guard<std::mutex> lock(mutex);if(preparing!=UINT32_MAX){++preparing;active=true;}}
 ~Preparation(){if(active){std::lock_guard<std::mutex> lock(mutex);--preparing;}}
 explicit operator bool()const{return active;}
};
struct Window {
 VoiceAdmissionScope grant;CoreS3AudioCodec* core=nullptr;
 std::unique_ptr<ActivityCustody> activity;std::unique_ptr<CredentialLease> reader;
 Kernel kernel;std::unique_ptr<Task> task;
 std::atomic<bool> revoked{false},started{false},creation_finished{false},reaping{false},retiring{false},show{false},end_confirmed{false};
 std::atomic<VoiceTaskStage> stage{VoiceTaskStage::Prepared};std::mutex reap_mutex;
 bool Current(){
  NativeClock clock;muse_native_review::Stamp n{};
  if(staging_broken.load()||!visible.load()||revoked.load()||!reader||!reader->Current()||!room::ProviderSelected(room::Provider::Muse)||!room::MuseRoomCleanupComplete()||!clock.Sample(n)||
     n.mono_us>=grant.budget.expires_us||n.wall_ms>=grant.budget.expires_wall_ms||!grant.credential_current||!grant.network_current||!grant.voice_current)return false;
  return grant.credential_current()&&!revoked.load()&&grant.network_current()&&!revoked.load()&&grant.voice_current()&&!revoked.load()&&visible.load()&&reader->Current();
 }
 void ReleaseOriginal(){grant.credential_current={};grant.network_current={};grant.voice_current={};Wipe(grant.node);Wipe(grant.vm);Wipe(grant.call);grant.budget={};task.reset();reader.reset();activity.reset();core=nullptr;}
};
class Work final:public muse_native_review::Custody {
 Window& owner_;std::function<bool()> task_current_;NativeClock clock_;M5VoiceDevice device_;
 voice_prep::Protocol protocol_;wire::MbedtlsCryptoBackend crypto_;NativeCoreVoiceCodec codec_;
 std::unique_ptr<NativeDeviceVoiceRuntime> runtime_;lv_obj_t* surface_=nullptr;
 bool Current(){return task_current_&&task_current_()&&owner_.Current()&&task_current_();}
public:
 Work(Window&owner,std::function<bool()> current):owner_(owner),task_current_(std::move(current)),codec_(*owner.core){runtime_=CreateNativeDeviceVoiceRuntime(protocol_,*owner.reader,crypto_,clock_,*this,codec_,device_);}
 ~Work(){assert(!surface_&&(!runtime_||runtime_->CleanupComplete()));}
 bool ReadBudget(muse_native_review::Budget&out)override{out={};if(!Current())return false;out=owner_.grant.budget;return true;}
 bool Current(const muse_native_review::Budget&b)override{return b.generation==owner_.grant.budget.generation&&b.expires_us==owner_.grant.budget.expires_us&&b.expires_wall_ms==owner_.grant.budget.expires_wall_ms&&Current();}
 bool Begin(){
  if(!runtime_||!Current())return false;
  auto& ui=NativeVoiceUiAccess();if(!ui.Enter())return false;
  // Independent surface: provider app parent/callbacks may be destroyed on Home.
  surface_=lv_obj_create(lv_screen_active());lv_obj_set_pos(surface_,0,0);lv_obj_set_size(surface_,320,240);lv_obj_set_style_pad_all(surface_,0,0);lv_obj_set_style_border_width(surface_,0,0);lv_obj_set_style_bg_opa(surface_,LV_OPA_TRANSP,0);lv_obj_remove_flag(surface_,LV_OBJ_FLAG_CLICKABLE);lv_obj_remove_flag(surface_,LV_OBJ_FLAG_SCROLLABLE);lv_obj_update_layout(surface_);ui.Leave();
  if(!Current())return false;
  auto scope=owner_.grant;auto wrap=[this](std::function<bool()> original){return [this,original=std::move(original)]{return Current()&&original&&original()&&Current();};};
  scope.credential_current=wrap(scope.credential_current);scope.network_current=wrap(scope.network_current);scope.voice_current=wrap(scope.voice_current);
  return runtime_->Open(surface_,scope);
 }
 bool Poll(){
  if(!Current()){runtime_->SignalClose();return false;}
  if(owner_.show.exchange(false))(void)runtime_->ShowReview();
  const bool progress=runtime_->Poll();owner_.end_confirmed.store(runtime_->EndConfirmed());
  if(runtime_->CleanupComplete()||!progress)return false;
  owner_.stage.store(runtime_->DirectionsBound()?VoiceTaskStage::DirectionsPrepared:protocol_.Status()==voice_prep::State::Live?VoiceTaskStage::Reviewing:VoiceTaskStage::Starting);
  return true;
 }
 void SignalClose(){owner_.stage.store(VoiceTaskStage::Closing);if(runtime_)runtime_->SignalClose();}
 bool Close(){
  if(runtime_&&!runtime_->Close())return false;
  if(runtime_)owner_.end_confirmed.store(runtime_->EndConfirmed());
  runtime_.reset();
  if(surface_){auto& ui=NativeVoiceUiAccess();if(!ui.Enter())return false;lv_obj_delete(surface_);surface_=nullptr;ui.Leave();}
  return true;
 }
};
std::shared_ptr<Window> Get(){std::lock_guard<std::mutex> lock(mutex);return window&&!window->retiring.load()?window:nullptr;}
void Reap(void*context){
 std::unique_ptr<std::shared_ptr<Window>> retained(static_cast<std::shared_ptr<Window>*>(context));auto&owner=*retained;
 while(!owner->creation_finished.load())vTaskDelay(pdMS_TO_TICKS(20));
 for(;;){if(owner->revoked.load())owner->task->SignalStop();if(owner->task->TryRetire())break;vTaskDelay(pdMS_TO_TICKS(20));}
 {std::lock_guard<std::mutex> lock(mutex);owner->retiring.store(true);}
 // No new API references after retiring. Existing callback/API borrows drain
 // before original grants and activity/credential custody are released.
 while(owner.use_count()>2)vTaskDelay(pdMS_TO_TICKS(20));
 owner->ReleaseOriginal();
 {std::lock_guard<std::mutex> lock(mutex);if(window==owner)window.reset();}
 retained.reset();vTaskDelete(nullptr); // no original context remains in this dispatcher
}
bool EnsureReaper(const std::shared_ptr<Window>&owner){
 std::lock_guard<std::mutex> lock(owner->reap_mutex);if(owner->reaping.load())return true;
 auto* retained=new(std::nothrow)std::shared_ptr<Window>(owner);if(!retained)return false;owner->reaping.store(true);
 if(xTaskCreate(Reap,"muse_voice_join",3072,retained,3,nullptr)!=pdPASS){owner->reaping.store(false);delete retained;return false;}return true;
}
}
void VoiceTaskProviderVisible(bool value){if(visible.exchange(value)!=value)InvalidateStaging();if(!value)StopNativeVoiceTask();}
bool PrepareNativeVoiceTask(const VoiceAdmissionScope&grant,CoreS3AudioCodec&core){
 if(!STACKCHAN_MUSE_RAM_ONLY)return false;
 Preparation borrowed;if(!borrowed)return false;
 const auto captured_lifecycle=lifecycle.load();
 if(!STACKCHAN_MUSE_RAM_ONLY||staging_broken.load()||!visible.load()||!room::ProviderSelected(room::Provider::Muse)||!room::MuseRoomCleanupComplete()||room::MuseRoomPrepared()||!DevicePairingCleanupComplete()||!NativeSessionCleanupComplete()||
    !voice_prep::Protocol::ValidAdmissionIds(grant.call,grant.node)||!Id(grant.vm)||!grant.budget.generation||!grant.credential_current||!grant.network_current||!grant.voice_current)return false;
 NativeClock clock;muse_native_review::Stamp now{};LocalIdentity identity;
 if(!clock.Sample(now)||grant.budget.expires_us<=now.mono_us||grant.budget.expires_us-now.mono_us>120000000||grant.budget.expires_wall_ms<=now.wall_ms||grant.budget.expires_wall_ms-now.wall_ms>120000||
    !DeviceLocalPort().Identity(identity)||identity.node!=grant.node)return false;
 auto prepared=std::make_shared<Window>();prepared->grant=grant;prepared->core=&core;prepared->activity.reset(new(std::nothrow)ActivityCustody);prepared->reader.reset(new(std::nothrow)CredentialLease);
 if(!prepared->activity||!prepared->reader||!prepared->activity->Acquire()||!prepared->reader->Acquire()||!prepared->Current())return false;
 auto* captured=prepared.get();prepared->task.reset(new(std::nothrow)Task(prepared->kernel,[captured](auto current)->std::unique_ptr<Work>{if(!current()||!captured->Current())return {};return std::unique_ptr<Work>(new(std::nothrow)Work(*captured,std::move(current)));}));
 if(!prepared->task)return false;
 std::lock_guard<std::mutex> lock(mutex);if(window||staging_broken.load()||lifecycle.load()!=captured_lifecycle||!visible.load()||grant.budget.generation<=last_generation)return false;
 last_generation=grant.budget.generation;window=std::move(prepared);return true;
}
bool StartPreparedNativeVoiceTask(){
 auto owner=Get();if(!owner||owner->started.exchange(true))return false;
 owner->stage.store(VoiceTaskStage::Starting);
 if(!EnsureReaper(owner)){owner->revoked.store(true);owner->creation_finished.store(true);return false;}
 const bool started=owner->Current()&&owner->task->Start(12288);
 if(!started)owner->revoked.store(true);
 owner->creation_finished.store(true);return started;
}
bool NativeVoiceTaskPrepared(){std::lock_guard<std::mutex> lock(mutex);return window&&!window->retiring.load()&&!window->revoked.load()&&!window->started.load();}
bool QueueNativeVoiceReview(){auto owner=Get();if(!owner||!owner->started.load()||owner->revoked.load()||owner->task->Done())return false;owner->show.store(true);return true;}
VoiceTaskSnapshot ViewNativeVoiceTask(){std::lock_guard<std::mutex> lock(mutex);if(!window)return {};return {window->retiring.load()||window->revoked.load()||window->task->Done()?VoiceTaskStage::Closing:window->stage.load(),window->end_confirmed.load()};}
void StopNativeVoiceTask(){InvalidateStaging();auto owner=Get();if(!owner)return;owner->revoked.store(true);if(!owner->started.exchange(true))owner->creation_finished.store(true);(void)EnsureReaper(owner);}
void PollNativeVoiceTaskCleanup(){auto owner=Get();if(owner&&owner->started.load()&&!owner->reaping.load())(void)EnsureReaper(owner);}
bool NativeVoiceTaskCleanupComplete(){std::lock_guard<std::mutex> lock(mutex);return !window&&!preparing;}
}
#endif
