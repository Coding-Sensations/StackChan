#include "remembered_reconnect.hpp"
#include <algorithm>
#include <limits>
namespace companion {
namespace {
uint64_t After(uint64_t now, uint64_t delay) {
    return now>std::numeric_limits<uint64_t>::max()-delay ?
        std::numeric_limits<uint64_t>::max() : now+delay;
}
bool Bounded(const std::string& value, size_t max) {
    return !value.empty() && value.size()<=max &&
        std::all_of(value.begin(),value.end(),[](unsigned char c){return c>=33 && c<=126;});
}
bool Hash(const std::string& value) {
    return value.size()==64 && std::all_of(value.begin(),value.end(),[](char c){
        return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
}
PeerReadStatus ValidateExistingPeer(const ExistingRememberedPeer& p,
                                  const RememberedPeerBinding& b,uint64_t unix_time) {
    if(!unix_time) return PeerReadStatus::ClockUnavailable;
    if(!Bounded(b.device,64)||!Bounded(b.uri,256)||b.uri.rfind("wss://",0)!=0||!Hash(b.ca_sha256))
        return PeerReadStatus::TrustMismatch;
    if(!Bounded(p.device,64)||!Bounded(p.uri,256)||!Hash(p.ca_sha256)||!p.secret_handle||!p.expires_unix)
        return PeerReadStatus::Malformed;
    if(!p.active) return PeerReadStatus::Revoked;
    if(p.device!=b.device||p.uri!=b.uri||p.ca_sha256!=b.ca_sha256||!p.microphone||!p.speaker)
        return PeerReadStatus::TrustMismatch;
    if(unix_time>=p.expires_unix) return PeerReadStatus::Expired;
    return PeerReadStatus::Available;
}
PeerReadStatus RememberedReconnect::Start(ExistingPeerReader& reader,RememberedPeerBinding binding,
                                         uint64_t now,uint64_t unix_time) {
    // Start cannot bypass outstanding cleanup or replace a live peer.
    if(state_!=ReconnectState::Stopped) return status_;
    peer_={}; binding_=std::move(binding); attempts_=0; retry_=false;
    physical_requested_=false; action_pending_=false;
    status_=reader.Read(peer_);
    if(status_==PeerReadStatus::Available) status_=ValidateExistingPeer(peer_,binding_,unix_time);
    if(status_!=PeerReadStatus::Available) {peer_={};return status_;}
    if(generation_==std::numeric_limits<uint64_t>::max()) {
        status_=PeerReadStatus::Malformed;peer_={};return status_;
    }
    BeginAttempt(now); return status_;
}
void RememberedReconnect::BeginAttempt(uint64_t now) {
    if(generation_==std::numeric_limits<uint64_t>::max()) {Stop(PeerReadStatus::Malformed);return;}
    ++generation_; ++attempts_; physical_requested_=false;
    state_=ReconnectState::Network;deadline_=After(now,15000);action_pending_=true;
}
void RememberedReconnect::Stop(PeerReadStatus reason) {
    status_=reason; retry_=false;physical_requested_=false;action_pending_=false;
    if(state_!=ReconnectState::Stopped) state_=ReconnectState::Cleanup;
    else peer_={};
}
ReconnectAction RememberedReconnect::Tick(uint64_t now,uint64_t unix_time,bool wifi) {
    if(state_==ReconnectState::Stopped) return ReconnectAction::None;
    if(state_!=ReconnectState::Cleanup) {
        auto valid=ValidateExistingPeer(peer_,binding_,unix_time);
        if(valid!=PeerReadStatus::Available) Stop(valid);
    }
    if(state_==ReconnectState::Cleanup) return ReconnectAction::CloseAndJoin;
    if(state_==ReconnectState::Backoff) {
        if(now<deadline_) return ReconnectAction::None;
        BeginAttempt(now);
    }
    if(state_==ReconnectState::Network) {
        if(now>=deadline_) {Failed(generation_,ReconnectFailure::Transient,now);return ReconnectAction::CloseAndJoin;}
        if(action_pending_) {action_pending_=false;return ReconnectAction::EnsureSavedWifi;}
        if(wifi) {state_=ReconnectState::Connecting;deadline_=After(now,12000);return ReconnectAction::ConnectExistingPeer;}
    } else if(state_==ReconnectState::Connecting) {
        if(now>=deadline_) {Failed(generation_,ReconnectFailure::Transient,now);return ReconnectAction::CloseAndJoin;}
    } else if(state_==ReconnectState::ListenRequired && physical_requested_) {
        physical_requested_=false;state_=ReconnectState::Arming;deadline_=After(now,5000);
        return ReconnectAction::RequestPhysicalConsent;
    } else if(state_==ReconnectState::Arming && now>=deadline_) {
        Failed(generation_,ReconnectFailure::Transient,now);return ReconnectAction::CloseAndJoin;
    }
    return ReconnectAction::None;
}
bool RememberedReconnect::Authenticated(uint64_t g,uint64_t now) {
    if(g!=generation_||state_!=ReconnectState::Connecting||now>=deadline_) return false;
    state_=ReconnectState::ListenRequired;physical_requested_=false;return true;
}
bool RememberedReconnect::PhysicalListen(uint64_t g) {
    if(g!=generation_||state_!=ReconnectState::ListenRequired||physical_requested_) return false;
    physical_requested_=true;return true;
}
bool RememberedReconnect::Armed(uint64_t g,uint64_t now,uint64_t unix_time) {
    if(g!=generation_||state_!=ReconnectState::Arming||now>=deadline_) return false;
    auto valid=ValidateExistingPeer(peer_,binding_,unix_time);
    if(valid!=PeerReadStatus::Available) {Stop(valid);return false;}
    state_=ReconnectState::Armed;return true;
}
void RememberedReconnect::Failed(uint64_t g,ReconnectFailure failure,uint64_t) {
    if(g!=generation_||state_==ReconnectState::Stopped||state_==ReconnectState::Cleanup||state_==ReconnectState::Backoff) return;
    physical_requested_=false;action_pending_=false;
    retry_=failure==ReconnectFailure::Transient && attempts_<MaxAttempts;
    if(failure==ReconnectFailure::Authentication||failure==ReconnectFailure::Revoked) status_=PeerReadStatus::Revoked;
    if(failure==ReconnectFailure::Trust) status_=PeerReadStatus::TrustMismatch;
    state_=ReconnectState::Cleanup;
}
void RememberedReconnect::Mute() { Stop(status_); }
void RememberedReconnect::Revoke() { Stop(PeerReadStatus::Revoked); }
void RememberedReconnect::CleanupJoined(uint64_t g,uint64_t now) {
    if(g!=generation_||state_!=ReconnectState::Cleanup) return;
    if(retry_) {state_=ReconnectState::Backoff;deadline_=After(now,1000ULL<<(attempts_-1));}
    else {state_=ReconnectState::Stopped;peer_={};}
}
}
