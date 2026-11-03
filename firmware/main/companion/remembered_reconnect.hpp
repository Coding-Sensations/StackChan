#pragma once
#include "remembered_peer.hpp"
namespace companion {
enum class ReconnectState { Stopped, Network, Backoff, Connecting, ListenRequired,
                            Arming, Armed, Cleanup };
enum class ReconnectAction { None, EnsureSavedWifi, ConnectExistingPeer,
                             RequestPhysicalConsent, CloseAndJoin };
enum class ReconnectFailure { Transient, Authentication, Trust, Revoked };
// Single owner task only; callbacks are delivered to that owner with generation.
// Actions are nonblocking requests. CloseAndJoin must acknowledge joined socket
// callbacks AND stopped codecs before another attempt; no worker is destroyed.
class RememberedReconnect {
public:
    static constexpr unsigned MaxAttempts=5;
    PeerReadStatus Start(ExistingPeerReader& reader, RememberedPeerBinding binding,
                         uint64_t monotonic_ms, uint64_t now_unix);
    ReconnectAction Tick(uint64_t monotonic_ms, uint64_t now_unix,
                         bool wifi_connected);
    bool Authenticated(uint64_t generation, uint64_t monotonic_ms);
    bool PhysicalListen(uint64_t generation);
    bool Armed(uint64_t generation, uint64_t monotonic_ms, uint64_t now_unix);
    void Failed(uint64_t generation, ReconnectFailure failure, uint64_t monotonic_ms);
    void Mute();
    void Revoke();
    void CleanupJoined(uint64_t generation, uint64_t monotonic_ms);
    ReconnectState State() const { return state_; }
    PeerReadStatus Status() const { return status_; }
    uint64_t Generation() const { return generation_; }
    unsigned Attempts() const { return attempts_; }
    // Transport reads metadata/opaque handle only during a connect action.
    const ExistingRememberedPeer& Peer() const { return peer_; }
private:
    ExistingRememberedPeer peer_;
    RememberedPeerBinding binding_;
    ReconnectState state_=ReconnectState::Stopped;
    PeerReadStatus status_=PeerReadStatus::Unavailable;
    uint64_t generation_=0, deadline_=0;
    unsigned attempts_=0;
    bool retry_=false, action_pending_=false, physical_requested_=false;
    void Stop(PeerReadStatus reason);
    void BeginAttempt(uint64_t monotonic_ms);
};
}
