// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "voice/exchange_api.hpp"
#include "voice/review_bridge.hpp"
#include "voice_exchange_crypto_fixture.hpp"
#include <cassert>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
using namespace voice_prep;
constexpr const char* call="11111111-2222-4333-8444-555555555555";
constexpr const char* session_event="{\"type\":\"hatch.voice.session\",\"call_id\":\"11111111-2222-4333-8444-555555555555\",\"session_id\":\"session-exact\",\"thread_id\":\"thread-exact\"}\n";
struct Packet {tn::ServiceFrameKind kind;int64_t stream;bool end;std::string path,body,content_type;};
struct Server {
 uint64_t nonce=0,chunk=1000;
 std::vector<std::vector<uint8_t>> MakeMany(tn::ServiceFrameView f,size_t part=16000){
  std::vector<uint8_t> frame(16000),envelope(16000);
  auto a=tn::EncodeServiceFrame(f,tn::ByteSpan(frame.data(),frame.size()));assert(a.ok());
  auto b=tn::EncodeServiceResponseEnvelope(tn::ConstByteSpan(frame.data(),a.size()),tn::ByteSpan(envelope.data(),envelope.size()));assert(b.ok());
  const auto chunks=uint32_t((b.size()+part-1)/part);const auto id=chunk++;
  std::vector<std::vector<uint8_t>> result;
  for(uint32_t index=0;index<chunks;++index){
   const auto offset=size_t(index)*part;const auto n=std::min(part,b.size()-offset);
   const tn::TransportFrameView view{id,index,chunks,tn::ConstByteSpan(envelope.data()+offset,n)};
   std::vector<uint8_t> payload(tn::ClientSession::kMaxOutboundWebSocketPayloadSize);
   auto c=tn::EncodeTransportFrame(view,tn::ByteSpan(payload.data(),payload.size()-16));assert(c.ok());
   payload.resize(c.size()+16);std::memset(payload.data()+c.size(),0,16);
   for(int i=0;i<8;++i)payload[c.size()+4+i]=uint8_t(nonce>>(56-8*i));
   ++nonce;result.push_back(std::move(payload));
  }
  return result;
 }
 std::vector<uint8_t> Make(tn::ServiceFrameView f){auto all=MakeMany(f);assert(all.size()==1);return all.front();}
 std::vector<uint8_t> Response(int64_t stream,int status,std::string body={},bool end=false){
  tn::ServiceFrameView f;f.stream_id=stream;f.kind=tn::ServiceFrameKind::Response;
  f.response.status=status;f.response.body=tn::ConstByteSpan(reinterpret_cast<const uint8_t*>(body.data()),body.size());f.response.end_body=end;return Make(f);
 }
 std::vector<uint8_t> Body(int64_t stream,std::string body,bool end=false){
  tn::ServiceFrameView f;f.stream_id=stream;f.kind=tn::ServiceFrameKind::BodyChunk;
  f.body_chunk.data=tn::ConstByteSpan(reinterpret_cast<const uint8_t*>(body.data()),body.size());f.body_chunk.end_body=end;return Make(f);
 }
};
