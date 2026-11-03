#pragma once
#include "borrowed_cpu_endpoint.hpp"
#include "borrowed_wake_pipeline.hpp"
#include "native_ports.hpp"
namespace companion::wake_runtime {
// Owner-placed, inert native pipeline. No producer installs this today.
// Asset/clock/provider lifetime must extend through producer checked Stop.
class CheckedNativeEndpoint final:public BorrowedCpuEndpoint {
    CanonicalFrontPort front_;FixedModelPort model_;
    BorrowedWakePipeline<CanonicalFrontPort,FixedModelPort> pipeline_;
    AudioLease lease_;uint32_t media_=0;uint64_t sequence_=0;
public:
    CheckedNativeEndpoint(ConsentAuthority& authority,ClockFn clock,void* context,const uint8_t* asset=nullptr,size_t bytes=0):model_(clock,context,asset,bytes),pipeline_(authority,&front_,&model_){}
    bool AcceptPhysicalProof(AudioLease lease,uint32_t media,ExplicitLocalConsent proof)noexcept override{
        if(pipeline_.Held()||!kNativePortsLiveAvailable)return false;
        lease_=lease;media_=media;sequence_=0;
        return pipeline_.BeginLive(lease,media,lease.purpose()==AudioPurpose::LocalWake?IntentPurpose::LocalWake:IntentPurpose::PhysicalListen,proof);
    }
    bool Feed16k(AudioLease lease,uint32_t media,const int16_t* pcm,size_t count)noexcept override{
        if(!kNativePortsLiveAvailable||sequence_==UINT64_MAX)return false;
        // Diagnostics intentionally discarded; scores never dispatch Listen.
        return pipeline_.FeedBorrowedPcm(lease,media,++sequence_,pcm,count,[](uint8_t)noexcept{});
    }
    bool CloseCpu()noexcept override{if(!pipeline_.Held())return true;return pipeline_.Close(lease_,media_);}
};
}
