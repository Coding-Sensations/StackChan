#pragma once
#include "remembered_vault.hpp"
#include <cstddef>
namespace companion {
// Installed 4f7fe49… source's exact little-endian ESP32-S3 PairRecord ABI.
// Never log, diagnose, serialize elsewhere or migrate this record.
struct ExistingScPairRecord {uint32_t version=0,expiry=0;char device[64]={},uri[128]={},ca[65]={},credential[257]={};};
static_assert(sizeof(ExistingScPairRecord)==524);
static_assert(offsetof(ExistingScPairRecord,device)==8&&offsetof(ExistingScPairRecord,credential)==265);
void ClearExistingRecord(ExistingScPairRecord&);
class ExistingScPairBackend {
public:
    virtual ~ExistingScPairBackend()=default;
    virtual StoreResult Read(ExistingScPairRecord&)=0;
    // Explicit Forget only, never called by startup/read/retry/mute.
    virtual StoreResult Forget()=0;
};
class NvsExistingScPairBackend final : public ExistingScPairBackend {
public:
    StoreResult Read(ExistingScPairRecord&) override;
    StoreResult Forget() override;
};
class ExistingScPairVault final : public RememberedCredentialVault {
public:
    ExistingScPairVault(ExistingScPairBackend& backend,RememberedPeerBinding binding,uint32_t expiry)
        :backend_(backend),binding_(std::move(binding)),expiry_(expiry){}
    ~ExistingScPairVault(){ReleaseSecret();}
    PeerReadStatus Read(ExistingRememberedPeer&) override;
    bool Credential(uint64_t handle,std::string& out)const override;
    void ReleaseSecret() override;
    StoreResult Revoke() override;
    // Only the installed authenticated Remember workflow may release the latch,
    // after its durable stored acknowledgement and joined audio cleanup.
    bool AuthenticatedEnrollmentStored(bool committed,bool owners_stopped);
private:
    ExistingScPairBackend& backend_;RememberedPeerBinding binding_;uint32_t expiry_;
    std::string secret_;uint64_t handle_=0,next_=0;bool revoked_=false;
};
}
