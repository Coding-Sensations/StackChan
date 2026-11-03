#pragma once
#include "camera_session_bridge.hpp"
#include <functional>
#include <set>
namespace companion {
// RAM owner implementation. None of these callbacks are wire parsers. Deployment
// owns authenticated camera-purpose delivery, current local device confirmation,
// and cleanup route/ACK provenance. Absent callbacks deny. Audio is routing only.
class BoundedCameraSessionDispatcher final : public TrustedCameraSessionDispatcher {
public:
    using Verify=std::function<bool(uint64_t,uint32_t,const CameraAuthorityContext&)>;
    using AckVerify=std::function<bool(const CameraAuthorityContext&,const desk::BoardCameraStopReport&)>;
private:
    std::mutex mutex_;
    Verify purpose_current_,device_current_,cleanup_route_;
    AckVerify ack_consumed_;
    CameraAuthorityContext held_{};
    uint64_t connection_=0,last_now_=0,last_generation_=0;
    std::set<std::string> authority_ids_;
    std::string device_;
    uint32_t epoch_=0;
    unsigned admitted_=0;
    bool held_valid_=false,revoked_=true,held_committed_=false,capture_eligible_=false;
    static bool Same(const CameraAuthorityContext& a,const CameraAuthorityContext& b) {
        return a.device==b.device&&a.conversation==b.conversation&&a.turn==b.turn&&
            a.purpose_id==b.purpose_id&&a.boot==b.boot&&a.turn_generation==b.turn_generation&&
            a.expires_ms==b.expires_ms&&a.authority_id==b.authority_id&&a.binding==b.binding;
    }
    bool Now(uint64_t now) {
        if(!now||now<last_now_){revoked_=true;return false;}
        last_now_=now;return true;
    }
    static bool Check(const Verify& f,uint64_t connection,uint32_t epoch,const CameraAuthorityContext& c) {
        if(!f)return false;
        try{return f(connection,epoch,c);}catch(...){return false;}
    }
    bool CurrentLocked(uint64_t connection,uint32_t epoch,const CameraAuthorityContext& c,uint64_t now) {
        if(!Now(now)||!held_valid_||revoked_||!Same(c,held_))return false;
        if(now>=held_.expires_ms||connection!=connection_||epoch!=epoch_||
           !Check(purpose_current_,connection,epoch,held_)||!Check(device_current_,connection,epoch,held_)){
            revoked_=true;return false;
        }
        return true;
    }
    bool Report(const CameraAuthorityContext& c,const desk::BoardCameraStopReport& r) const {
        const auto& d=r.driver();
        return held_valid_&&revoked_&&Same(c,held_)&&r.exact_session()&&r.custody_released()&&
            r.requested()==c.binding&&d.binding()==c.binding&&d.authority_id()==c.authority_id&&
            (d.owned_driver_closed()||d.owned_never_started());
    }
    bool RetainLocked(uint64_t connection,uint32_t epoch,uint64_t now,const CameraAuthorityContext& c) {
        if(!Now(now)||held_valid_||!connection||!epoch||admitted_>=64||
           !CameraBridgeContextValid(c,now)||c.expires_ms-now>60000||
           c.binding.generation<=last_generation_||authority_ids_.count(c.boot+":"+std::to_string(c.authority_id)+":"+std::to_string(c.binding.generation)+":"+std::to_string(c.binding.epoch))||(admitted_&&c.device!=device_))return false;
        held_=c;held_valid_=true;revoked_=true;capture_eligible_=true;connection_=connection;epoch_=epoch;
        last_generation_=c.binding.generation;authority_ids_.insert(c.boot+":"+std::to_string(c.authority_id)+":"+std::to_string(c.binding.generation)+":"+std::to_string(c.binding.epoch));device_=c.device;++admitted_;return true;
    }
public:
    explicit BoundedCameraSessionDispatcher(Verify purpose={},Verify device={},Verify cleanup={},AckVerify ack={})
        :purpose_current_(std::move(purpose)),device_current_(std::move(device)),
         cleanup_route_(std::move(cleanup)),ack_consumed_(std::move(ack)){}
    // Reserve cleanup custody before a confirmation is sent or a host commit
    // arrives. This admission is revoked and cannot authorize frames.
    bool RetainReservationFromTrustedOwner(uint64_t connection,uint32_t epoch,uint64_t now,const CameraAuthorityContext& c) {
        std::lock_guard<std::mutex> l(mutex_);
        // Actual acquired custody survives transport/clock/budget races. These
        // failures deny capture activation; they cannot abandon cleanup.
        if(held_valid_||!connection||!epoch||!CameraBridgeContextValid(c,0)||
           c.binding.generation<=last_generation_||(admitted_&&c.device!=device_))return false;
        capture_eligible_=now&&now>=last_now_&&admitted_<64&&c.expires_ms>now&&c.expires_ms-now<=60000;
        if(now>last_now_)last_now_=now;
        held_=c;held_valid_=true;revoked_=true;connection_=connection;epoch_=epoch;
        last_generation_=c.binding.generation;device_=c.device;
        if(admitted_<64){authority_ids_.insert(c.boot+":"+std::to_string(c.authority_id)+":"+std::to_string(c.binding.generation)+":"+std::to_string(c.binding.epoch));++admitted_;}
        return true;
    }
    // Commit only the exact retained reservation. A shorter host expiry is
    // allowed; identity, generation and original cleanup custody cannot change.
    bool InstallFromTrustedOwner(uint64_t connection,uint32_t epoch,uint64_t now,const CameraAuthorityContext& c) {
        std::lock_guard<std::mutex> l(mutex_);
        // Preserve direct trusted-owner callers: reserve atomically only after
        // both existing authorization callbacks pass. Wire callers always use
        // the explicit precommit reservation API above.
        if(!held_valid_&&(!Check(purpose_current_,connection,epoch,c)||!Check(device_current_,connection,epoch,c)||
                         !RetainLocked(connection,epoch,now,c)))return false;
        auto bounded=c;bounded.expires_ms=held_.expires_ms;
        if(!Now(now)||!held_valid_||!capture_eligible_||held_committed_||!revoked_||!Same(bounded,held_)||
           connection!=connection_||epoch!=epoch_||!CameraBridgeContextValid(c,now)||
           c.expires_ms>held_.expires_ms||
           !Check(purpose_current_,connection,epoch,c)||!Check(device_current_,connection,epoch,c))return false;
        held_=c;revoked_=false;held_committed_=true;return true;
    }
    bool Snapshot(uint64_t connection,uint32_t epoch,uint64_t now,CameraAuthorityContext& out) override {
        std::lock_guard<std::mutex> l(mutex_);out={};
        if(!CurrentLocked(connection,epoch,held_,now))return false;
        out=held_;return true;
    }
    bool Current(uint64_t connection,uint32_t epoch,const CameraAuthorityContext& c,uint64_t now) override {
        std::lock_guard<std::mutex> l(mutex_);return CurrentLocked(connection,epoch,c,now);
    }
    void RevokeFromTrustedOwner() {std::lock_guard<std::mutex> l(mutex_);revoked_=true;}
    bool RetainedSnapshot(uint64_t connection,uint32_t epoch,CameraAuthorityContext& out) override {
        std::lock_guard<std::mutex> l(mutex_);out={};
        if(!held_valid_||!connection||!epoch||!Check(cleanup_route_,connection,epoch,held_))return false;
        out=held_;return true;
    }
    bool RetainedStopCurrent(uint64_t connection,uint32_t epoch,const CameraAuthorityContext& c) override {
        std::lock_guard<std::mutex> l(mutex_);
        return held_valid_&&Same(c,held_)&&connection&&epoch&&Check(cleanup_route_,connection,epoch,held_);
    }
    bool RevokedNoNewerStream(const CameraAuthorityContext& c,const desk::BoardCameraStopReport& r) override {
        std::lock_guard<std::mutex> l(mutex_);return Report(c,r);
    }
    bool ReleaseAfterVerifiedAck(const CameraAuthorityContext& c,const desk::BoardCameraStopReport& r) {
        std::lock_guard<std::mutex> l(mutex_);
        if(!Report(c,r)||!ack_consumed_)return false;
        try{if(!ack_consumed_(c,r))return false;}catch(...){return false;}
        held_valid_=held_committed_=false;held_={};connection_=0;epoch_=0;return true;
    }
};
}
