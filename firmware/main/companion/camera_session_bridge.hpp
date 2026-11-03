#pragma once
#include "companion/camera_stop_receipt.hpp"
#include <string>
#include <memory>
#include <mutex>
namespace companion {
struct CameraAuthorityContext {
    std::string device,conversation,turn,purpose_id,boot;
    uint64_t turn_generation=0,expires_ms=0,authority_id=0;
    desk::CameraBinding binding{};
};
inline bool CameraBridgeAscii(const std::string& s,size_t maximum) {
    if(s.empty()||s.size()>maximum)return false;
    for(unsigned char c:s)if(c<33||c>126)return false;
    return true;
}
inline bool CameraBridgeHex(const std::string& s) {
    if(s.size()!=32)return false;
    for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    return true;
}
inline bool CameraBridgeContextValid(const CameraAuthorityContext& c,uint64_t now) {
    return CameraBridgeAscii(c.device,64)&&CameraBridgeAscii(c.conversation,128)&&CameraBridgeAscii(c.turn,128)&&
        CameraBridgeAscii(c.purpose_id,128)&&CameraBridgeHex(c.boot)&&c.binding.generation&&c.binding.epoch&&
        c.turn_generation&&c.authority_id&&c.expires_ms>now;
}
// Camera-purpose owner interface; camera_permission_wire binds the concrete
// expiry/revocation ledger and independently confirmed physical device UI choice.
// Audio TLS, sc_pair, peer JSON and connection numbers cannot implement it.
class TrustedCameraSessionDispatcher {
public:
    virtual ~TrustedCameraSessionDispatcher()=default;
    virtual bool Snapshot(uint64_t connection,uint32_t audio_epoch,uint64_t now,CameraAuthorityContext& out)=0;
    virtual bool RetainedSnapshot(uint64_t connection,uint32_t audio_epoch,CameraAuthorityContext& out)=0;
    virtual bool Current(uint64_t connection,uint32_t audio_epoch,const CameraAuthorityContext&,uint64_t now)=0;
    virtual bool RetainedStopCurrent(uint64_t connection,uint32_t audio_epoch,const CameraAuthorityContext&)=0;
    virtual bool RevokedNoNewerStream(const CameraAuthorityContext&,const desk::BoardCameraStopReport&)=0;
};
struct CameraStopChallenge {
    CameraAuthorityContext owner;
    std::string nonce;
    uint64_t sequence=0;
};
// Held across reconnects. No peer bool or ordinary report serial releases it.
class CameraRestartBarrier {
    mutable std::mutex mutex_;
    CameraStopChallenge retained_{};
    uint64_t report_serial_=0,last_sequence_=0;
    bool pending_=false,joined_=false,revoked_no_newer_=false;
public:
    bool Begin(const CameraStopChallenge& c) {
        std::lock_guard<std::mutex> l(mutex_);
        if(!CameraBridgeHex(c.nonce)||!c.sequence||c.sequence<=last_sequence_)return false;
        if(pending_){const auto& a=retained_.owner;const auto& b=c.owner;
            if(c.nonce==retained_.nonce||a.device!=b.device||a.conversation!=b.conversation||a.turn!=b.turn||a.boot!=b.boot||
               a.purpose_id!=b.purpose_id||a.turn_generation!=b.turn_generation||a.authority_id!=b.authority_id||!(a.binding==b.binding))return false;
        }
        retained_=c;last_sequence_=c.sequence;pending_=true;joined_=revoked_no_newer_=false;report_serial_=0;return true;
    }
    bool Completed(const desk::BoardCameraStopReport& r,bool worker_joined,bool revoked_no_newer) {
        std::lock_guard<std::mutex> l(mutex_);
        const auto& d=r.driver();
        if(!pending_||!r.exact_session()||!r.custody_released()||!(r.requested()==retained_.owner.binding)||
           !(d.binding()==retained_.owner.binding)||d.authority_id()!=retained_.owner.authority_id||
           (!d.owned_driver_closed()&&!d.owned_never_started())||!worker_joined||!revoked_no_newer)return false;
        report_serial_=d.report_serial();joined_=true;revoked_no_newer_=true;return true;
    }
    bool ReadyAck(const CameraStopChallenge& c,uint64_t serial) {
        std::lock_guard<std::mutex> l(mutex_);
        const auto& a=retained_.owner;const auto& b=c.owner;
        if(!joined_||!revoked_no_newer_||!serial||serial!=report_serial_||c.sequence!=retained_.sequence||
           c.nonce!=retained_.nonce||a.device!=b.device||a.conversation!=b.conversation||a.turn!=b.turn||
           a.boot!=b.boot||a.authority_id!=b.authority_id||!(a.binding==b.binding))return false;
        return true;
    }
    bool Acknowledge(const CameraStopChallenge& c,uint64_t serial) {
        std::lock_guard<std::mutex> l(mutex_);
        const auto& a=retained_.owner;const auto& b=c.owner;
        if(!joined_||!revoked_no_newer_||!serial||serial!=report_serial_||c.sequence!=retained_.sequence||
           c.nonce!=retained_.nonce||a.device!=b.device||a.conversation!=b.conversation||a.turn!=b.turn||
           a.boot!=b.boot||a.authority_id!=b.authority_id||!(a.binding==b.binding))return false;
        pending_=false;return true;
    }
    bool Pending() const {std::lock_guard<std::mutex> l(mutex_);return pending_;}
};
}
