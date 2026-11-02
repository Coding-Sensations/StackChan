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
struct Fixture {
 NonceTagCrypto crypto;tn::ClientSession session{crypto};Protocol protocol;ServiceExchange exchange{protocol,session};
 Server server;tn::OrderedTransportFramer outbound;int64_t now=0;
 Fixture(){Establish(session);assert(exchange.Begin({7,call,"synthetic-device",7,9,10000000},now,1760000000000LL));}
 ~Fixture(){if(!exchange.CleanupComplete()){assert(exchange.Abort());assert(exchange.Joined(true));}}
 Packet Send(){
  assert(exchange.Queue(++now));auto p=exchange.Next(++now);assert(p.size&&p.ticket);
  assert(!exchange.Next(now).size);assert(!exchange.Written(p.ticket+1,true,now));
  std::array<uint8_t,16000> scratch{};tn::ConstByteSpan view;
  auto decoded=outbound.DecodeInboundFrame(tn::ConstByteSpan(p.data,p.size-16),tn::ByteSpan(scratch),view);assert(decoded.frame_status==tn::InboundFrameStatus::Complete);
  tn::DecodedServiceRequestEnvelope envelope;assert(tn::DecodeServiceRequestEnvelopeDetailed(view,envelope).ok()&&envelope.service==tn::ServiceType::Daemon);
  tn::DecodedServiceFrame f;std::array<tn::HeaderView,16> headers{};assert(tn::DecodeServiceFrameDetailed(envelope.payload,tn::Span<tn::HeaderView>(headers),f).ok());
  const bool request=f.kind==tn::ServiceFrameKind::Request;
  auto body=request?f.request.body:f.body_chunk.data;
  Packet result{f.kind,f.stream_id,request?f.request.end_body:f.body_chunk.end_body,request?std::string(f.request.path.data(),f.request.path.size()):"",body.size()?std::string(reinterpret_cast<const char*>(body.data()),body.size()):"",""};
  for(const auto& h:headers)if(h.key.size()&&std::string(h.key.data(),h.key.size())=="Content-Type")result.content_type=std::string(h.value.data(),h.value.size());
  assert(exchange.Written(p.ticket,true,++now));for(size_t i=0;i<p.size;++i)assert(p.data[i]==0);assert(!exchange.Written(p.ticket,true,now));return result;
 }
 bool Receive(const std::vector<uint8_t>& p){return exchange.Receive(p.data(),p.size(),++now)==ReceiveResult::Consumed;}
 void Live(){auto p=Send();assert(p.kind==tn::ServiceFrameKind::Request&&p.stream==7&&!p.end&&p.path==std::string("/voice/realtime?call_id=")+call);assert(p.content_type=="application/x-ndjson; charset=utf-8");assert(p.body.find("synthetic-device")!=std::string::npos);
  assert(Receive(server.Response(7,200)));const std::string e=session_event;assert(Receive(server.Body(7,e.substr(0,57))));assert(protocol.Status()==State::Opening);assert(Receive(server.Body(7,e.substr(57))));assert(protocol.Status()==State::Live);
 }
 void MustRetain(){assert(exchange.NeedsTransportClose());assert(!exchange.CleanupComplete());assert(!exchange.Joined(false));assert(!exchange.CleanupComplete());assert(exchange.Joined(true));assert(exchange.CleanupComplete()&&protocol.Status()==State::Done);assert(!exchange.Joined(true));assert(!exchange.Queue(++now));}
};
int main(){
 {
  Fixture f;f.Send();tn::ServiceFrameView response;response.kind=tn::ServiceFrameKind::Response;response.stream_id=7;response.response.status=200;
  std::string padding(9000,'x');const tn::HeaderView headers[]={{tn::StringView("X-Padding"),tn::StringView(padding.data(),padding.size())}};response.response.headers=tn::Span<const tn::HeaderView>(headers);
  response.response.body=tn::ConstByteSpan(reinterpret_cast<const uint8_t*>(session_event),std::strlen(session_event));auto packets=f.server.MakeMany(response,512);assert(packets.size()>1);
  for(size_t i=0;i<packets.size();++i){assert(f.Receive(packets[i]));assert(f.protocol.Status()==(i+1==packets.size()?State::Live:State::Opening));}
  TargetSnapshot target;assert(f.protocol.ReturnedTarget(7,++f.now,target));assert(!std::strcmp(target.session_id.data(),"session-exact"));
 }
 {
  Fixture f;f.Live();assert(f.exchange.Cancel(++f.now));while(f.protocol.Pending().kind!=Kind::None)f.Send();assert(f.Receive(f.server.Response(9,200,"{\"ok\":true}",true)));
  auto half=f.Send();assert(half.kind==tn::ServiceFrameKind::BodyChunk&&half.end);f.now+=2000001;auto reset=f.Send();assert(reset.kind==tn::ServiceFrameKind::Reset&&reset.stream==7);f.MustRetain();
 }
 {
  Fixture f;f.Live();assert(f.exchange.Cancel(++f.now));auto end=f.Send();assert(end.path=="/voice/end-call");
  assert(f.Receive(f.server.Body(7,"{\"type\":\"response.done\"}\n",true)));
  assert(f.protocol.Status()==State::Ending&&!f.exchange.NeedsTransportClose());
  assert(f.Receive(f.server.Response(9,200,"{\"ok\":true}",true)));assert(f.protocol.EndConfirmed());
  f.MustRetain();
 }

 {
  Fixture f;assert(f.exchange.Queue(++f.now));auto p=f.exchange.Next(++f.now);auto response=f.server.Response(7,200);
  assert(f.exchange.Receive(response.data(),response.size(),++f.now)==ReceiveResult::Busy);
  assert(f.exchange.Written(p.ticket,true,++f.now));assert(f.Receive(response));
 }

 for(const char* event : {"{\"type\":\"conversation.item.added\",\"item\":{\"type\":\"session_control\",\"name\":\"stop\"}}\n", "{\"type\":\"error\",\"error\":{\"code\":\"session_dead\"}}\n"}){
  Fixture f;f.Live();assert(f.Receive(f.server.Body(7,event)));assert(f.protocol.Status()==State::Ending);assert(!f.exchange.NeedsTransportClose());
  bool ended=false;while(f.protocol.Pending().kind!=Kind::None){auto p=f.Send();if(p.path=="/voice/end-call"){ended=true;assert(p.content_type=="application/json");}}assert(ended);
 }

 {
  NonceTagCrypto c;tn::ClientSession s(c);Protocol p;ServiceExchange x(p,s);
  assert(!x.Begin({7,call,"device",7,9,10000},0,1760000000000LL));assert(p.Status()==State::Idle&&x.CleanupComplete());
 }
 {
  Fixture f;f.Live();TargetSnapshot t;assert(f.protocol.ReturnedTarget(7,++f.now,t));
  assert(!std::strcmp(t.session_id.data(),"session-exact")&&!std::strcmp(t.thread_id.data(),"thread-exact"));
  muse_native_review::Bridge review(f.protocol);assert(!review.Open()&&!review.Review().NativePermit());
  assert(!f.exchange.Begin({8,call,"other",11,13,10000},f.now,1760000000000LL));
  assert(!f.exchange.Queue(++f.now));assert(!f.exchange.NeedsTransportClose());
 }
 {
  Fixture f;f.Live();assert(f.protocol.Hold(7,true,++f.now));assert(f.protocol.AckPlaybackFlush(7,f.protocol.FlushTicket()));f.now+=200000;
  int16_t samples[320]{};for(int i=0;i<3&&f.protocol.Pending().kind==Kind::None;++i){auto n=f.protocol.Microphone(7,samples,320,++f.now);assert(n>0&&n<=320);}
  assert(f.protocol.Pending().kind==Kind::Append);auto p=f.Send();assert(p.kind==tn::ServiceFrameKind::BodyChunk&&!p.end&&p.body.find("input_audio_buffer.append")!=std::string::npos);
 }
 {
  Fixture f;f.Live();assert(!f.Receive(f.server.Body(7,"not-json\n")));f.MustRetain();
 }
 for(int error=0;error<6;++error){
  Fixture f;f.Send();std::vector<uint8_t> p;
  switch(error){
   case 0:p=f.server.Body(7,session_event);break; // body before response
   case 1:p=f.server.Response(11,200);break; // wrong stream
   case 2:p=f.server.Response(9,200,"{\"ok\":true}",true);break; // end before dispatch
   case 3:p=f.server.Response(7,401);break;
   case 4:assert(f.Receive(f.server.Response(7,200)));p=f.server.Response(7,200);break;
   case 5:p=f.server.Response(7,200,std::string(5000,'x'));break;
  }
  assert(!f.Receive(p));f.MustRetain();
 }
 {
  Fixture f;f.Live();assert(f.exchange.Cancel(++f.now));bool end=false;
  for(int i=0;i<8&&f.protocol.Pending().kind!=Kind::None;++i){auto p=f.Send();if(p.path=="/voice/end-call"){end=true;assert(p.end&&p.stream==9&&p.body.find(call)!=std::string::npos);}}
  assert(end);assert(!f.protocol.EndConfirmed());assert(f.Receive(f.server.Response(9,200,"{\"ok\":")));assert(!f.protocol.EndConfirmed());assert(f.Receive(f.server.Body(9,"true}",true)));assert(f.protocol.EndConfirmed());
  assert(f.protocol.Pending().kind==Kind::HalfClose);auto half=f.Send();assert(half.kind==tn::ServiceFrameKind::BodyChunk&&half.end&&half.body.empty());
  assert(f.Receive(f.server.Body(7,"",true)));
  // End-call ACK and media EOF are not proof of socket join/resource release.
  f.MustRetain();
 }
 for(int error=0;error<3;++error){
  Fixture f;f.Send();tn::ServiceFrameView frame;frame.stream_id=7;
  if(error==0){frame.kind=tn::ServiceFrameKind::Reset;frame.reset.code=tn::ResetCode::Cancelled;}
  if(error==1){frame.kind=tn::ServiceFrameKind::Request;frame.request.verb=tn::StringView("POST");frame.request.path=tn::StringView("/untrusted");}
  std::string padding(13000,'x');const tn::HeaderView headers[]={{tn::StringView("X-Padding"),tn::StringView(padding.data(),padding.size())}};
  if(error==2){frame.kind=tn::ServiceFrameKind::Response;frame.response.status=200;frame.response.headers=tn::Span<const tn::HeaderView>(headers);}
  auto packets=f.server.MakeMany(frame,512);bool failed=false;
  for(auto& packet:packets)if(!f.Receive(packet)){failed=true;break;}
  assert(failed);f.MustRetain();
 }
 for(int error=0;error<5;++error){
  Fixture f;assert(f.exchange.Queue(++f.now));auto p=f.exchange.Next(++f.now);assert(p.size);
  if(error==0)assert(!f.exchange.Written(p.ticket,false,++f.now));
  if(error==1)assert(!f.exchange.Written(p.ticket,true,0));
  if(error==2)assert(!f.exchange.Written(p.ticket,true,10000001));
  if(error==3){assert(f.exchange.Abort());assert(!f.exchange.Written(p.ticket,true,++f.now));}
  if(error==4)assert(!f.exchange.Cancel(++f.now));
  f.MustRetain();assert(!f.exchange.Written(p.ticket,true,++f.now));
 }
 {
  Fixture f;f.Live();auto p=f.server.Body(7,session_event);p.back()^=1; // fixture nonce-tag checks first 12 bytes only
  p[p.size()-16]^=1;assert(!f.Receive(p));f.MustRetain();
 }
 for(int error=0;error<3;++error){
  Fixture f;f.Live();assert(f.exchange.Cancel(++f.now));while(f.protocol.Pending().kind!=Kind::None)f.Send();
  if(error==0){assert(f.Receive(f.server.Response(9,200,std::string(500,' '))));assert(!f.Receive(f.server.Body(9,std::string(13,' '),true)));}
  if(error==1)assert(!f.Receive(f.server.Response(9,200,"{\"ok\":false}",true)));
  if(error==2)assert(!f.Receive(f.server.Body(9,"{\"ok\":true}",true)));
  f.MustRetain();
 }
 {
  const auto& api=*stackchan_muse_voice_exchange_api();assert(api.version==1&&api.bytes==sizeof(ServiceExchange));
  NonceTagCrypto c;tn::ClientSession s(c);Establish(s);Protocol p;
  alignas(ServiceExchange) unsigned char storage[sizeof(ServiceExchange)+1];
  assert(!api.construct(nullptr,sizeof storage,&p,&s));assert(!api.construct(storage+1,sizeof(ServiceExchange),&p,&s));assert(!api.construct(storage,sizeof(ServiceExchange)-1,&p,&s));assert(!api.construct(storage,sizeof storage,nullptr,&s));
  auto* x=api.construct(storage,sizeof storage,&p,&s);assert(x);
  assert((x->*api.begin)({7,call,"device",7,9,10000},0,1760000000000LL));assert(!api.destroy(x));assert((x->*api.abort)());assert(!(x->*api.joined)(false));assert(!api.destroy(x));assert((x->*api.joined)(true));assert(api.destroy(x));
  // Reference members are not plain storage; owned buffers, not pointers, wipe.
  assert(api.destroy(nullptr));
 }
 std::printf("Fixed owner storage: exchange=%zu protocol=%zu client_session=%zu bytes; no stack worker allocation or native runtime measured\n",sizeof(ServiceExchange),sizeof(Protocol),sizeof(tn::ClientSession));
 std::puts("PASS actual SDK request/body/reset framing, returned session -> review, split replies, stale write tickets, transport error/rollback/expiry and checked retained join");
}
