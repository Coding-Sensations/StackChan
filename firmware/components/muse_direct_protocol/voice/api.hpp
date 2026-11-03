// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "voice_protocol.hpp"
#include "review_bridge.hpp"
namespace voice_prep {
// Internal C++ ABI, data processing only. No authority, transport or native I/O.
// All calls on an instance must be serialized by its retained owner. The owner
// supplies trustworthy clocks, permissions and checked transport/codec cleanup.
// Construct into suitably aligned owner storage (never a worker stack). Destroy
// exactly once after cleanup; do not use a pointer or borrowed Pending thereafter.
struct Api {
 unsigned version;size_t bytes,alignment;
 Protocol* (*construct)(void*,size_t);
 void (*destroy)(Protocol*);
 decltype(&Protocol::Status) status;
 decltype(&Protocol::Pending) pending;
 decltype(&Protocol::Begin) begin;
 decltype(&Protocol::Sent) sent;
 decltype(&Protocol::Head) head;
 decltype(&Protocol::Media) media;
 decltype(&Protocol::Hold) hold;
 decltype(&Protocol::Microphone) microphone;
 decltype(&Protocol::ReadPcm) read_pcm;
 decltype(&Protocol::Cancel) cancel;
 decltype(&Protocol::EndReply) end_reply;
 decltype(&Protocol::EndConfirmed) end_confirmed;
 decltype(&Protocol::MediaEnded) media_ended;
 decltype(&Protocol::TransportClosed) transport_closed;
 decltype(&Protocol::Poll) poll;
 decltype(&Protocol::ActiveTurns) active_turns;
 decltype(&Protocol::FlushTicket) flush_ticket;
 decltype(&Protocol::AckPlaybackFlush) ack_playback_flush;
 decltype(&Protocol::ReturnedTarget) returned_target;
 // Review data projection only. Borrowed protocol/clock/custody must outlive
 // this serialized owner; no native voice permit is provided by this ABI.
 size_t review_bytes,review_alignment;
 muse_native_review::Bridge* (*construct_review)(void*,size_t,Protocol*,muse_native_review::Clock*,muse_native_review::Custody*);
 void (*destroy_review)(muse_native_review::Bridge*);
};
}
extern "C" const voice_prep::Api* stackchan_muse_voice_protocol_api();
