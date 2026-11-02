// SPDX-License-Identifier: Apache-2.0
#include "service_exchange.hpp"
#include <cassert>
#include <limits>

namespace voice_prep {
namespace wire=musegadgets::noise::core;
namespace {
void Zero(void* storage,size_t size){auto* p=static_cast<volatile unsigned char*>(storage);while(size--)*p++=0;}
}
ServiceExchange::ServiceExchange(Protocol& protocol,wire::ClientSession& session):protocol_(protocol),session_(session){}
ServiceExchange::~ServiceExchange(){assert(!bound_);Wipe();}
void ServiceExchange::Wipe(){
 Zero(service_.data(),service_.size());Zero(envelope_.data(),envelope_.size());
 Zero(websocket_.data(),websocket_.size());Zero(frame_.data(),frame_.size());
 Zero(response_.data(),response_.size());Zero(headers_.data(),sizeof(headers_));
 Zero(end_body_.data(),end_body_.size());end_size_=0;
}
bool ServiceExchange::Begin(Scope scope,int64_t now,int64_t wall_ms){
 if(spent_||!session_.isEstablished()||session_.HasOutboundWebSocketPayload()||!protocol_.Begin(scope,now,wall_ms))return false;
 spent_=bound_=true;generation_=scope.generation;media_=scope.media_stream;end_=scope.end_stream;last_=now;return true;
}
bool ServiceExchange::Guard(int64_t now){
 if(!bound_||failed_||closed_)return false;
 if(now<last_||!session_.isEstablished()){Abort();return false;}
 last_=now;
 if(!protocol_.Poll(generation_,now)){Abort();return false;}
 return true;
}
bool ServiceExchange::Queue(int64_t now){
 if(!Guard(now)||queued_)return false;
 const auto& op=protocol_.Pending();
 if(op.kind==Kind::None)return false;
 if(op.generation!=generation_||!op.sequence||op.length>=op.body.size()||session_.HasOutboundWebSocketPayload()){Abort();return false;}
 if((op.kind==Kind::OpenEndCall||op.kind==Kind::ResetEndCall)?op.stream!=end_:op.stream!=media_){Abort();return false;}
 wire::StatusWithSize encoded{wire::Status::InvalidArgument(),0};
 const auto body=wire::ConstByteSpan(reinterpret_cast<const uint8_t*>(op.body.data()),op.length);
 switch(op.kind){
  case Kind::OpenMedia:
  case Kind::OpenEndCall:{
   if(!std::memchr(op.path.data(),0,op.path.size())){Abort();return false;}
   const wire::HeaderView headers[]={
    {wire::StringView("Content-Type"),wire::StringView(op.kind==Kind::OpenMedia?"application/x-ndjson; charset=utf-8":"application/json")},
    {wire::StringView("Accept"),wire::StringView("application/x-ndjson")}};
   wire::ApplicationRequestView request;
   request.verb=wire::StringView("POST");request.path=wire::StringView(op.path.data());
   request.headers=wire::Span<const wire::HeaderView>(headers,op.kind==Kind::OpenMedia?2:1);
   request.body=body;request.end_body=op.kind==Kind::OpenEndCall;
   encoded=session_.StartOutboundApplicationRequest(wire::ServiceType::Daemon,op.stream,request,wire::ByteSpan(service_),wire::ByteSpan(envelope_));break;
  }
  case Kind::Append:
  case Kind::HalfClose:{
   const wire::BodyChunkView chunk{body,op.kind==Kind::HalfClose};
   encoded=session_.StartOutboundBodyChunk(wire::ServiceType::Daemon,op.stream,chunk,wire::ByteSpan(service_),wire::ByteSpan(envelope_));break;
  }
  case Kind::ResetMedia:
  case Kind::ResetEndCall:{
   const wire::ResetView reset{wire::ResetCode::Cancelled,wire::StringView("voice closed")};
   encoded=session_.StartOutboundReset(wire::ServiceType::Daemon,op.stream,reset,wire::ByteSpan(service_),wire::ByteSpan(envelope_));break;
  }
  case Kind::None:return false;
 }
 if(!encoded.ok()||!session_.HasOutboundWebSocketPayload()){Abort();return false;}
 sequence_=op.sequence;queued_kind_=op.kind;queued_=true;return true;
}
ServiceExchange::Payload ServiceExchange::Next(int64_t now){
 if(!Guard(now)||!queued_||awaiting_write_)return {};
 if(protocol_.Pending().sequence!=sequence_||ticket_==std::numeric_limits<uint64_t>::max()){Abort();return {};}
 const auto result=session_.WriteNextOutboundWebSocketPayload(wire::ByteSpan(websocket_));
 if(!result.ok()||!result.size()){Abort();return {};}
 awaiting_write_=true;return {websocket_.data(),result.size(),++ticket_};
}
bool ServiceExchange::Written(uint64_t ticket,bool success,int64_t now){
 // A stale or duplicate callback cannot acknowledge or revoke a newer payload.
 if(!bound_||!awaiting_write_||!ticket||ticket!=ticket_)return false;
 if(!success){Abort();return false;}
 if(!Guard(now)||protocol_.Pending().sequence!=sequence_){Abort();return false;}
 awaiting_write_=false;Zero(websocket_.data(),websocket_.size());
 if(session_.HasOutboundWebSocketPayload())return true;
 const auto kind=queued_kind_;queued_=false;queued_kind_=Kind::None;
 if(!protocol_.Sent(generation_,sequence_,true,now)){Abort();return false;}
 if(kind==Kind::OpenEndCall)end_dispatched_=true;
 Zero(service_.data(),service_.size());Zero(envelope_.data(),envelope_.size());return true;
}
bool ServiceExchange::Frame(const wire::DecodedServiceFrame& frame,int64_t now){
 const bool head=frame.kind==wire::ServiceFrameKind::Response;
 if(!head&&frame.kind!=wire::ServiceFrameKind::BodyChunk)return false;
 const auto body=head?frame.response.body:frame.body_chunk.data;
 const bool ended=head?frame.response.end_body:frame.body_chunk.end_body;
 if(body.size()&&!body.data())return false;
 if(frame.stream_id==media_){
  if(head){if(media_head_||!protocol_.Head(generation_,media_,frame.response.status,now))return false;media_head_=true;}
  if(!media_head_)return false;
  if(body.size()&&!protocol_.Media(generation_,media_,reinterpret_cast<const char*>(body.data()),body.size(),now))return false;
  return !ended||protocol_.MediaEnded(generation_,media_,true,head?frame.response.status:200,now);
 }
 if(frame.stream_id==end_){
  if(!end_dispatched_)return false;
  if(head){if(end_head_||frame.response.status<200||frame.response.status>=300)return false;end_head_=true;end_status_=frame.response.status;}
  if(!end_head_||body.size()>end_body_.size()-end_size_)return false;
  if(body.size())std::memcpy(end_body_.data()+end_size_,body.data(),body.size());
  end_size_+=body.size();
  if(!ended)return true;
  const bool okay=protocol_.EndReply(generation_,end_,end_status_,end_body_.data(),end_size_,now);
  Zero(end_body_.data(),end_body_.size());end_size_=0;return okay;
 }
 return false;
}
ReceiveResult ServiceExchange::Receive(const uint8_t* payload,size_t size,int64_t now){
 if(!Guard(now))return ReceiveResult::Failed;
 if(queued_)return ReceiveResult::Busy; // drain outbound before consuming a view
 if(!payload||!size||size>websocket_.size()){Abort();return ReceiveResult::Failed;}
 const auto decoded=session_.ProcessInboundWebSocketPayload(wire::ConstByteSpan(payload,size),wire::ByteSpan(frame_),wire::ByteSpan(response_),wire::Span<wire::HeaderView>(headers_));
 const bool okay=decoded.ok()&&(decoded.frame_status!=wire::InboundFrameStatus::Complete||Frame(decoded.frame,now));
 Zero(frame_.data(),frame_.size());Zero(headers_.data(),sizeof(headers_));
 // response_ is SDK reassembly storage; preserve it until a complete frame.
 if(decoded.frame_status==wire::InboundFrameStatus::Complete)Zero(response_.data(),response_.size());
 if(!okay){Abort();return ReceiveResult::Failed;}return ReceiveResult::Consumed;
}
bool ServiceExchange::Cancel(int64_t now){
 if(!bound_||closed_||failed_)return false;
 if(queued_){Abort();return false;}
 if(!Guard(now))return false;
 return protocol_.Cancel(generation_,now);
}
bool ServiceExchange::Abort(){
 if(!bound_||closed_)return false;
 failed_=true;protocol_.TransportFailed(generation_);return true;
}
bool ServiceExchange::Joined(bool native_transport_joined){
 if(!bound_||closed_||!native_transport_joined)return false;
 protocol_.TransportFailed(generation_);
 if(!protocol_.TransportClosed(generation_,true))return false;
 bound_=false;closed_=true;queued_=awaiting_write_=false;Wipe();return true;
}
bool ServiceExchange::NeedsTransportClose()const{return bound_&&(failed_||protocol_.Status()==State::AwaitTransportClose);}
bool ServiceExchange::CleanupComplete()const{return !bound_;}
}
