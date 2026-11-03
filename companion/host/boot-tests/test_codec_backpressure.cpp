// Real production queue/worker with threaded SDK and blocking audio doubles.
// This proves queue policy under modeled delay, not target decoder/I2S timing.
#include "companion/codec_tasks.h"
#include "sdk_boundary.hpp"
#include <string>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <thread>
using namespace std::chrono;
static void check(bool b,const char* s){if(!b){std::fprintf(stderr,"FAIL %s\n",s);std::exit(1);}}
struct Codec:AudioCodec{
 std::atomic<unsigned> slices{0};
 bool InputData(std::vector<int16_t>&)override{return false;}
 void OutputData(std::vector<int16_t>& p)override{check(p.size()==480,"20ms output slice");std::this_thread::sleep_for(milliseconds(20));++slices;}
};
static std::array<uint8_t,17> wire(unsigned seq){std::array<uint8_t,17>b{};uint8_t p=seq;check(companion::encode_wire({1,seq,true,&p,1},b.data(),b.size())==17,"wire");return b;}
int main(int argc,char**argv){
 check(argc==2,"mode");std::string mode=argv[1];Codec codec;companion::CodecTasks t;std::atomic<unsigned> stops{0};
 check(t.Start(&codec,[](const uint8_t*,size_t){return true;},[&](uint32_t,bool){++stops;}),"start");
 check(t.Begin(1)&&t.Acknowledge(1)&&t.Consent(1,true)&&t.Playing(1,true),"physical consent/direction");
 auto first=wire(0),second=wire(1),third=wire(2);check(t.Receive(first.data(),first.size()),"first admitted");
 std::this_thread::sleep_for(milliseconds(60));check(t.Receive(second.data(),second.size()),"second admitted");
 std::this_thread::sleep_for(milliseconds(60));auto before=steady_clock::now();
 std::thread release;
 if(mode=="delayed_consumer"||mode=="late_resume"){
  if(mode=="late_resume")codec_fixture::receive_delay_ms=150;
  codec_fixture::decode_delay_ms=30;
  release=std::thread([]{std::this_thread::sleep_for(milliseconds(30));codec_fixture::release_worker=true;});
 }else if(mode=="interrupt"||mode=="stop"||mode=="epoch_change"){
  release=std::thread([&]{std::this_thread::sleep_for(milliseconds(30));
   if(mode=="stop")t.Stop(0);
   else if(mode=="epoch_change")check(t.Begin(2)&&t.Acknowledge(2)&&t.Consent(2,true)&&t.Playing(2,true),"replacement session");
   else t.Interrupt(1);
  });
 }else check(mode=="timeout","known mode");
 bool accepted=t.Receive(third.data(),third.size());auto waited=duration_cast<milliseconds>(steady_clock::now()-before).count();
 if(release.joinable())release.join();
 if(mode=="delayed_consumer"){
  check(accepted,"full queue waits for delayed consumer without muting/dropping");check(waited>=20&&waited<120,"bounded first wait");
  for(unsigned seq=3;seq<12;++seq){std::this_thread::sleep_for(milliseconds(60));auto frame=wire(seq);check(t.Receive(frame.data(),frame.size()),"slow consumer receives every ordered packet");}
  auto end=steady_clock::now()+seconds(3);while(codec.slices<36&&steady_clock::now()<end)std::this_thread::sleep_for(milliseconds(5));
  check(codec.slices==36&&t.IsActive(),"all twelve frames played through modeled driver");
 }else{
  check(!accepted,"pending frame rejected on timeout/cancellation");
  check(t.IsActive()==(mode=="epoch_change"),"cancel preserves replacement; otherwise disarmed");
  if(mode=="timeout"){check(waited>=119&&waited<250,"120ms maximum queue wait");check(codec_fixture::fault_code==companion::diagnostic::FaultCode::QueueFull&&codec_fixture::fault_detail==2,"same queue timeout fault/depth");}
  else if(mode=="late_resume"){check(waited>=150&&waited<300,"late socket resume");check(codec_fixture::fault_code==companion::diagnostic::FaultCode::QueueFull,"expired wait cannot admit even a newly free slot");}
  else{check(waited>=20&&waited<120,"interrupt cancels wait promptly");check(!codec_fixture::fault_recorded,"interrupt is not queue failure");}
 }
 codec_fixture::release_worker=true;check(t.Stop(),"bounded stop");codec_fixture::worker.join();
 if(mode=="delayed_consumer"){check(codec_fixture::decoded_sequence.size()==12,"no packet dropped");for(unsigned i=0;i<12;++i)check(codec_fixture::decoded_sequence[i]==i,"no reordering");check(!codec_fixture::fault_recorded,"no fault under bounded delay");}
 check(!codec.input_enabled()&&!codec.output_enabled(),"IO disabled after stop");
 std::printf("%s wait_ms=%lld decoded=%u slices=%u stopped=true\n",mode.c_str(),static_cast<long long>(waited),codec_fixture::decode_calls,codec.slices.load());
}
