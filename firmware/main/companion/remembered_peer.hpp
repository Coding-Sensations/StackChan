#pragma once
#include <cstdint>
#include <string>
namespace companion {
// Metadata only. The adapter/transport retains the bearer privately; this handle
// must never be interpreted as a credential or persisted by the controller.
struct ExistingRememberedPeer {
    std::string device, uri, ca_sha256;
    uint64_t expires_unix=0, secret_handle=0;
    bool active=false, microphone=false, speaker=false;
};
struct RememberedPeerBinding { std::string device, uri, ca_sha256; };
enum class PeerReadStatus { Available, Unavailable, UnsupportedFormat, Malformed,
    TrustMismatch, Expired, Revoked, ClockUnavailable };
class ExistingPeerReader {
public:
    virtual ~ExistingPeerReader()=default;
    // Read only, previously enrolled authority only. Never pair, renew, write,
    // recover from a host hash, or promote a temporary pair.grant.
    virtual PeerReadStatus Read(ExistingRememberedPeer& out)=0;
};
// No durable robot format/writer has been identified in the reviewed firmware.
// Deliberately no NVS guess, test fixture fallback, or enrollment side effect.
class UnavailableExistingPeerReader final : public ExistingPeerReader {
public:
    PeerReadStatus Read(ExistingRememberedPeer& out) override {
        out={}; return PeerReadStatus::Unavailable;
    }
};
PeerReadStatus ValidateExistingPeer(const ExistingRememberedPeer& peer,
                                  const RememberedPeerBinding& binding,
                                  uint64_t now_unix);
}
