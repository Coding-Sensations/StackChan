#pragma once
#include "remembered_startup.hpp"
#include "audio_owner.hpp"
#include "remembered_worker_policy.hpp"
#include "companion_socket.h"
#include "physical_source_gesture.hpp"
#include "psram_endpoint_factory.hpp"
#include "wake_model_asset_provider.hpp"
#include <deque>
namespace companion {
struct RememberedWorkerEvent {
    enum class Kind { Authenticated, Armed, Failure, Joined } kind;
    uint64_t generation,monotonic_ms;
    ReconnectFailure failure=ReconnectFailure::Transient;
};
// Boot-lifetime owner. Constructor inert; StartWorker requires already approved
// existing-peer runtime. No pairing, NVS initialization or enrollment occurs here.
class RememberedAudioWorker final : public RememberedStartupPort {
public:
    explicit RememberedAudioWorker(RememberedCredentialVault& store);
    bool StartWorker(bool startup_approved);
    void EnsureSavedWifi(uint64_t generation) override;
    void ConnectExisting(const ExistingRememberedPeer&,uint64_t generation) override;
    void RequestPhysicalConsent(uint64_t generation) override;
    void CloseAndJoin(uint64_t generation) override;
    void InterruptImmediately() override;
    bool Poll(RememberedWorkerEvent& event);
    void RegisterPhysicalControl(const void*,uint64_t view);
    void RegisterCameraControls(const void* allow,const void* decline,uint64_t view){socket_.PublishCameraPhysicalControls(allow,decline,view);}
    std::string ShowCameraPermission(){return Live()?socket_.ShowCameraPermission():std::string{};}
    void HideCameraPermission(){socket_.HideCameraPermission();}
    bool CaptureCameraChoice(const void* control,uint64_t view,uint64_t generation){return generation&&generation==generation_.load()&&Live()&&socket_.CaptureCameraLocalChoice(control,view);}
    void CapturePhysicalLocalProcessing(const void*,uint64_t view,uint64_t transport_generation);
private:
    enum class CommandKind {Wifi,Connect,Listen,Close};
    struct Command {CommandKind kind;uint64_t generation;ExistingRememberedPeer peer;std::string secret;wake_runtime::CapturedPhysicalGesture gesture{};};
    RememberedCredentialVault& store_;CodecTasks tasks_;CompanionSocket socket_{tasks_};
    RememberedWorkerGate gate_;std::atomic<uint64_t> generation_{0},expiry_{0};
    std::atomic<bool> started_{false},codec_fault_{false};
    TaskHandle_t task_=nullptr;std::mutex mutex_;std::deque<Command> commands_;
    std::deque<RememberedWorkerEvent> events_;
    static uint64_t LocalClock(void*)noexcept;
    wake_runtime::ConsentAuthority local_consent_{LocalClock};
    wake_runtime::PhysicalSourceDispatcher physical_source_{local_consent_};
    wake_runtime::FactoryHandle cpu_factory_;wake_runtime::DetachedCpuEndpointReceipt cpu_receipt_;
    wake_runtime::CapturedPhysicalGesture captured_gesture_;uint64_t captured_transport_=0;
    const void* physical_control_=nullptr;uint64_t physical_view_=0;
    AudioLease audio_lease_;
    bool authenticated_=false,armed_=false,owns_audio_=false,closing_=false;
    bool Queue(Command command);void Emit(RememberedWorkerEvent::Kind,ReconnectFailure failure=ReconnectFailure::Transient);
    void Run();void Execute(Command&);bool Live()const;
};
}
