#pragma once
#include "remembered_reconnect.hpp"
#include "remembered_vault.hpp"
namespace companion {
// Nonblocking worker boundary. Implementer must privately consume the reader's
// handle, use validated TLS and a fresh request, and marshal events back to owner.
// No direct codec work or synchronous Connect under the LVGL lock.
class RememberedStartupPort {
public:
    virtual ~RememberedStartupPort()=default;
    virtual void EnsureSavedWifi(uint64_t generation)=0;
    virtual void ConnectExisting(const ExistingRememberedPeer&,uint64_t generation)=0;
    virtual void RequestPhysicalConsent(uint64_t generation)=0;
    virtual void CloseAndJoin(uint64_t generation)=0;
    virtual void InterruptImmediately()=0;
};
class RememberedStartup {
public:
    RememberedStartup(RememberedCredentialVault& reader,RememberedStartupPort& port)
        :reader_(reader),port_(port){}
    PeerReadStatus Start(RememberedPeerBinding binding,uint64_t ms,uint64_t wall,bool startup_approved){
        if(!startup_approved)return PeerReadStatus::Unavailable;
        auto status=controller_.Start(reader_,std::move(binding),ms,wall);
        if(status!=PeerReadStatus::Available)reader_.ReleaseSecret();
        return status;
    }
    void Tick(uint64_t ms,uint64_t wall,bool wifi){
        auto action=controller_.Tick(ms,wall,wifi);auto g=controller_.Generation();
        switch(action){
        case ReconnectAction::EnsureSavedWifi:port_.EnsureSavedWifi(g);break;
        case ReconnectAction::ConnectExistingPeer:port_.ConnectExisting(controller_.Peer(),g);break;
        case ReconnectAction::RequestPhysicalConsent:port_.RequestPhysicalConsent(g);break;
        case ReconnectAction::CloseAndJoin:port_.InterruptImmediately();port_.CloseAndJoin(g);break;
        case ReconnectAction::None:break;
        }
    }
    void Mute(){port_.InterruptImmediately();controller_.Mute();}
    StoreResult Revoke(){port_.InterruptImmediately();controller_.Revoke();return reader_.Revoke();}
    // Event delivery is single-owner/generation-scoped, no callback mutation.
    RememberedReconnect& Events(){return controller_;}
private:
    RememberedCredentialVault& reader_;RememberedStartupPort& port_;RememberedReconnect controller_;
};
}
