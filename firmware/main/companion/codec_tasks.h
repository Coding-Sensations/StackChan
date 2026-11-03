#pragma once
#include "borrowed_cpu_endpoint.hpp"
#include "detached_cpu_endpoint_receipt.hpp"
#include "audio_session.hpp"
#include "companion/audio_owner.hpp"
#include "audio_wire.hpp"
#include <audio/audio_codec.h>
#include <esp_ae_rate_cvt.h>
#include <freertos/task.h>
#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <vector>
#include <array>
#include <memory>
namespace companion {
// Exclusive BSP codec owner. No constructor IO, NVS, network, camera or motion.
// Retain this object until Stop() returns true; no forced task deletion.
class CodecTasks {
public:
    using Send=std::function<bool(const uint8_t*,std::size_t)>;
    // Observer must return promptly without throwing; it receives a scalar only.
    // The owner validates generation/epoch before applying it to UI state.
    using LevelObserver=std::function<void(uint64_t,uint32_t,uint16_t,bool)>;
    bool ConfigureLevelObserver(uint64_t generation,LevelObserver observer);
    using Stopped=std::function<void(uint32_t,bool)>;
    bool IsActive() const { return enabled_.load() && !done_.load(); }
    bool IsStopped() const { return done_.load(); }
    ~CodecTasks();
    bool BindAudioLease(AudioLease lease);
    wake_runtime::DetachedCpuEndpointReceipt DetachBorrowedCpuAfterStop(wake_runtime::BorrowedCpuEndpoint* endpoint){std::lock_guard<std::mutex> lock(control_mutex_);if(!done_||stop_admission_||!endpoint||borrowed_cpu_!=endpoint||!CommonAudioAuthority().Holds(lease_))return {};borrowed_cpu_=nullptr;borrowed_proof_pending_=false;return {endpoint,lease_.generation()};}
    bool BindBorrowedCpu(wake_runtime::BorrowedCpuEndpoint* endpoint){std::lock_guard<std::mutex> lock(control_mutex_);if(!done_||stop_admission_)return false;borrowed_cpu_=endpoint;return true;}
    // Typed proof is staged by the owner; a running worker never takes UI object references.
    bool StageBorrowedPhysicalProof(AudioLease lease,uint32_t media,wake_runtime::ExplicitLocalConsent proof);
    bool Start(AudioCodec* codec,Send send,Stopped stopped);
    bool ResetStoppedConnection();
    bool Stop(uint32_t timeout_ms=2000);
    bool Begin(uint32_t epoch);
    bool Acknowledge(uint32_t epoch);
    bool Consent(uint32_t epoch,bool physical,const std::function<bool()>& permission={});
    bool Heartbeat(uint32_t epoch);
    bool Playing(uint32_t epoch,bool playing);
    void Interrupt(uint32_t epoch);
    bool Receive(const uint8_t* wire,std::size_t bytes);
private:
    // Packet scratch must not consume the codec task stack during native Opus calls.
    struct Scratch {std::array<uint8_t,opus_max> opus{};std::array<uint8_t,wire_header+opus_max> wire{};};
    std::unique_ptr<Scratch> scratch_;
    struct Packet {
        uint32_t epoch=0; std::vector<uint8_t> payload;
        Packet()=default;
        Packet(uint32_t e,std::vector<uint8_t> p):epoch(e),payload(std::move(p)){}
        Packet(Packet&&)=default;Packet& operator=(Packet&&)=default;
        ~Packet(){volatile uint8_t* p=payload.data();for(auto n=payload.size();n;--n)*p++=0;}
    };
    std::mutex control_mutex_;bool stop_admission_=false;
    wake_runtime::BorrowedCpuEndpoint* borrowed_cpu_=nullptr;
    wake_runtime::ExplicitLocalConsent borrowed_proof_;uint32_t borrowed_proof_epoch_=0;bool borrowed_proof_pending_=false;bool borrowed_cpu_armed_=false;
    AudioLease lease_{};
    AudioSession session_; std::mutex mutex_; std::deque<Packet> incoming_;
    std::atomic<uint32_t> epoch_{0}; std::atomic<bool> enabled_{false},playing_{false},playback_known_{false},quit_{false},done_{true};
    uint64_t level_generation_=0;LevelObserver level_observer_;
    void ObserveLevel(uint32_t epoch,const std::vector<int16_t>& pcm,bool playback);
    AudioCodec* codec_=nullptr; Send send_; Stopped stopped_; TaskHandle_t task_=nullptr;
    void* encoder_=nullptr; void* decoder_=nullptr; esp_ae_rate_cvt_handle_t resampler_=nullptr;
    std::vector<int16_t> pending_; uint32_t mic_sequence_=0;
    void WipePending();bool Input(bool enable);bool Output(bool enable);bool CloseDirections();
    bool OpenCodecs(); void CloseCodecs(); void Run(); bool Current(uint32_t epoch);
};
}
