#pragma once
#include <companion/audio_owner.hpp>
#include <atomic>
#include <cstdint>
#include <limits>
#include <mutex>
namespace companion::wake_runtime {
inline constexpr bool kPhysicalEventHookEnabled=false;
inline constexpr bool kLocalWakeOptInHookEnabled=false;
inline constexpr bool kLiveBorrowedProcessingAvailable=false;
using ClockFn=uint64_t(*)(void*) noexcept;
enum class IntentPurpose { LocalWake, PhysicalListen };
enum class ProofOrigin { None, PhysicalBoundary, SyntheticOffline };
class ConsentAuthority;
class ExistingPhysicalDispatcher;
class PhysicalSourceDispatcher;
class LocalWakeHardwareOwner;
class LocalWakeController;
#ifdef WAKE_CONSENT_SYNTHETIC_TESTS
class SyntheticAccess;
#endif
class LocalIntent {
    AudioLease lease_;
    IntentPurpose purpose_=IntentPurpose::LocalWake;
    ProofOrigin origin_=ProofOrigin::None;
    uint32_t media_epoch_=0;
    uint64_t nonce_=0,captured_ms_=0,expires_ms_=0;
    friend class ConsentAuthority;friend class ExistingPhysicalDispatcher;friend class PhysicalSourceDispatcher;
#ifdef WAKE_CONSENT_SYNTHETIC_TESTS
    friend class SyntheticAccess;
#endif
public:LocalIntent()=default;
};
class ExplicitLocalConsent {
    const ConsentAuthority* authority_=nullptr;
    AudioLease lease_;
    IntentPurpose purpose_=IntentPurpose::LocalWake;
    ProofOrigin origin_=ProofOrigin::None;
    uint32_t media_epoch_=0;
    uint64_t serial_=0,expires_ms_=0;
    friend class ConsentAuthority;
public:ExplicitLocalConsent()=default;
};
class ConsentAuthority {
    ClockFn clock_=nullptr;void* clock_context_=nullptr;
    mutable std::mutex mutex_;
    uint64_t last_clock_=0,last_nonce_=0,serial_=0,next_source_nonce_=0;
    bool clock_seen_=false,clock_broken_=false,revoked_=true;
    const void* consumer_=nullptr;
    ExplicitLocalConsent held_;
    static bool Compatible(const AudioLease& lease,IntentPurpose purpose,uint32_t media){
        if(!lease||lease.owner()!=AudioOwner::Mira||!media)return false;
        if(purpose==IntentPurpose::LocalWake)return lease.purpose()==AudioPurpose::LocalWake;
        if(purpose==IntentPurpose::PhysicalListen)return lease.purpose()==AudioPurpose::PhysicalListen||lease.purpose()==AudioPurpose::Session;
        return false;
    }
    static bool SameLease(const AudioLease& a,const AudioLease& b){return a.generation()==b.generation()&&a.epoch()==b.epoch()&&a.owner()==b.owner()&&a.purpose()==b.purpose();}
    uint64_t NextSourceNonce(){std::lock_guard<std::mutex> lock(mutex_);if(next_source_nonce_<last_nonce_)next_source_nonce_=last_nonce_;if(next_source_nonce_==UINT64_MAX)return 0;return ++next_source_nonce_;}
    void CancelCurrentSource(){std::lock_guard<std::mutex> lock(mutex_);revoked_=true;}
    bool InputCurrent(const ExplicitLocalConsent& proof,const AudioLease& lease,uint32_t media,IntentPurpose purpose){const void* consumer=nullptr;{std::lock_guard<std::mutex> lock(mutex_);consumer=consumer_;}return consumer&&Check(proof,lease,media,purpose,consumer,proof.origin_);}
    bool Issue(const LocalIntent& event,ExplicitLocalConsent& out){
        out={};uint64_t now=0;
        if(!Compatible(event.lease_,event.purpose_,event.media_epoch_)||!CommonAudioAuthority().Current(event.lease_)||!Now(now)||
           !CommonAudioAuthority().Current(event.lease_)||!event.nonce_||event.captured_ms_>now||now-event.captured_ms_>500||event.expires_ms_<=now||
           event.expires_ms_-event.captured_ms_>60000||(event.origin_!=ProofOrigin::PhysicalBoundary&&event.origin_!=ProofOrigin::SyntheticOffline))return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if(clock_broken_||consumer_||event.nonce_<=last_nonce_||serial_==std::numeric_limits<uint64_t>::max())return false;
        last_nonce_=event.nonce_;held_={};held_.authority_=this;held_.lease_=event.lease_;held_.purpose_=event.purpose_;
        held_.origin_=event.origin_;held_.media_epoch_=event.media_epoch_;held_.serial_=++serial_;held_.expires_ms_=event.expires_ms_;
        revoked_=false;consumer_=nullptr;out=held_;return true;
    }
    bool CompleteCpu(const ExplicitLocalConsent& proof,const void* consumer){std::lock_guard<std::mutex> lock(mutex_);if(proof.authority_!=this||proof.serial_!=held_.serial_||consumer_!=consumer||!revoked_)return false;consumer_=nullptr;return true;}
    template<class,class>friend class BorrowedWakePipeline;
    friend class ExistingPhysicalDispatcher;friend class PhysicalSourceDispatcher;friend class LocalWakeHardwareOwner;friend class LocalWakeController;
#ifdef WAKE_CONSENT_SYNTHETIC_TESTS
    friend class SyntheticAccess;
#endif
public:
    explicit ConsentAuthority(ClockFn clock=nullptr,void* context=nullptr):clock_(clock),clock_context_(context){}
    ConsentAuthority(const ConsentAuthority&)=delete;
    bool Now(uint64_t& now){
        now=0;if(!clock_)return false;
        const uint64_t sampled=clock_(clock_context_);
        std::lock_guard<std::mutex> lock(mutex_);
        if(clock_broken_||(clock_seen_&&sampled<last_clock_)){clock_broken_=true;revoked_=true;return false;}
        clock_seen_=true;last_clock_=sampled;now=sampled;return true;
    }
    bool Consume(const ExplicitLocalConsent& proof,const AudioLease& lease,uint32_t media,IntentPurpose purpose,const void* consumer,ProofOrigin origin){
        if(!consumer||!Check(proof,lease,media,purpose,nullptr,origin))return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if(revoked_||consumer_||proof.serial_!=held_.serial_)return false;
        consumer_=consumer;return true;
    }
    bool Check(const ExplicitLocalConsent& proof,const AudioLease& lease,uint32_t media,IntentPurpose purpose,const void* consumer,ProofOrigin origin){
        uint64_t now=0;
        if(!Now(now)||!Compatible(lease,purpose,media)||!CommonAudioAuthority().Current(lease))return false;
        std::lock_guard<std::mutex> lock(mutex_);
        return !clock_broken_&&!revoked_&&proof.authority_==this&&proof.serial_&&proof.serial_==held_.serial_&&
            proof.origin_==origin&&proof.origin_==held_.origin_&&proof.purpose_==purpose&&proof.media_epoch_==media&&
            SameLease(proof.lease_,lease)&&SameLease(held_.lease_,lease)&&now<proof.expires_ms_&&
            proof.expires_ms_==held_.expires_ms_&&consumer_==consumer;
    }
    bool Cancel(const ExplicitLocalConsent& proof){std::lock_guard<std::mutex> lock(mutex_);if(proof.authority_!=this||!proof.serial_||proof.serial_!=held_.serial_)return false;revoked_=true;return true;}
    uint64_t Deadline(const ExplicitLocalConsent& proof)const{return proof.authority_==this?proof.expires_ms_:0;}
};
// Existing physical LVGL origin owns this private typed constructor. Publishing
// is metadata only. Compile-time gates remain false; no proof is issued now.
class ExistingPhysicalDispatcher {
    ConsentAuthority& authority_;
    std::mutex mutex_;
    AudioLease published_;uint32_t media_epoch_=0;
    uint64_t next_nonce_=0;
    LocalIntent pending_;
    bool Capture(IntentPurpose purpose,uint64_t stamp_ms,ProofOrigin origin,bool enabled){
        if(!enabled)return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if(!CommonAudioAuthority().Current(published_)||!media_epoch_||next_nonce_==UINT64_MAX||stamp_ms>UINT64_MAX-60000)return false;
        pending_={};pending_.lease_=published_;pending_.media_epoch_=media_epoch_;pending_.purpose_=purpose;
        pending_.origin_=origin;pending_.nonce_=authority_.NextSourceNonce();pending_.captured_ms_=stamp_ms;pending_.expires_ms_=stamp_ms+60000;return true;
    }
    bool Accept(const AudioLease& lease,uint32_t media,IntentPurpose purpose,ProofOrigin origin,bool enabled,ExplicitLocalConsent& proof){
        proof={};if(!enabled)return false;
        LocalIntent event;
        {std::lock_guard<std::mutex> lock(mutex_);
         if(!CommonAudioAuthority().Current(lease)||pending_.origin_!=origin||pending_.purpose_!=purpose||pending_.media_epoch_!=media||!ConsentAuthority::SameLease(pending_.lease_,lease))return false;
         event=pending_;pending_={};}
        return authority_.Issue(event,proof);
    }
#ifdef WAKE_CONSENT_SYNTHETIC_TESTS
    friend class SyntheticAccess;
#endif
public:
    explicit ExistingPhysicalDispatcher(ConsentAuthority& authority):authority_(authority){}
    void Publish(const AudioLease& lease,uint32_t media){std::lock_guard<std::mutex> lock(mutex_);if(!ConsentAuthority::SameLease(published_,lease)||media_epoch_!=media)pending_={};published_=lease;media_epoch_=media;}
    bool CapturePhysicalListen(uint64_t stamp_ms){return Capture(IntentPurpose::PhysicalListen,stamp_ms,ProofOrigin::PhysicalBoundary,kPhysicalEventHookEnabled);}
    bool CaptureLocalWakeOptIn(uint64_t stamp_ms){return Capture(IntentPurpose::LocalWake,stamp_ms,ProofOrigin::PhysicalBoundary,kLocalWakeOptInHookEnabled);}
    bool AcceptPhysicalListen(const AudioLease& lease,uint32_t media,ExplicitLocalConsent& proof){return Accept(lease,media,IntentPurpose::PhysicalListen,ProofOrigin::PhysicalBoundary,kPhysicalEventHookEnabled,proof);}
    bool AcceptLocalWakeOptIn(const AudioLease& lease,uint32_t media,ExplicitLocalConsent& proof){return Accept(lease,media,IntentPurpose::LocalWake,ProofOrigin::PhysicalBoundary,kLocalWakeOptInHookEnabled,proof);}
};
}
