#pragma once
#include "codec_tasks.h"
#include "camera_control_session.hpp"
#include "camera_session_bridge.hpp"
#include "camera_local_permission.hpp"
#include "camera_authority_ledger.hpp"
#include <thread>
#include "camera_video_wire.hpp"
#include "camera_media_priority.hpp"
#include "companion/desk_camera_adapter.hpp"
#include <network_interface.h>
#include "bounded_web_socket.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
namespace companion {
// Explicit, volatile connection grant. No NVS/token issuance or auto-connect.
// Endpoint is WSS with vendor ESP TLS certificate verification, no insecure mode.
class CompanionSocket {
public:
    enum class State { Disconnected, AwaitingBegin, ConsentRequired, Arming, Listening, Speaking, Muted, Fault };
    State GetState() const { return state_.load(); }
    uint32_t Epoch() const {return epoch_.load();}
    explicit CompanionSocket(CodecTasks& tasks);
    ~CompanionSocket(){Close();if(camera_stop_worker_.joinable())camera_stop_worker_.join();}
    bool Connect(NetworkInterface& network,const std::string& uri,const std::string& device_id,const std::string& credential,const std::string& request_id);
    bool PhysicalConsent();
    // Constructor installs the concrete two-choice camera purpose ledger.
    // Trusted replacement cannot erase retained cleanup custody.
    void SetTrustedCameraDispatcher(std::shared_ptr<TrustedCameraSessionDispatcher> dispatcher);
    std::string CameraProducerBoot() const;
    bool CameraRestartPending() const {return camera_restart_.Pending();}
    void SetConsentGuard(std::function<bool()> guard){consent_guard_=std::move(guard);}
    void InterruptImmediately();
    bool AuthenticationRejected()const{return auth_rejected_.load();}
    bool PhysicalConsent(uint32_t expected_epoch);
    void PublishCameraPhysicalControls(const void* allow,const void* decline,uint64_t view);
    std::string ShowCameraPermission();
    void HideCameraPermission(){camera_local_permission_.Hide();}
    bool CaptureCameraLocalChoice(const void* control,uint64_t view);
    bool SendCameraFrame(desk::CameraBinding board_lease,CameraVideoIdentity identity,uint8_t* bounded_bgra,size_t capacity,uint8_t* bounded_fragment,size_t fragment_capacity);
    bool SendAudio(const uint8_t* data,std::size_t bytes);
    void Tick(); void Close();
private:
    bool PhysicalCameraOptIn(uint32_t expected_epoch);
    CameraLocalPermissionOwner camera_local_permission_;
    std::shared_ptr<BoundedCameraSessionDispatcher> camera_purpose_dispatcher_;
    std::shared_ptr<const CameraAuthorityContext> camera_confirmed_;
    std::shared_ptr<const CameraPermissionProposal> camera_confirmed_proposal_;
    std::atomic<bool> camera_purpose_revoked_{true},camera_permission_commit_consumed_{false};
    std::atomic<uint64_t> camera_permission_revision_{1},camera_confirmed_revision_{0},camera_view_generation_{0};
    std::string camera_device_,camera_conversation_,camera_turn_;uint64_t camera_turn_generation_=0;
    mutable std::mutex camera_choice_mutex_;
    bool CameraPermissionProtocol(void* root,uint64_t connection);
    bool DeviceCameraChoice(const CapturedCameraChoice& choice);
    bool CameraPermissionReply(const CameraPermissionProposal&,const char* state,const char* reason,const CameraAuthorityContext* authority=nullptr);
    bool SendCameraFragment(const uint8_t* data,size_t bytes,uint64_t expected_connection,uint32_t expected_epoch);
    bool CameraFrameContextCurrent(uint64_t connection,uint32_t epoch,desk::CameraBinding lease,CameraVideoIdentity identity);
    CameraControlSession camera_control_;CameraMediaPriority media_priority_;
    mutable std::mutex camera_protocol_mutex_;
    std::shared_ptr<TrustedCameraSessionDispatcher> camera_dispatcher_;
    CameraRestartBarrier camera_restart_;
    std::thread camera_stop_worker_;std::atomic<bool> camera_worker_finished_{false};
    CameraStopChallenge camera_stop_challenge_;desk::BoardCameraStopReport camera_stop_report_;
    bool camera_stop_ack_consumed_=false; // protocol mutex; bounded exact ACK retry receipt
    uint64_t camera_stop_connection_=0;uint32_t camera_stop_audio_epoch_=0;
    std::string camera_clock_scope_,camera_clock_nonces_[64];size_t camera_clock_nonce_count_=0;std::string camera_boot_;
    bool CameraPurposeCurrent(uint64_t connection,uint32_t epoch);
    bool CameraProtocolData(void* root,uint64_t generation);
    bool CameraText(const std::string& wire,uint64_t generation,uint32_t audio_epoch);
    void PollCameraStop();
    void RevokeCameraSession();
    std::atomic<uint64_t> connection_generation_{0};
    std::atomic<bool> transport_current_{false},connection_exhausted_{false};
    std::atomic<State> state_{State::Disconnected};
    CodecTasks& tasks_;std::shared_ptr<BoundedWebSocket> socket_;std::mutex mutex_;
    std::atomic<uint32_t> epoch_{0};std::atomic<uint32_t> consent_epoch_{0};
    std::atomic<bool> auth_rejected_{false};
    std::function<bool()> consent_guard_;std::atomic<uint64_t> consent_deadline_ms_{0};
    std::atomic<uint64_t> last_heartbeat_ms_{0};std::string request_id_;
    bool Text(const std::string& message);void Data(const char* data,std::size_t bytes,bool binary,uint64_t callback_generation);
    bool CameraControlData(void* root,uint64_t callback_generation);
};
}
