#pragma once
#include "local_consent.hpp"
#include <array>
#include <cstddef>
namespace companion::wake_runtime {
// Owner-thread-only lifecycle. Providers and this object outlive synchronous
// callbacks; no concurrent Feed/Close/destruction. Reentrant calls are denied.
// This CPU borrower NEVER acquires, revokes or releases the hardware lease.
template<class Front,class Model> class BorrowedWakePipeline {
    ConsentAuthority& authority_;Front* front_;Model* model_;
    AudioLease lease_;ExplicitLocalConsent proof_;uint32_t media_=0;
    IntentPurpose purpose_=IntentPurpose::LocalWake;ProofOrigin origin_=ProofOrigin::None;
    uint64_t sequence_=0;bool busy_=false,held_=false,ready_=false,cleanup_=false;
    template<class T,size_t N>static void Wipe(std::array<T,N>& values)noexcept{volatile unsigned char* bytes=reinterpret_cast<volatile unsigned char*>(values.data());for(size_t i=0;i<sizeof(values);++i)bytes[i]=0;}
    struct Busy {bool& value;explicit Busy(bool& v):value(v){value=true;}~Busy(){value=false;}};
    bool Matches(const AudioLease& lease,uint32_t media)const noexcept {
        return held_&&media==media_&&lease.generation()==lease_.generation()&&lease.epoch()==lease_.epoch()&&lease.owner()==lease_.owner()&&lease.purpose()==lease_.purpose()&&CommonAudioAuthority().Holds(lease);
    }
    bool Current(){return ready_&&!cleanup_&&authority_.Check(proof_,lease_,media_,purpose_,this,origin_)&&CommonAudioAuthority().Current(lease_);}
    bool Begin(const AudioLease& lease,uint32_t media,IntentPurpose purpose,const ExplicitLocalConsent& proof,ProofOrigin origin){
        if(busy_||held_||!front_||!model_||!authority_.Consume(proof,lease,media,purpose,this,origin))return false;
        Busy operation(busy_);held_=true;ready_=true;lease_=lease;media_=media;proof_=proof;purpose_=purpose;origin_=origin;sequence_=0;
        if(!Current()||!front_->Configure()||!Current()||!model_->Configure(lease_.generation(),authority_.Deadline(proof_))||!Current()) {ready_=false;cleanup_=true;authority_.Cancel(proof_);return false;}
        return true;
    }
#ifdef WAKE_CONSENT_SYNTHETIC_TESTS
    friend class SyntheticAccess;
#endif
public:
    BorrowedWakePipeline(ConsentAuthority& authority,Front* front=nullptr,Model* model=nullptr):authority_(authority),front_(front),model_(model){}
    BorrowedWakePipeline(const BorrowedWakePipeline&)=delete;
    bool BeginLive(const AudioLease& lease,uint32_t media,IntentPurpose purpose,const ExplicitLocalConsent& proof){
        if(!kLiveBorrowedProcessingAvailable)return false;
        return Begin(lease,media,purpose,proof,ProofOrigin::PhysicalBoundary);
    }
    bool CleanupPending()const noexcept{return held_&&!ready_;}
    bool Held()const noexcept{return held_;}
    bool Reset(const AudioLease& lease,uint32_t media){
        if(busy_||!Matches(lease,media))return false;
        Busy operation(busy_);
        if(!Current()||!front_->Reset()||!Current()||!model_->Reset(lease_.generation(),authority_.Deadline(proof_))||!Current()){ready_=false;cleanup_=true;authority_.Cancel(proof_);return false;}
        // Sequence stays monotonic across recurrent-state reset to deny replay.
        return true;
    }
    bool Close(const AudioLease& lease,uint32_t media){
        if(!Matches(lease,media))return false;
        if(busy_){ready_=false;cleanup_=true;authority_.Cancel(proof_);return false;}
        Busy operation(busy_);
        ready_=false;cleanup_=true;authority_.Cancel(proof_);
        // Cleanup remains allowed after expiry/revocation and never releases IO.
        // Producer MUST retain Common.Holds until this CPU cleanup succeeds.
        const bool front_closed=front_->Close();
        const bool model_closed=model_->Close(lease_.generation(),authority_.Deadline(proof_));
        if(!front_closed||!model_closed||!authority_.CompleteCpu(proof_,this))return false;
        lease_={};proof_={};media_=0;sequence_=0;held_=cleanup_=false;return true;
    }
    template<class Output> bool FeedBorrowedPcm(const AudioLease& lease,uint32_t media,uint64_t sequence,const int16_t* pcm,size_t count,Output&& output){
        static_assert(noexcept(output(uint8_t{})),"borrowed diagnostic consumer must be noexcept");
        if(busy_||!Matches(lease,media)||!sequence||sequence<=sequence_||!pcm||!count||count>512)return false;
        Busy operation(busy_);sequence_=sequence;
        bool valid=Current();std::array<uint8_t,4> scores{};size_t score_count=0;
        if(valid)valid=front_->Feed(pcm,count,[&](const std::array<uint16_t,40>& raw)noexcept->bool{
            if(!valid||!Current()){valid=false;return false;}
            std::array<int8_t,120> features{};
            const bool packed=front_->Pack(raw,features);
            if(!Current()){valid=false;Wipe(features);return false;}
            if(packed){uint8_t score=0;
                if(score_count==scores.size()||!model_->Invoke(features.data(),features.size(),lease_.generation(),authority_.Deadline(proof_),score)||!Current())valid=false;
                else scores[score_count++]=score;
            }
            Wipe(features);return valid;
        })&&valid;
        if(valid)valid=Current();
        // Output is an unqualified diagnostic byte, never a wake/listen action.
        for(size_t i=0;valid&&i<score_count;++i){if(!Current()){valid=false;break;}output(scores[i]);valid=Current();}
        Wipe(scores);
        if(!valid){ready_=false;cleanup_=true;authority_.Cancel(proof_);}return valid;
    }
};
#ifdef WAKE_CONSENT_SYNTHETIC_TESTS
// Absent from production: issues ONLY SyntheticOffline proofs, which BeginLive
// rejects even if a future live availability gate is enabled.
class SyntheticAccess {
public:
    static bool Issue(ConsentAuthority& authority,const AudioLease& lease,uint32_t media,IntentPurpose purpose,uint64_t nonce,uint64_t captured,uint64_t expiry,ExplicitLocalConsent& proof){LocalIntent intent;intent.lease_=lease;intent.media_epoch_=media;intent.purpose_=purpose;intent.origin_=ProofOrigin::SyntheticOffline;intent.nonce_=nonce;intent.captured_ms_=captured;intent.expires_ms_=expiry;return authority.Issue(intent,proof);}
    template<class P>static bool Begin(P& pipe,const AudioLease& lease,uint32_t media,IntentPurpose purpose,const ExplicitLocalConsent& proof){return pipe.Begin(lease,media,purpose,proof,ProofOrigin::SyntheticOffline);}
    static bool Capture(ExistingPhysicalDispatcher& dispatcher,IntentPurpose purpose,uint64_t stamp){return dispatcher.Capture(purpose,stamp,ProofOrigin::SyntheticOffline,true);}
    static bool Accept(ExistingPhysicalDispatcher& dispatcher,const AudioLease& lease,uint32_t media,IntentPurpose purpose,ExplicitLocalConsent& proof){return dispatcher.Accept(lease,media,purpose,ProofOrigin::SyntheticOffline,true,proof);}
};
#endif
}
