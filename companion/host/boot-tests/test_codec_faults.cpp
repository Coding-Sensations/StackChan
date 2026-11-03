// Actual production policy/worker; SDK codec+driver boundaries are explicit doubles.
// Never represents Xtensa decode, driver success, sound, or the live reset cause.
#include "companion/codec_tasks.h"
#include "sdk_boundary.hpp"
#include <array>
#include <fstream>
#include <string>
#include <cstdio>
#include <cstdlib>
using companion::CodecTasks;
static void check(bool b,const char* s){if(!b){std::fprintf(stderr,"FAIL %s\n",s);std::exit(1);}}
struct Codec:AudioCodec {
 bool input_ok=false;int opens=0,closes=0,reads=0,slices=0;std::function<void()> output;
 void EnableOutput(bool b)override{if(b!=output_){if(b)++opens;else ++closes;}AudioCodec::EnableOutput(b);}
 bool InputData(std::vector<int16_t>&)override{++reads;return input_ok;}
 void OutputData(std::vector<int16_t>& d)override{check(output_&&d.size()==480,"bounded speaker slice");++slices;codec_fixture::clock_ms+=20;if(output)output();}
};
struct Receipt {bool require_fault=false;int stops=0;uint32_t epoch=0;bool physical_silence=true;};
static void start(CodecTasks& t,Codec& c,Receipt& r){
 codec_fixture::clock_ms=100;codec_fixture::delay={};codec_fixture::decode_calls=0;codec_fixture::decode_result=0;codec_fixture::decoded_bytes=2880;codec_fixture::consume_all=true;codec_fixture::fault_recorded=false;codec_fixture::fault_calls=0;codec_fixture::samples=0;codec_fixture::encoder_open_result=0;codec_fixture::decoder_open_result=0;codec_fixture::resampler_open_result=0;codec_fixture::encoder_process_result=0;codec_fixture::resample_samples=0;
 check(t.Start(&c,[](const uint8_t*,std::size_t){return true;},[&](uint32_t e,bool silence){if(r.require_fault)check(codec_fixture::fault_recorded,"first fault recorded before stopped/cleanup callback");++r.stops;r.epoch=e;r.physical_silence=silence;}),"start");
 check(t.Begin(1)&&t.Acknowledge(1)&&t.Consent(1,true)&&t.Playing(1,true),"handshake and explicit speaker direction");
}
static std::array<uint8_t,17> wire(uint32_t seq){std::array<uint8_t,17> b{};const uint8_t p=0;check(companion::encode_wire({1,seq,true,&p,1},b.data(),b.size())==17,"frame");return b;}
static void run(){codec_fixture::entry(codec_fixture::argument);codec_fixture::delay={};}
static void fault(const std::string& mode){
 CodecTasks t;Codec c;Receipt r;start(t,c,r);r.require_fault=true;
 if(mode=="decode_error")codec_fixture::decode_result=-1;
 if(mode=="short_pcm")codec_fixture::decoded_bytes=2878;
 if(mode=="partial_input")codec_fixture::consume_all=false;
 if(mode=="encode_error"){
  c.input_ok=true;codec_fixture::resample_samples=320;codec_fixture::encoder_process_result=-20;check(t.Playing(1,false),"explicit capture direction");
 }else if(mode=="sequence_gap"){
  auto first=wire(1);check(!t.Receive(first.data(),first.size()),"first packet sequence gap rejected");codec_fixture::delay=[&]{t.Stop(0);};
 }else{
  auto first=wire(0);check(t.Receive(first.data(),first.size()),"first packet admitted");
 }
 if(mode=="encoder_open")codec_fixture::encoder_open_result=-11;
 if(mode=="decoder_open")codec_fixture::decoder_open_result=-12;
 if(mode=="resampler_open")codec_fixture::resampler_open_result=-13;
 if(mode=="overflow"){
  auto second=wire(1),third=wire(2);check(t.Receive(second.data(),second.size()),"queue holds two");check(!t.Receive(third.data(),third.size()),"third queued packet rejects");check(!t.IsActive(),"overflow immediately disarms");
  codec_fixture::delay=[&]{t.Stop(0);};
 }
 run();
 check(!t.IsActive()&&t.IsStopped()&&c.opens==0&&c.reads==(mode=="encode_error"?3:0),"first failure remains input/output disabled");
 check(r.stops==1&&r.epoch==1&&!r.physical_silence,"same stopped signal; no cause field and no physical silence attestation");
 const bool opens_failed=mode=="encoder_open"||mode=="decoder_open"||mode=="resampler_open";
 check(codec_fixture::decode_calls==((mode=="overflow"||mode=="sequence_gap"||mode=="encode_error"||opens_failed)?0U:1U),"failure boundary visited");
 using companion::diagnostic::FaultCode;using companion::diagnostic::Task;
 FaultCode expected=FaultCode::None;int32_t detail=0;
 if(mode=="decode_error"){expected=FaultCode::DecodeError;detail=-1;}
 if(mode=="short_pcm"){expected=FaultCode::DecodedSize;detail=2878;}
 if(mode=="partial_input"){expected=FaultCode::PartialConsumption;detail=0;}
 if(mode=="overflow"){expected=FaultCode::QueueFull;detail=2;}
 if(mode=="encoder_open"){expected=FaultCode::EncoderOpen;detail=-11;}
 if(mode=="decoder_open"){expected=FaultCode::DecoderOpen;detail=-12;}
 if(mode=="resampler_open"){expected=FaultCode::ResamplerOpen;detail=-13;}
 if(mode=="encode_error"){expected=FaultCode::EncodeError;detail=-20;}
 if(mode=="sequence_gap"){expected=FaultCode::PacketRejected;detail=0;}
 check(codec_fixture::fault_code==expected&&codec_fixture::fault_detail==detail,"specific first fault code and numeric detail");
 check(codec_fixture::fault_task==((mode=="overflow"||mode=="sequence_gap")?Task::AudioTls:Task::Codec),"fault attributed to actual calling task");
 auto before=codec_fixture::fault_calls;companion::diagnostic::Fault(Task::Codec,FaultCode::DriverOutputClose,-99);
 check(codec_fixture::fault_calls==before+1&&codec_fixture::fault_code==expected&&codec_fixture::fault_detail==detail,"cleanup fault cannot replace recorded first fault");
 std::printf("%s stops=%d epoch=%u physical_silence=%d decode_calls=%u\n",mode.c_str(),r.stops,r.epoch,r.physical_silence,codec_fixture::decode_calls);
}
static std::vector<std::vector<uint8_t>> frames(const char* path){
 std::ifstream f(path,std::ios::binary);check(bool(f),"cue frame file");std::vector<std::vector<uint8_t>> result;
 for(;;){uint8_t h[4];f.read(reinterpret_cast<char*>(h),4);if(f.gcount()==0)break;check(f.gcount()==4,"length");auto n=companion::be32(h);check(n>=17&&n<=4112,"bounded wire length");std::vector<uint8_t>b(n);f.read(reinterpret_cast<char*>(b.data()),n);check(f.gcount()==n,"complete frame");result.push_back(std::move(b));}
 check(result.size()==83,"pinned cue has 83 padded 60ms packets");return result;
}
static void cue(const char* path){
 auto input=frames(path);CodecTasks t;Codec c;Receipt r;start(t,c,r);size_t next=1;uint64_t last_heartbeat=100;
 check(t.Receive(input[0].data(),input[0].size()),"first real host frame");
 c.output=[&]{
  if(codec_fixture::clock_ms-last_heartbeat>=500){check(t.Heartbeat(1),"bounded heartbeat accepted during cue");last_heartbeat=codec_fixture::clock_ms;}
  if(c.slices%3==0&&next<input.size()){auto& b=input[next++];check(t.Receive(b.data(),b.size()),"next real host frame");}
 };
 codec_fixture::delay=[&]{check(next==83&&c.slices==249,"complete cue worker path");t.Stop(0);};
 run();check(c.opens==1&&c.closes==1&&c.reads==0,"one output interval, no capture during cue");
 check(codec_fixture::decode_calls==83&&r.stops==1&&!r.physical_silence,"worker consumes all modeled decode results and does not attest sound");
 std::printf("cue packets=%u slices=%d opens=%d closes=%d simulated_ms=%llu vendor_decoder=false\n",codec_fixture::decode_calls,c.slices,c.opens,c.closes,static_cast<unsigned long long>(codec_fixture::clock_ms-100));
}
int main(int argc,char**argv){check(argc>=2,"mode");std::string mode=argv[1];if(mode=="cue"){check(argc==3,"cue file");cue(argv[2]);}else{check(mode=="decode_error"||mode=="short_pcm"||mode=="partial_input"||mode=="overflow"||mode=="encoder_open"||mode=="decoder_open"||mode=="resampler_open"||mode=="encode_error"||mode=="sequence_gap","known mode");fault(mode);}}
