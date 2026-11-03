#pragma once
#include "local_consent.hpp"
namespace companion {class RememberedAudioWorker;namespace wake_runtime {
class LocalWakeHardwareOwner;class PhysicalSourceDispatcher;
class CapturedPhysicalGesture {
    const PhysicalSourceDispatcher* source_=nullptr;const void* control_=nullptr;
    uint64_t view_=0,transport_=0,nonce_=0,captured_=0,expires_=0,revision_=0;
    uint32_t media_=0;IntentPurpose purpose_=IntentPurpose::PhysicalListen;ProofOrigin origin_=ProofOrigin::None;
    friend class PhysicalSourceDispatcher;friend class LocalWakeHardwareOwner;
public:CapturedPhysicalGesture()=default;explicit operator bool()const noexcept{return source_&&control_&&nonce_;}
};
// Source control identity/view and exact transport/media snapshot are published
// by their existing owner. A reconnect/pair/model callback cannot construct this.
class PhysicalSourceDispatcher {
    ConsentAuthority& authority_;std::mutex mutex_;const void* control_=nullptr;
    uint64_t view_=0,transport_=0,nonce_=0,revision_=0;uint32_t media_=0;CapturedPhysicalGesture pending_;
    bool Context(const CapturedPhysicalGesture& event,uint64_t transport,uint32_t media)const noexcept{
        return event.revision_==revision_&&event.source_==this&&event.control_==control_&&event.view_==view_&&event.transport_==transport_&&event.transport_==transport&&event.media_==media_&&event.media_==media&&event.nonce_==pending_.nonce_;
    }
    CapturedPhysicalGesture Capture(const void* control,uint64_t view,IntentPurpose purpose,bool enabled,ProofOrigin origin){
        CapturedPhysicalGesture event;uint64_t now=0;if(!enabled||!authority_.Now(now)||now>UINT64_MAX-60000)return event;
        std::lock_guard<std::mutex> lock(mutex_);
        if(!control||control!=control_||view!=view_||!view_||!media_||CommonAudioAuthority().Owner()!=AudioOwner::None||nonce_==UINT64_MAX||
           (purpose!=IntentPurpose::LocalWake&&purpose!=IntentPurpose::PhysicalListen)||
           (purpose==IntentPurpose::LocalWake?transport_!=0:transport_==0))return event;
        event.revision_=revision_;event.source_=this;event.control_=control;event.view_=view_;event.transport_=transport_;event.media_=media_;event.purpose_=purpose;event.origin_=origin;event.nonce_=++nonce_;event.captured_=now;event.expires_=now+60000;pending_=event;return event;
    }
    bool Validate(const CapturedPhysicalGesture& event,uint64_t transport,uint32_t media,IntentPurpose purpose){
        uint64_t now=0;if(!authority_.Now(now))return false;
        std::lock_guard<std::mutex> lock(mutex_);
        return Context(event,transport,media)&&event.purpose_==purpose&&(event.origin_==ProofOrigin::PhysicalBoundary||event.origin_==ProofOrigin::SyntheticOffline)&&event.captured_<=now&&now-event.captured_<=500&&now<event.expires_;
    }
    bool IssueBound(const CapturedPhysicalGesture& event,const AudioLease& lease,uint64_t transport,uint32_t media,IntentPurpose purpose,ExplicitLocalConsent& proof){
        proof={};if(!Validate(event,transport,media,purpose)||!CommonAudioAuthority().Current(lease)||lease.owner()!=AudioOwner::Mira||
           (purpose==IntentPurpose::LocalWake?lease.purpose()!=AudioPurpose::LocalWake:lease.purpose()!=AudioPurpose::PhysicalListen)||lease.epoch()!=media)return false;
        LocalIntent bound;
        {std::lock_guard<std::mutex> lock(mutex_);if(!Context(event,transport,media))return false;pending_={};}
        bound.lease_=lease;bound.media_epoch_=media;bound.purpose_=purpose;bound.origin_=event.origin_;
        bound.nonce_=authority_.NextSourceNonce();bound.captured_ms_=event.captured_;bound.expires_ms_=event.expires_;
        if(!authority_.Issue(bound,proof))return false;
        {std::lock_guard<std::mutex> lock(mutex_);if(event.revision_!=revision_||event.control_!=control_||event.view_!=view_||event.transport_!=transport_||event.media_!=media_){authority_.Cancel(proof);proof={};return false;}}
        return true;
    }
    friend class LocalWakeHardwareOwner;friend class ::companion::RememberedAudioWorker;
#ifdef WAKE_FINAL_SYNTHETIC_TESTS
    friend class FinalIntegrationTestAccess;
#endif
public:
    explicit PhysicalSourceDispatcher(ConsentAuthority& authority):authority_(authority){}
    void Publish(const void* control,uint64_t view,uint64_t transport,uint32_t media){std::lock_guard<std::mutex> lock(mutex_);if(control_!=control||view_!=view||transport_!=transport||media_!=media){pending_={};if(revision_!=UINT64_MAX)++revision_;else{control=nullptr;view=0;media=0;}}control_=control;view_=view;transport_=transport;media_=media;}
    CapturedPhysicalGesture CapturePhysicalListen(const void* control,uint64_t view){return Capture(control,view,IntentPurpose::PhysicalListen,kPhysicalEventHookEnabled,ProofOrigin::PhysicalBoundary);}
    CapturedPhysicalGesture CaptureLocalWakeOptIn(const void* control,uint64_t view){return Capture(control,view,IntentPurpose::LocalWake,kLocalWakeOptInHookEnabled,ProofOrigin::PhysicalBoundary);}
};
}}
