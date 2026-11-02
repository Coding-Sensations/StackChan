// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "voice_protocol.hpp"
#include <xplat/noise/core/ClientSession.h>

namespace voice_prep {
// Serialized, exclusive borrowed-session adapter. The retained native owner
// must establish/verify the session, enforce current admission before every
// call and retain this storage until actual socket close/join. No I/O occurs
// here. This class does not grant microphone/output or authenticate a principal.
enum class ReceiveResult {Consumed,Busy,Failed};
class ServiceExchange {
 public:
  struct Payload { const uint8_t* data=nullptr; size_t size=0; uint64_t ticket=0; };
  ServiceExchange(Protocol&,musegadgets::noise::core::ClientSession&);
  ServiceExchange(const ServiceExchange&)=delete;
  ServiceExchange& operator=(const ServiceExchange&)=delete;
  ~ServiceExchange();
  bool Begin(Scope,int64_t now,int64_t wall_ms);
  bool Queue(int64_t now);
  Payload Next(int64_t now);
  // success means the entire captured payload was sent. A partial, failed or
  // uncertain write must report false; never resend or reuse this session.
  bool Written(uint64_t ticket,bool success,int64_t now);
  // Busy leaves all inbound bytes/crypto state untouched: retain/retry them.
  ReceiveResult Receive(const uint8_t*,size_t,int64_t now);
  bool Cancel(int64_t now);
  bool Abort();
  bool Joined(bool native_transport_joined);
  bool NeedsTransportClose()const;
  bool CleanupComplete()const;
 private:
  Protocol& protocol_;
  musegadgets::noise::core::ClientSession& session_;
  std::array<uint8_t,4608> service_{},envelope_{};
  std::array<uint8_t,musegadgets::noise::core::ClientSession::kMaxOutboundWebSocketPayloadSize> websocket_{};
  std::array<uint8_t,12288> frame_{},response_{};
  std::array<musegadgets::noise::core::HeaderView,16> headers_{};
  std::array<char,512> end_body_{};
  uint64_t generation_=0,sequence_=0,ticket_=0;
  int64_t media_=0,end_=0,last_=0;
  size_t end_size_=0;
  int end_status_=0;
  Kind queued_kind_=Kind::None;
  bool bound_=false,spent_=false,failed_=false,closed_=false;
  bool queued_=false,awaiting_write_=false,media_head_=false,end_head_=false,end_dispatched_=false;
  bool Guard(int64_t now);
  bool Frame(const musegadgets::noise::core::DecodedServiceFrame&,int64_t now);
  void Wipe();
};
}
