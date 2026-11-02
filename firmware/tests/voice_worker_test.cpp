#include "voice_exchange_fixture.hpp"
#include <companion/muse_voice_session_worker.hpp>
#include <deque>
#include <functional>
using companion::muse::VoiceSessionWorker;
struct Clock:muse_native_review::Clock {
 int64_t mono=0,wall=1760000000000LL;int samples=0;bool okay=true;std::function<void()> hook;std::function<void(int)> sample_hook;
 bool Sample(muse_native_review::Stamp& s)override{++samples;if(sample_hook)sample_hook(samples);if(hook){auto f=std::move(hook);hook={};f();}s={mono,wall};return okay;}
};
struct Custody:muse_native_review::Custody {
 muse_native_review::Budget budget{7,10000000,1760000010000LL};bool current=true;
 bool ReadBudget(muse_native_review::Budget& b)override{b=budget;return true;}
 bool Current(const muse_native_review::Budget& b)override{return current&&b.generation==budget.generation&&b.expires_us==budget.expires_us&&b.expires_wall_ms==budget.expires_wall_ms;}
};
struct Socket {
 bool verified=true,send_ok=true,close_ok=true;int sends=0,receives=0,closes=0;int receive_error=0;uint8_t* receive_buffer=nullptr;size_t receive_capacity=0;
 std::deque<std::vector<uint8_t>> incoming;std::vector<std::vector<uint8_t>> sent;
 std::function<void()> send_hook,receive_hook,close_hook;
 bool VerifiedMetaEndpoint()const{return verified;}
 bool Send(const uint8_t* p,size_t n,unsigned timeout){assert(timeout&&timeout<=100);++sends;sent.emplace_back(p,p+n);if(send_hook)send_hook();return send_ok;}
 int Receive(uint8_t* p,size_t capacity,unsigned timeout){assert(timeout&&timeout<=20);++receives;receive_buffer=p;receive_capacity=capacity;if(receive_hook)receive_hook();if(receive_error)return receive_error;if(incoming.empty())return 0;auto packet=std::move(incoming.front());incoming.pop_front();assert(packet.size()<=capacity);std::memcpy(p,packet.data(),packet.size());return int(packet.size());}
 bool CloseAndJoin(unsigned timeout){assert(timeout<=20);++closes;if(close_hook)close_hook();return close_ok;}
};
using Worker=VoiceSessionWorker<Socket>;
struct Owner {
 NonceTagCrypto crypto;Protocol protocol;Clock clock;Custody custody;Server server;
 std::unique_ptr<tn::ClientSession> session{new tn::ClientSession(crypto)};
 std::unique_ptr<Socket> socket{new Socket};Socket* port=socket.get();tn::ClientSession* original=session.get();std::unique_ptr<Worker> worker;
 Owner(){Establish(*session);worker=Worker::Adopt(protocol,session,socket,clock,custody);assert(worker&&!session&&!socket);}
 ~Owner(){port->close_ok=true;assert(worker->Close());}
 bool Begin(){return worker->Begin({7,call,"synthetic-device",7,9,10000000});}
 void Live(){assert(Begin());port->incoming.push_back(server.Response(7,200));assert(worker->Tick());port->incoming.push_back(server.Body(7,session_event));assert(worker->Tick());assert(protocol.Status()==State::Live);assert(worker->Review().Open());}
};
int main(){
 {
  Owner o;assert(!o.port->sends&&!o.port->receives&&!o.port->closes);o.Live();auto& review=o.worker->Review().Review();while(!review.Reviewed())assert(review.Acknowledge(review.Revision(),review.Page()));assert(review.OptIn(review.Revision(),true,true));assert(review.Finish(review.Revision()));auto receipt=review.Receipt();assert(receipt.valid&&!std::strcmp(receipt.target.session,"session-exact")&&!std::strcmp(receipt.target.thread,"thread-exact"));assert(!review.NativePermit());assert(o.port->sends==1);assert(o.worker->Close());assert(o.worker->CleanupComplete());
 }
 for(int error=0;error<6;++error){
  Owner o;assert(o.Begin());
  if(error==0)o.port->send_ok=false;
  if(error==1)o.port->send_hook=[&]{o.custody.current=false;};
  if(error==2)o.port->receive_error=-1;
  if(error==3)o.port->receive_hook=[&]{o.custody.current=false;};
  if(error==4)o.port->send_hook=[&]{assert(!o.worker->Close());};
  if(error==5)o.port->send_hook=[&]{assert(!o.worker->Cancel());};
  assert(!o.worker->Tick());const int sends=o.port->sends,reads=o.port->receives;assert(!o.worker->Tick());assert(o.port->sends==sends&&o.port->receives==reads);o.port->close_ok=false;assert(!o.worker->Close()&&!o.worker->CleanupComplete());assert(o.original->isEstablished());o.port->close_ok=true;assert(o.worker->Close()&&o.worker->CleanupComplete());
 }
 for(int error=0;error<5;++error){
  Owner o;
  if(error==0)o.custody.current=false;
  if(error==1)o.custody.budget.generation=8;
  if(error==2)o.custody.budget.expires_us=9999999;
  if(error==3)o.port->verified=false;
  if(error==4)o.clock.mono=-1;
  assert(!o.Begin());assert(!o.port->sends&&!o.port->receives);assert(o.worker->Close());
 }
 {
  Owner o;o.Live();o.clock.mono=1000;assert(o.worker->Tick());const int count=o.port->sends;o.clock.mono=999;assert(!o.worker->Tick()&&o.port->sends==count);assert(o.worker->Close());
 }
 {
  Owner o;o.Live();assert(o.worker->Cancel());o.port->incoming.push_back(o.server.Body(7,"{\"type\":\"response.done\"}\n",true));assert(o.worker->Tick());assert(o.protocol.Status()==State::Ending);o.port->incoming.push_back(o.server.Response(9,200,"{\"ok\":true}",true));assert(!o.worker->Tick());assert(o.protocol.EndConfirmed());assert(!o.worker->CleanupComplete());assert(o.worker->Close());
 }
 {
  Owner o;assert(o.Begin());assert(o.worker->Tick());o.clock.mono=5000001;assert(o.worker->Tick());assert(o.protocol.Status()==State::Ending&&o.port->sends==2);assert(o.worker->Close());
 }
 {
  Owner o;assert(o.Begin());const int next=o.clock.samples+4;o.clock.sample_hook=[&](int n){if(n==next)o.custody.current=false;};assert(!o.worker->Tick());assert(!o.port->sends&&!o.port->receives);assert(o.worker->Close());
 }

 {
  NonceTagCrypto crypto;Protocol protocol;Clock clock;Custody custody;std::unique_ptr<tn::ClientSession> session(new tn::ClientSession(crypto));std::unique_ptr<Socket> socket(new Socket);auto* original=session.get();auto* port=socket.get();assert(!Worker::Adopt(protocol,session,socket,clock,custody));assert(session.get()==original&&socket.get()==port&&!port->closes);
 }
 for(int error=0;error<5;++error){
  Owner o;o.Live();const int sends=o.port->sends,reads=o.port->receives;
  if(error==0)o.custody.budget.expires_us++;
  if(error==1)o.custody.budget.expires_wall_ms++;
  if(error==2)o.clock.mono=10000000;
  if(error==3)o.port->verified=false;
  if(error==4){o.clock.wall+=1;assert(o.worker->Tick());o.clock.wall-=1;}
  const int current_reads=o.port->receives;assert(!o.worker->Tick());assert(o.port->sends==sends&&o.port->receives==current_reads&&current_reads>=reads);assert(o.worker->Close());
 }
 {
  Owner o;assert(o.Begin());o.port->incoming.push_back({1,2,3});assert(!o.worker->Tick());assert(o.port->receive_buffer&&std::all_of(o.port->receive_buffer,o.port->receive_buffer+o.port->receive_capacity,[](auto b){return b==0;}));assert(o.worker->Close());
 }
 {
  Owner o;assert(o.Begin());tn::ServiceFrameView response;response.kind=tn::ServiceFrameKind::Response;response.stream_id=7;response.response.status=200;std::string padding(9000,'x');const tn::HeaderView headers[]={{tn::StringView("X-Padding"),tn::StringView(padding.data(),padding.size())}};response.response.headers=tn::Span<const tn::HeaderView>(headers);response.response.body=tn::ConstByteSpan(reinterpret_cast<const uint8_t*>(session_event),std::strlen(session_event));auto packets=o.server.MakeMany(response,512);assert(packets.size()>1);
  for(size_t i=0;i<packets.size();++i){o.port->incoming.push_back(packets[i]);assert(o.worker->Tick());assert(o.protocol.Status()==(i+1==packets.size()?State::Live:State::Opening));}assert(o.worker->Review().Open());assert(o.worker->Close());
 }

 {
  Owner o;o.Live();int16_t samples[320]={};
  // Protocol-only injection. No native microphone grant, codec read or physical
  // flush is supplied by this synthetic turn/framing test.
  for(int turn=0;turn<2;++turn){assert(o.protocol.Hold(7,true,++o.clock.mono));assert(o.protocol.AckPlaybackFlush(7,o.protocol.FlushTicket()));o.clock.mono+=200000;for(int i=0;i<3&&o.protocol.Pending().kind==Kind::None;++i){auto n=o.protocol.Microphone(7,samples,320,++o.clock.mono);assert(n>0&&n<=320);}assert(o.protocol.Pending().kind==Kind::Append);const int sends=o.port->sends;assert(o.worker->Tick()&&o.port->sends==sends+1);assert(o.protocol.Hold(7,false,++o.clock.mono));}assert(o.worker->Close());
 }
 {
  Owner o;assert(o.Begin());const int next=o.clock.samples+8;o.clock.sample_hook=[&](int n){if(n==next)++o.custody.budget.generation;};assert(!o.worker->Tick());assert(o.port->sends==1&&!o.port->receives);assert(o.worker->Close());
 }
 std::puts("PASS retained actual SDK Muse worker / original socket / returned review / callout revoke / no retry / joined cleanup; synthetic ports only");
}
