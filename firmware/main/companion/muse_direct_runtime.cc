// SPDX-License-Identifier: Apache-2.0
#include "muse_direct_runtime.h"
#include "muse_direct_store.h"
#include "muse_direct_credential_custody.hpp"
#include "muse_direct_api.h"
#include "muse_direct_native_socket.h"
#include <new>
#include <xplat/noise/core/MbedtlsCryptoBackend.h>
namespace companion::muse {
static PairingSession* Pair(CredentialStore& store,DeviceVerifier& verifier){return new(std::nothrow)PairingSession(store,verifier);}
static NoiseChatOwner* Chat(NoiseSocket& socket){return new(std::nothrow)NoiseChatOwner(socket);}
static wire::CryptoBackend* Crypto(){return new(std::nothrow)wire::MbedtlsCryptoBackend();}
static CredentialStore* Store(){
#if STACKCHAN_MUSE_RAM_ONLY
 auto* lease=new(std::nothrow)CredentialLease();
 if(lease&&!lease->Acquire()){delete lease;lease=nullptr;}return lease;
#else
 return new(std::nothrow)DirectNvsStore();
#endif
}
static DeviceVerifier* Verifier(std::function<bool()> current){return new(std::nothrow)DirectApiVerifier(std::move(current));}
static NoiseSocket* Socket(std::function<bool()> current){return current?new(std::nothrow)DirectMetaSocket(std::move(current)):nullptr;}
static const RuntimeApi api{Pair,Chat,Crypto,Store,Verifier,&PairingSession::Open,&PairingSession::Receive,&PairingSession::PhysicalPress,&PairingSession::Close,
    &NoiseChatOwner::Open,&NoiseChatOwner::Poll,&NoiseChatOwner::Submit,&NoiseChatOwner::Close,
    Socket,sizeof(DirectMetaSocket),sizeof(PairingSession),sizeof(NoiseChatOwner),NoiseChatOwner::BufferBytes()};
}
extern "C" const companion::muse::RuntimeApi* stackchan_muse_direct_runtime(){return &companion::muse::api;}
