#pragma once
#include <cstdint>
#include <mutex>

namespace companion::desk {
enum class MicFact { Unknown, OffConfirmed, LocalWake, Listening };
enum class OutputFact { Unknown, OffConfirmed, Playing, DrainPending };
enum class StopFact { None, Pending, Failed, Confirmed };
enum class FaceIntent { Idle, Listening, Thinking, Speaking, Disconnected, Error };
struct OwnerFacts {
    MicFact mic=MicFact::Unknown;
    OutputFact output=OutputFact::Unknown;
    StopFact stop=StopFact::None;
    FaceIntent face=FaceIntent::Idle;
};
struct StatusIntent {
    const char* mic="Mic state unverified";
    const char* output="Output state unverified";
    const char* stop="";
    bool dim=false;
};
struct ReactiveFrame { int eye_x=0,eye_y=0,mouth_weight=0; bool apply_face=false; StatusIntent status; };

// Passive scalar mailbox. Methods may run on the serialized codec owner or UI
// task; no renderer/audio pointer, PCM, clock, hardware, or task is retained.
class ReactiveAvatarPolicy {
public:
    bool Bind(uint64_t generation,uint32_t epoch) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!generation||!epoch||generation<=last_generation_)return false;
        last_generation_=generation;epoch_=epoch;bound_=true;enabled_=false;have_face_=false;
        facts_={};ClearLevel();return true;
    }
    bool AdvanceEpoch(uint64_t generation,uint32_t epoch) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!bound_||generation!=last_generation_||epoch<=epoch_)return false;
        epoch_=epoch;enabled_=false;facts_={};ClearLevel();return true;
    }
    bool SetFacts(uint64_t generation,uint32_t epoch,OwnerFacts facts,bool enable_face) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!Current(generation,epoch))return false;
        const bool eligibility_changed=facts_.mic!=facts.mic||facts_.output!=facts.output||facts_.stop!=facts.stop;
        facts_=facts;enabled_=enable_face;have_face_=have_face_||enable_face;
        if(!enable_face||eligibility_changed||facts.stop!=StopFact::None)ClearLevel();
        return true;
    }
    void Revoke() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        bound_=false;enabled_=false;facts_={};ClearLevel();
    }
    // Hold/local stop immediately suppresses the face; physical facts remain
    // unknown until the real owner reports checked direction close results.
    void RequestStop() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_=false;facts_.mic=MicFact::Unknown;facts_.output=OutputFact::Unknown;
        facts_.stop=StopFact::Pending;ClearLevel();
    }
    bool Observe(uint64_t generation,uint32_t epoch,uint16_t peak,bool playback,uint32_t now) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!Current(generation,epoch)||!enabled_||facts_.stop!=StopFact::None||
           (playback?facts_.output!=OutputFact::Playing:facts_.mic!=MicFact::Listening))return false;
        // Peak is absolute int16 amplitude (32768 is possible); conservative
        // deadband, bounded intensity, immediate attack, slow scalar release.
        const unsigned bounded=peak>32768?32768:peak;
        const int target=bounded<=512?0:int((bounded-512)*60/(32768-512));
        weight_=target>=weight_?target:(weight_*3+target)/4;
        seen_level_=true;level_at_=now;return true;
    }
    ReactiveFrame Snapshot(uint32_t now,bool local_activity=false) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if(local_activity)last_activity_=now;
        ReactiveFrame frame;
        frame.apply_face=bound_&&have_face_;
        switch(facts_.mic){
        case MicFact::Unknown:break;
        case MicFact::OffConfirmed:frame.status.mic="Mic off confirmed";break;
        case MicFact::LocalWake:frame.status.mic="Local wake capture active";break;
        case MicFact::Listening:frame.status.mic="Listening: capture active";break;
        }
        switch(facts_.output){
        case OutputFact::Unknown:break;
        case OutputFact::OffConfirmed:frame.status.output="Output off confirmed";break;
        case OutputFact::Playing:frame.status.output="Playback active";break;
        case OutputFact::DrainPending:frame.status.output="Playback completion unverified";break;
        }
        switch(facts_.stop){
        case StopFact::None:break;
        case StopFact::Pending:frame.status.stop="Stop pending";break;
        case StopFact::Failed:frame.status.stop="Stop unconfirmed";break;
        case StopFact::Confirmed:frame.status.stop="Directions closed confirmed";break;
        }
        frame.status.dim=facts_.mic==MicFact::OffConfirmed&&facts_.output==OutputFact::OffConfirmed&&
            uint32_t(now-last_activity_)>=30000;
        if(!bound_||!enabled_)return frame;
        switch(facts_.face){
        case FaceIntent::Idle:frame.eye_x=int((now/2000)%3)*4-4;break;
        case FaceIntent::Listening:frame.eye_y=-3;break;
        case FaceIntent::Thinking:frame.eye_x=4;frame.eye_y=-4;break;
        case FaceIntent::Speaking:break;
        case FaceIntent::Disconnected:frame.eye_y=2;break;
        case FaceIntent::Error:frame.eye_y=4;break;
        }
        if(seen_level_&&uint32_t(now-level_at_)<=250)frame.mouth_weight=weight_;
        else ClearLevel();
        return frame;
    }
private:
    std::mutex mutex_;
    uint64_t last_generation_=0;uint32_t epoch_=0,level_at_=0,last_activity_=0;
    bool bound_=false,enabled_=false,seen_level_=false,have_face_=false;int weight_=0;
    OwnerFacts facts_;
    bool Current(uint64_t g,uint32_t e) const noexcept{return bound_&&g==last_generation_&&e==epoch_;}
    void ClearLevel() noexcept{seen_level_=false;weight_=0;level_at_=0;}
};

// Status gets a dedicated persistent surface, never the speech slot. Snapshot
// is fresh after HAL locking; the scalar mailbox never takes HAL locks. Caller
// must supply a status callback that does not reenter audio or this mailbox.
template<class Guard,class Avatar,class PersistentStatus>
bool RenderReactiveAvatar(Avatar& avatar,ReactiveAvatarPolicy& policy,uint32_t now,PersistentStatus status,bool local_activity=false) {
    Guard guard;
    const auto frame=policy.Snapshot(now,local_activity);
    status(frame.status);
    if(!frame.apply_face||avatar.isModifyLocked())return false;
    avatar.leftEye().setPosition({frame.eye_x,frame.eye_y});
    avatar.rightEye().setPosition({frame.eye_x,frame.eye_y});
    avatar.mouth().setWeight(frame.mouth_weight);
    return true;
}
enum class LocalGesture { Tap, Tilt, Hold };
enum class LocalIntent { None, WakeDisplay, ShowControls, RequestStop };
class PassiveLocalControls {
    bool seen_=false;uint32_t at_=0;
public:
    // The returned intent must be routed to the existing owners. This is never
    // physical stop success, a hardware subscription, or authorization to start.
    LocalIntent Event(uint32_t now,LocalGesture gesture,ReactiveAvatarPolicy& policy) noexcept {
        if(gesture==LocalGesture::Hold){policy.RequestStop();seen_=true;at_=now;return LocalIntent::RequestStop;}
        if(gesture!=LocalGesture::Tap&&gesture!=LocalGesture::Tilt)return LocalIntent::None;
        if(seen_&&uint32_t(now-at_)<750)return LocalIntent::None;
        seen_=true;at_=now;
        return gesture==LocalGesture::Tap?LocalIntent::WakeDisplay:LocalIntent::ShowControls;
    }
};
}
