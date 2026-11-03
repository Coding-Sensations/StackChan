#pragma once
#include "audio_owner.hpp"
#include "desk_codec_adapter.hpp"
namespace companion::desk {
// Integration barrier: AudioService still lacks matched custody, checked IO and
// a checked join receipt. Exact Hey Mira model is unavailable. No runtime toggle.
inline constexpr bool kAllAudioConsumersCovered=false;
inline constexpr bool kExactLocalWakeModelVerified=false;
class ExistingAudioOwnerBridge {
public:
    using ConsentGuard=bool(*)(AudioBinding,InputPurpose);
private:
    AudioLease lease_;
    AudioBinding binding_{};
    InputPurpose purpose_=InputPurpose::LocalWake;
    ConsentGuard consent_=nullptr;
public:
    explicit ExistingAudioOwnerBridge(ConsentGuard guard=nullptr):consent_(guard){}
    bool AcquireNoneToMira(AudioBinding b,InputPurpose p){
        if(!kAllAudioConsumersCovered||!b.generation||!b.epoch||lease_||!consent_)return false;
        if(p!=InputPurpose::PhysicalListen&&p!=InputPurpose::LocalWake)return false;
        if(p==InputPurpose::LocalWake&&!kExactLocalWakeModelVerified)return false;
        if(!consent_(b,p))return false;
        const auto purpose=p==InputPurpose::LocalWake?AudioPurpose::LocalWake:AudioPurpose::PhysicalListen;
        if(!AcquireAudio(AudioOwner::Mira,purpose,b.epoch,lease_))return false;
        binding_=b;purpose_=p;
        if(!Current(b,p)){RevokeMira(b);ReleaseMira(b);return false;}
        return true;
    }
    bool HoldsMiraLease(AudioBinding b){return b==binding_&&CommonAudioAuthority().Holds(lease_);}
    bool Current(AudioBinding b,InputPurpose p){
        return kAllAudioConsumersCovered&&b==binding_&&p==purpose_&&consent_&&consent_(b,p)&&
            CommonAudioAuthority().Current(lease_);
    }
    bool RevokeMira(AudioBinding b){return HoldsMiraLease(b)&&RevokeAudio(lease_);}
    // Called only by BorrowedCodecInput after its checked close and DSP reset.
    bool ReleaseMira(AudioBinding b){if(!HoldsMiraLease(b))return false;return ReleaseAudio(lease_,true);}
};
}
