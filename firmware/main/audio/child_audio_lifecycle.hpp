#pragma once
#include <companion/audio_owner.hpp>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <mutex>
#include <vector>

namespace checked_audio_child {
// This counts object-touching work, not merely a requested task stop. A false
// join preserves object/model/owner custody; no task is deleted by another task.
class Lifecycle {
    companion::AudioLease lease_;
    mutable std::mutex bind_mutex_;
    std::atomic<bool> accepting_{false},stop_{true};
    std::atomic<unsigned> workers_{0},operations_{0};
public:
    bool Bind(companion::AudioLease lease){
        std::lock_guard<std::mutex> lock(bind_mutex_);
        if(accepting_||workers_||operations_||!companion::CommonAudioAuthority().Current(lease)||
           (lease.owner()!=companion::AudioOwner::Stock&&lease.owner()!=companion::AudioOwner::Native))return false;
        lease_=lease;return true;
    }
    bool Authorized()const{companion::AudioLease snapshot;{std::lock_guard<std::mutex> lock(bind_mutex_);snapshot=lease_;}return companion::CommonAudioAuthority().Current(snapshot);}
    bool Resume(){if(!Authorized()||workers_||operations_)return false;stop_=false;accepting_=true;return true;}
    bool Current()const{return accepting_&&!stop_&&Authorized();}
    bool StopRequested()const{return stop_.load();}
    void RequestStop(){accepting_=false;stop_=true;}
    void WorkerStarted(){++workers_;}
    void WorkerFinished(){--workers_;} // last object access in the task wrapper
    bool OperationsJoined()const{return operations_==0;}
    bool Joined()const{return workers_==0&&operations_==0;}
    void ReserveOperation(){++operations_;}
    bool Join(uint32_t timeout_ms){
        RequestStop();const TickType_t start=xTaskGetTickCount();
        const TickType_t limit=pdMS_TO_TICKS(timeout_ms);
        while(!Joined()){
            if(!timeout_ms||TickType_t(xTaskGetTickCount()-start)>=limit)return false;
            vTaskDelay(pdMS_TO_TICKS(1)>0?pdMS_TO_TICKS(1):1);
        }
        return true;
    }
    class Operation {
        Lifecycle& life_;bool entered_;
    public:
        explicit Operation(Lifecycle& life,bool reserved=false):life_(life){if(!reserved)++life_.operations_;entered_=life_.Current();}
        ~Operation(){--life_.operations_;}
        explicit operator bool()const{return entered_;}
        Operation(const Operation&)=delete;
    };
};
template<class F> class ScopeExit {
    F f_;
public:explicit ScopeExit(F f):f_(f){}~ScopeExit(){f_();}ScopeExit(const ScopeExit&)=delete;
};
template<class F> ScopeExit<F> Finally(F f){return ScopeExit<F>(f);}
inline uint32_t Remaining(TickType_t start,uint32_t ms){const uint64_t elapsed=uint64_t(TickType_t(xTaskGetTickCount()-start))*portTICK_PERIOD_MS;return elapsed>=ms?0:ms-uint32_t(elapsed);}
template<class Predicate> bool Wait(Predicate done,TickType_t start,uint32_t ms){while(!done()){if(!Remaining(start,ms))return false;vTaskDelay(pdMS_TO_TICKS(1)>0?pdMS_TO_TICKS(1):1);}return true;}
// Vendor reset/clean has no timeout argument. Execute it on a tracked owned
// worker so caller timeout cannot turn an unfinished reset into a join receipt.
class ResetWorker {
    Lifecycle& life_;std::atomic<bool> active_{false},success_{true};
    bool(*operation_)(void*)=nullptr;void* context_=nullptr;
public:
    explicit ResetWorker(Lifecycle& life):life_(life){}
    void Dirty(){success_=false;}
    bool Success()const{return !active_&&success_;}
    bool Active()const{return active_.load();}
    bool Start(bool(*operation)(void*),void* context){
        if(!operation)return false;
        bool idle=false;if(!active_.compare_exchange_strong(idle,true))return true;
        operation_=operation;context_=context;success_=false;life_.WorkerStarted();
        if(xTaskCreate([](void* p){auto* self=static_cast<ResetWorker*>(p);self->success_=self->operation_(self->context_);self->active_=false;self->life_.WorkerFinished();vTaskDelete(nullptr);},"audio_child_reset",4096,this,3,nullptr)!=pdPASS){active_=false;life_.WorkerFinished();return false;}
        return true;
    }
};
template<class T> inline void Wipe(std::vector<T>& data){volatile T* p=data.empty()?nullptr:data.data();for(size_t i=0;i<data.size();++i)p[i]=0;data.clear();}
}
