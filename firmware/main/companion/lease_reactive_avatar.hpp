#pragma once
#include "audio_owner.hpp"
#include "desk_reactive_avatar.hpp"
namespace companion::desk {
// Boot-lifetime passive owner, only immutable token copies enter callbacks.
// The codec callback is checked IO evidence; socket state alone is not.
class LeaseReactiveAvatar {
    std::mutex mutex_;
    ReactiveAvatarPolicy policy_;
    AudioLease lease_;
    uint32_t media_epoch_=0,expected_epoch_=0,last_observed_=0;
    bool selected_=false,have_observed_=false,stopping_=false;
    StopFact stop_=StopFact::None;
public:
    bool Bind(const AudioLease& lease) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!CommonAudioAuthority().Current(lease)||lease.owner()!=AudioOwner::Mira||(lease.purpose()!=AudioPurpose::Session&&lease.purpose()!=AudioPurpose::PhysicalListen))return false;
        lease_=lease;media_epoch_=expected_epoch_=0;have_observed_=false;stopping_=false;stop_=StopFact::None;policy_.Revoke();return true;
    }
    void Select(bool selected){std::lock_guard<std::mutex> lock(mutex_);selected_=selected;}
    bool Selected(){std::lock_guard<std::mutex> lock(mutex_);return selected_;}
    bool Observe(const AudioLease& captured,uint64_t generation,uint32_t epoch,uint32_t current_media_epoch,uint16_t peak,bool playback,uint32_t now) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(stopping_||!CommonAudioAuthority().Current(captured)||!lease_||captured.generation()!=lease_.generation()||
           captured.epoch()!=lease_.epoch()||generation!=captured.generation()||!epoch||epoch!=current_media_epoch)return false;
        if(!media_epoch_){if(!policy_.Bind(generation,epoch))return false;media_epoch_=epoch;}
        else if(epoch!=media_epoch_){if(!policy_.AdvanceEpoch(generation,epoch))return false;media_epoch_=epoch;}
        OwnerFacts facts{playback?MicFact::OffConfirmed:MicFact::Listening,playback?OutputFact::Playing:OutputFact::OffConfirmed,StopFact::None,playback?FaceIntent::Speaking:FaceIntent::Listening};
        if(!policy_.SetFacts(generation,epoch,facts,true)||!policy_.Observe(generation,epoch,peak,playback,now))return false;
        expected_epoch_=current_media_epoch;last_observed_=now;have_observed_=true;return true;
    }
    void UpdateMediaEpoch(const AudioLease& captured,uint32_t epoch){std::lock_guard<std::mutex> lock(mutex_);if(Match(captured))expected_epoch_=epoch;}
    void RequestStopFor(const AudioLease& captured){std::lock_guard<std::mutex> lock(mutex_);if(Match(captured))StopLocked();}
    void ClosedFor(const AudioLease& captured,bool checked_closed_and_joined){std::lock_guard<std::mutex> lock(mutex_);if(Match(captured))ClosedLocked(checked_closed_and_joined);}
    void RequestStop(){std::lock_guard<std::mutex> lock(mutex_);StopLocked();}
    void Closed(bool checked_closed_and_joined){std::lock_guard<std::mutex> lock(mutex_);ClosedLocked(checked_closed_and_joined);}
    ReactiveFrame Snapshot(uint32_t now,uint32_t current_media_epoch) {
        std::lock_guard<std::mutex> lock(mutex_);
        return SnapshotLocked(now,current_media_epoch);
    }
    ReactiveFrame SnapshotCurrent(uint32_t now){std::lock_guard<std::mutex> lock(mutex_);return SnapshotLocked(now,expected_epoch_);}
private:
    bool Match(const AudioLease& l)const{return l&&lease_&&CommonAudioAuthority().Holds(l)&&l.generation()==lease_.generation()&&l.epoch()==lease_.epoch()&&l.owner()==lease_.owner()&&l.purpose()==lease_.purpose();}
    void StopLocked(){stopping_=true;stop_=StopFact::Pending;have_observed_=false;policy_.RequestStop();}
    void ClosedLocked(bool checked){stopping_=true;stop_=checked?(CommonAudioAuthority().Holds(lease_)?StopFact::Confirmed:StopFact::None):StopFact::Failed;have_observed_=false;policy_.Revoke();}
    ReactiveFrame SnapshotLocked(uint32_t now,uint32_t current_media_epoch){
        ReactiveFrame frame;
        if(selected_&&stop_==StopFact::None)frame.eye_x=int((now/2000)%3)*4-4;
        if(!stopping_&&CommonAudioAuthority().Current(lease_)&&have_observed_&&media_epoch_==current_media_epoch&&uint32_t(now-last_observed_)<=250)frame=policy_.Snapshot(now);
        // Facts expire with checked observations; neither transport state nor
        // logical mute can turn unknown directions into confirmed physical off.
        frame.apply_face=selected_; // Pure companion face always resets safely.
        if(stop_==StopFact::Pending)frame.status.stop="Stop pending";
        else if(stop_==StopFact::Failed)frame.status.stop="Stop unconfirmed";
        else if(stop_==StopFact::Confirmed&&(CurrentAudioOwner()==AudioOwner::None||CommonAudioAuthority().Holds(lease_))){frame.status.stop="Directions closed confirmed";frame.status.mic="Mic off confirmed";frame.status.output="Output off confirmed";}
        return frame;
    }
};
}
