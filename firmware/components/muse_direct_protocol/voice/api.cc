// SPDX-License-Identifier: Apache-2.0
#include "api.hpp"
#include <new>
#include <cstdint>
namespace {
using voice_prep::Protocol;
Protocol* Construct(void* storage,size_t bytes){
 if(!storage||bytes<sizeof(Protocol)||reinterpret_cast<uintptr_t>(storage)%alignof(Protocol))return nullptr;
 return ::new(storage) Protocol();
}
void Destroy(Protocol* p){if(p)p->~Protocol();}
muse_native_review::Bridge* ConstructReview(void* storage,size_t bytes,Protocol* protocol,muse_native_review::Clock* clock,muse_native_review::Custody* custody){
 using muse_native_review::Bridge;
 if(!storage||bytes<sizeof(Bridge)||reinterpret_cast<uintptr_t>(storage)%alignof(Bridge)||!protocol||bool(clock)!=bool(custody))return nullptr;
 return clock?::new(storage) Bridge(*protocol,*clock,*custody) : ::new(storage) Bridge(*protocol);
}
void DestroyReview(muse_native_review::Bridge* p){if(p){p->~Bridge();muse_review::Wipe(p,sizeof(*p));}}
constexpr voice_prep::Api api{2,sizeof(Protocol),alignof(Protocol),Construct,Destroy,
 &Protocol::Status,&Protocol::Pending,&Protocol::Begin,&Protocol::Sent,&Protocol::Head,
 &Protocol::Media,&Protocol::Hold,&Protocol::Microphone,&Protocol::ReadPcm,&Protocol::Cancel,
 &Protocol::EndReply,&Protocol::EndConfirmed,&Protocol::MediaEnded,&Protocol::TransportClosed,
 &Protocol::Poll,&Protocol::ActiveTurns,&Protocol::FlushTicket,&Protocol::AckPlaybackFlush,
 &Protocol::ReturnedTarget,sizeof(muse_native_review::Bridge),alignof(muse_native_review::Bridge),ConstructReview,DestroyReview};
}
// Deliberately dormant link root. No constructor, task, codec, socket or token.
extern "C" const voice_prep::Api* stackchan_muse_voice_protocol_api(){return &api;}
