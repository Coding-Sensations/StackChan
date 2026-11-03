#include "existing_sc_pair.hpp"
#include <cstring>
#include <limits>
namespace companion {
namespace {
void Wipe(std::string& s){volatile char* p=s.empty()?nullptr:&s[0];for(size_t i=0;i<s.size();++i)p[i]=0;s.clear();}
struct Guard{ExistingScPairRecord& record;~Guard(){ClearExistingRecord(record);}};
}
void ClearExistingRecord(ExistingScPairRecord& r){volatile uint8_t* p=reinterpret_cast<volatile uint8_t*>(&r);for(size_t i=0;i<sizeof(r);++i)p[i]=0;}
void ExistingScPairVault::ReleaseSecret(){Wipe(secret_);handle_=0;}
PeerReadStatus ExistingScPairVault::Read(ExistingRememberedPeer& out){
    out={};ReleaseSecret();if(revoked_)return PeerReadStatus::Revoked;
    ExistingScPairRecord r{};Guard guard{r};auto result=backend_.Read(r);
    if(result==StoreResult::Missing)return PeerReadStatus::Unavailable;
    if(result!=StoreResult::Ok||r.version!=1||!std::memchr(r.device,0,sizeof(r.device))||!std::memchr(r.uri,0,sizeof(r.uri))||!std::memchr(r.ca,0,sizeof(r.ca))||!std::memchr(r.credential,0,sizeof(r.credential)))return PeerReadStatus::Malformed;
    if(!expiry_||r.expiry!=expiry_)return PeerReadStatus::TrustMismatch;
    ExistingRememberedPeer peer{r.device,r.uri,r.ca,r.expiry,1,true,true,true};
    auto valid=ValidateExistingPeer(peer,binding_,1);if(valid!=PeerReadStatus::Available)return valid;
    size_t length=std::strlen(r.credential);if(length<32||length>256)return PeerReadStatus::Malformed;
    for(size_t i=0;i<length;++i)if(static_cast<unsigned char>(r.credential[i])<33||static_cast<unsigned char>(r.credential[i])>126)return PeerReadStatus::Malformed;
    if(next_==std::numeric_limits<uint64_t>::max())return PeerReadStatus::Malformed;
    secret_=r.credential;handle_=++next_;peer.secret_handle=handle_;out=std::move(peer);return PeerReadStatus::Available;
}
bool ExistingScPairVault::Credential(uint64_t handle,std::string& out)const{Wipe(out);if(revoked_||!handle||handle!=handle_||secret_.empty())return false;out=secret_;return true;}
bool ExistingScPairVault::AuthenticatedEnrollmentStored(bool committed,bool owners_stopped){
 if(!committed||!owners_stopped)return false;
 ReleaseSecret();revoked_=false;return true;
}
StoreResult ExistingScPairVault::Revoke(){revoked_=true;ReleaseSecret();return backend_.Forget();}
}
