#pragma once
#include "remembered_peer.hpp"
namespace companion {
enum class StoreResult { Ok, Missing, Denied, Malformed, IoFailure, ProtectionUnavailable };
// Private credential ownership shared by production reader and offline fixtures.
// Production reconnect never creates, migrates or overwrites a credential.
class RememberedCredentialVault : public ExistingPeerReader {
public:
    virtual bool Credential(uint64_t handle,std::string& out)const=0;
    virtual void ReleaseSecret()=0;
    virtual StoreResult Revoke()=0;
};
}
