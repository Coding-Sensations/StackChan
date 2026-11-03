#pragma once
#include "local_wake_hardware_owner.hpp"
#include <freertos/task.h>
#include <memory>
#include <new>
namespace companion::wake_runtime {
// Boot-lifetime controller: UI publishes a private gesture, only the dedicated
// owner worker may allocate/read/close. Stop timeout retains task/object custody.
class LocalWakeController {
    ConsentAuthority& authority_;PhysicalSourceDispatcher dispatcher_;ClockFn clock_;void* clock_context_;
    AudioCodec*(*codec_)()noexcept=nullptr;
    std::mutex control_;CapturedPhysicalGesture pending_;
    std::unique_ptr<LocalWakeHardwareOwner> owner_;
    std::atomic<bool> done_{true},stop_{false};bool stopping_=false;TaskHandle_t task_=nullptr;
    void Run(){
        CapturedPhysicalGesture gesture;{std::lock_guard<std::mutex> lock(control_);gesture=pending_;pending_={};}
        auto* codec=codec_?codec_():nullptr;
        if(codec)owner_.reset(new(std::nothrow) LocalWakeHardwareOwner(*codec,authority_,dispatcher_,clock_,clock_context_));
        if(owner_&&owner_->Begin(gesture))while(!stop_&&owner_->ReadOne()){}
        while(owner_&&!owner_->Close())vTaskDelay(pdMS_TO_TICKS(10));
    }
public:
    LocalWakeController(ConsentAuthority& authority,ClockFn clock,void* context,AudioCodec*(*codec)()noexcept):authority_(authority),dispatcher_(authority),clock_(clock),clock_context_(context),codec_(codec){}
    void PublishControl(const void* control,uint64_t view){dispatcher_.Publish(control,view,0,view<=UINT32_MAX?static_cast<uint32_t>(view):0);}
    bool QueueExplicitLocalWakeOptIn(const void* control,uint64_t view){
        auto gesture=dispatcher_.CaptureLocalWakeOptIn(control,view);if(!gesture)return false;
        std::lock_guard<std::mutex> lock(control_);if(!done_||stopping_||pending_)return false;pending_=gesture;return true;
    }
    bool StartPending(){
        if(!kLocalWakeHardwareRouteReady||!kLocalWakeOptInHookEnabled||!QualifiedAssetView().qualification)return false;
        std::lock_guard<std::mutex> lock(control_);if(!done_||stopping_||!pending_)return false;stop_=false;done_=false;
        if(xTaskCreate([](void* p){auto* self=static_cast<LocalWakeController*>(p);self->Run();self->done_.store(true);vTaskDelete(nullptr);},"local_wake_owner",8192,this,3,&task_)!=pdPASS){done_=true;pending_={};task_=nullptr;return false;}return true;
    }
    bool Stop(uint32_t timeout_ms=2000){
        const TickType_t start=xTaskGetTickCount();{std::lock_guard<std::mutex> lock(control_);stopping_=true;stop_=true;pending_={};dispatcher_.Publish(nullptr,0,0,0);authority_.CancelCurrentSource();}
        while(!done_&&uint64_t(TickType_t(xTaskGetTickCount()-start))*portTICK_PERIOD_MS<timeout_ms)vTaskDelay(pdMS_TO_TICKS(1)>0?pdMS_TO_TICKS(1):1);
        std::lock_guard<std::mutex> lock(control_);if(done_)stopping_=false;return done_;
    }
    bool Stopped()const noexcept{return done_;}
};
}
