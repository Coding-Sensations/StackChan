#pragma once
#include <cstdint>
#include <limits>
#include <mutex>
namespace companion {
enum class AudioOwner { None, Mira, Stock, Native, DirectUsb, Muse };
enum class AudioPurpose { Session, StockRuntime, NativeRuntime, DirectUsbSession, LocalWake, PhysicalListen, MusePushToTalk };
// Custody identity, not consent: pairing/reconnect or reservation never authorizes capture.
class AudioLease {
    const void* authority_=nullptr;
    AudioOwner owner_=AudioOwner::None;
    AudioPurpose purpose_=AudioPurpose::Session;
    uint64_t generation_=0,epoch_=0;
    friend class AudioOwnerAuthority;
public:
    explicit operator bool()const noexcept{return generation_!=0;}
    uint64_t generation()const noexcept{return generation_;}
    uint64_t epoch()const noexcept{return epoch_;}
    AudioOwner owner()const noexcept{return owner_;}
    AudioPurpose purpose()const noexcept{return purpose_;}
};
class AudioOwnerAuthority {
    mutable std::mutex mutex_;
    AudioLease held_{};
    uint64_t next_=0;
    bool revoked_=false;
    bool Match(const AudioLease& l)const {
        return l.authority_==this&&l.generation_&&l.generation_==held_.generation_&&l.epoch_==held_.epoch_&&
            l.owner_==held_.owner_&&l.purpose_==held_.purpose_;
    }
public:
    AudioOwnerAuthority()=default;
    AudioOwnerAuthority(const AudioOwnerAuthority&)=delete;
    bool Acquire(AudioOwner owner,AudioPurpose purpose,uint64_t epoch,AudioLease& out){
        std::lock_guard<std::mutex> lock(mutex_);
        if(out||held_||owner==AudioOwner::None||!epoch||next_==std::numeric_limits<uint64_t>::max())return false;
        const bool valid=(owner==AudioOwner::Mira&&(purpose==AudioPurpose::Session||purpose==AudioPurpose::LocalWake||purpose==AudioPurpose::PhysicalListen))||
            (owner==AudioOwner::Stock&&purpose==AudioPurpose::StockRuntime)||
            (owner==AudioOwner::Native&&purpose==AudioPurpose::NativeRuntime)||
            (owner==AudioOwner::DirectUsb&&purpose==AudioPurpose::DirectUsbSession)||
            (owner==AudioOwner::Muse&&purpose==AudioPurpose::MusePushToTalk);
        if(!valid)return false;
        held_.authority_=this;held_.owner_=owner;held_.purpose_=purpose;held_.epoch_=epoch;held_.generation_=++next_;
        revoked_=false;out=held_;return true;
    }
    bool Holds(const AudioLease& l)const{std::lock_guard<std::mutex> lock(mutex_);return Match(l);}
    bool Current(const AudioLease& l)const{std::lock_guard<std::mutex> lock(mutex_);return Match(l)&&!revoked_;}
    bool Revoke(const AudioLease& l){std::lock_guard<std::mutex> lock(mutex_);if(!Match(l))return false;revoked_=true;return true;}
    // Receipt must cover this owner's hardware directions and worker join.
    // Failed closure preserves revoked custody and blocks every next acquire.
    bool Release(AudioLease& l,bool checked_closed_and_joined){
        std::lock_guard<std::mutex> lock(mutex_);if(!Match(l))return false;
        revoked_=true;if(!checked_closed_and_joined)return false;
        held_={};l={};return true;
    }
    AudioOwner Owner()const{std::lock_guard<std::mutex> lock(mutex_);return held_.owner_;}
};
inline AudioOwnerAuthority& CommonAudioAuthority(){static AudioOwnerAuthority authority;return authority;}
inline bool AcquireAudio(AudioOwner owner,AudioPurpose purpose,uint64_t epoch,AudioLease& out){return CommonAudioAuthority().Acquire(owner,purpose,epoch,out);}
inline bool RevokeAudio(const AudioLease& l){return CommonAudioAuthority().Revoke(l);}
inline bool ReleaseAudio(AudioLease& l,bool closed_and_joined){return CommonAudioAuthority().Release(l,closed_and_joined);}
inline AudioOwner CurrentAudioOwner(){return CommonAudioAuthority().Owner();}
// No enum-only release or public CAS remains: unmigrated source fails compilation.
}
