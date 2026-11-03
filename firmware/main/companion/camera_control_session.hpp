#pragma once
#include <cstdint>
#include <mutex>
#include <string>
namespace companion {
enum class CameraControlResult { NoSession, LocalOptInRequired, CameraPurposeGrantMissing, DriverUnqualified, Stopping, Replayed, InvalidToken };
// There is deliberately no public constructor capable of minting authority.
// Audio pairing/grants do not carry a camera purpose. The separate approved
// camera_permission_wire exchange binds fresh Mac and physical device choices;
// its concrete dispatcher is separate from this deliberately audio-only token.
class CameraPurposeToken {
    uint64_t issuer_generation_=0;
    friend class CameraPurposeIssuer;
public:
    CameraPurposeToken()=default;
    bool valid() const noexcept {return issuer_generation_!=0;}
};
class CameraPurposeIssuer {
public:
    static CameraPurposeToken FromExistingAudioGrant() noexcept {return {};}
    static constexpr bool ExistingGrantPermitsCamera() noexcept {return false;}
    static bool Current(const CameraPurposeToken& token) noexcept {return token.valid()&&ExistingGrantPermitsCamera();}
};
class CameraControlSession {
    mutable std::mutex mutex_;
    uint64_t connection_=0,opt_in_connection_=0;
    uint32_t epoch_=0,opt_in_epoch_=0;
    uint64_t control_sequence_=0;
    CameraPurposeToken purpose_{};
    bool connected_=false,off_requested_=false;
public:
    void Begin(uint64_t connection,uint32_t epoch) {
        std::lock_guard<std::mutex> lock(mutex_);
        connection_=connection;epoch_=epoch;connected_=connection&&epoch;
        opt_in_connection_=0;opt_in_epoch_=0;control_sequence_=0;off_requested_=false;
        purpose_=CameraPurposeIssuer::FromExistingAudioGrant();
    }
    void Disconnect() {
        std::lock_guard<std::mutex> lock(mutex_);
        connected_=false;opt_in_connection_=0;opt_in_epoch_=0;purpose_={};off_requested_=true;
    }
    bool DeviceOptIn(uint64_t connection,uint32_t epoch) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!connected_||connection!=connection_||epoch!=epoch_)return false;
        opt_in_connection_=connection;opt_in_epoch_=epoch;off_requested_=false;return true;
    }
    CameraControlResult On(uint64_t connection,uint32_t epoch,uint64_t sequence,const std::string& wire_token={}) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!connected_||connection!=connection_||epoch!=epoch_)return CameraControlResult::NoSession;
        if(!sequence||sequence<=control_sequence_)return CameraControlResult::Replayed;
        control_sequence_=sequence;
        if(!wire_token.empty())return CameraControlResult::InvalidToken;
        if(opt_in_connection_!=connection||opt_in_epoch_!=epoch||off_requested_)return CameraControlResult::LocalOptInRequired;
        if(!CameraPurposeIssuer::Current(purpose_))return CameraControlResult::CameraPurposeGrantMissing;
        return CameraControlResult::DriverUnqualified;
    }
    CameraControlResult Off(uint64_t connection,uint32_t epoch,uint64_t sequence) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!connected_||connection!=connection_||epoch!=epoch_)return CameraControlResult::NoSession;
        if(!sequence||sequence<=control_sequence_)return CameraControlResult::Replayed;
        control_sequence_=sequence;off_requested_=true;opt_in_connection_=0;opt_in_epoch_=0;purpose_={};
        // Revocation receipt is not a physical camera-off receipt.
        return CameraControlResult::Stopping;
    }
    bool DeviceOptInCurrent(uint64_t connection,uint32_t epoch) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return connected_&&!off_requested_&&connection==connection_&&epoch==epoch_&&opt_in_connection_==connection&&opt_in_epoch_==epoch;
    }
    bool CurrentFrameAuthority(uint64_t connection,uint32_t epoch) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return connected_&&!off_requested_&&connection==connection_&&epoch==epoch_&&
            opt_in_connection_==connection&&opt_in_epoch_==epoch&&CameraPurposeIssuer::Current(purpose_);
    }
};
inline const char* CameraControlReason(CameraControlResult r) noexcept {
    switch(r){
        case CameraControlResult::NoSession:return "session_changed";
        case CameraControlResult::LocalOptInRequired:return "device_camera_opt_in_required";
        case CameraControlResult::CameraPurposeGrantMissing:return "camera_purpose_grant_missing";
        case CameraControlResult::DriverUnqualified:return "capture_clock_and_checked_stop_unqualified";
        case CameraControlResult::Stopping:return "revoked_physical_stop_unverified";
        case CameraControlResult::Replayed:return "control_replayed";
        default:return "camera_token_invalid";
    }
}
}
