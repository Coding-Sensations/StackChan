// Executes the production worker. SDK task/codec boundaries are offline doubles;
// these tests cannot establish driver behavior, sound, or a physical reset cause.
#include "companion/codec_tasks.h"
#include "sdk_boundary.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
using companion::CodecTasks;
static void require(bool ok,const char* reason){if(!ok){std::fprintf(stderr,"FAIL: %s\n",reason);std::exit(1);}}
struct Codec:AudioCodec{
 int reads=0,input_opens=0,opens=0,closes=0,slices=0;
 void EnableInput(bool b)override{if(b&&!input_)++input_opens;AudioCodec::EnableInput(b);}
 void EnableOutput(bool b)override{if(b!=output_){if(b)++opens;else ++closes;}AudioCodec::EnableOutput(b);}
 bool InputData(std::vector<int16_t>&)override{++reads;return false;}
 void OutputData(std::vector<int16_t>& data)override{require(output_&&data.size()==480,"bounded enabled speaker slice");++slices;codec_fixture::clock_ms+=20;}
};
static void start(CodecTasks& tasks,Codec& codec){
 codec_fixture::clock_ms=100;codec_fixture::delay={};
 require(tasks.Start(&codec,[](const uint8_t*,std::size_t){return true;},[](uint32_t,bool){}),"start");
 require(tasks.Begin(1)&&tasks.Acknowledge(1)&&tasks.Consent(1,true),"armed epoch");
}
static void packet(CodecTasks& tasks,uint32_t sequence){
 std::array<uint8_t,17> wire{};const uint8_t payload=0;
 require(companion::encode_wire({1,sequence,true,&payload,1},wire.data(),wire.size())==17,"frame");
 require(tasks.Receive(wire.data(),wire.size()),"speaker packet admitted");
}
static void unknown_direction(){
 CodecTasks tasks;Codec codec;start(tasks,codec);
 codec_fixture::delay=[&]{tasks.Stop(0);};
 codec_fixture::entry(codec_fixture::argument);codec_fixture::delay={};
 require(codec.reads==0&&codec.input_opens==0,"armed before playing must not open/read microphone");
 require(!codec.input_enabled()&&codec.opens==0,"unknown direction stays physically disabled");
}
static void continuous_speaker(){
 CodecTasks tasks;Codec codec;start(tasks,codec);require(tasks.Playing(1,true),"speaking");packet(tasks,0);packet(tasks,1);
 int steps=0;
 codec_fixture::delay=[&]{
  if(steps++==0){require(codec.slices==6&&codec.opens==1&&codec.closes==0,"one speaker open across two packets and idle gap");require(tasks.Playing(1,false),"end speaking");}
  else tasks.Stop(0);
 };
 codec_fixture::entry(codec_fixture::argument);codec_fixture::delay={};
 require(codec.opens==1&&codec.closes==1,"one close on speaking exit");
 require(codec.reads==1,"explicit playing false permits microphone capture");
 require(!codec.output_enabled(),"output stopped");
}
static void interrupted_speaker(){
 CodecTasks tasks;Codec codec;start(tasks,codec);require(tasks.Playing(1,true),"speaking");packet(tasks,0);
 int steps=0;
 codec_fixture::delay=[&]{if(steps++==0){require(codec.opens==1&&codec.closes==0,"speaker open before interrupt");tasks.Interrupt(1);}else tasks.Stop(0);};
 codec_fixture::entry(codec_fixture::argument);codec_fixture::delay={};
 require(codec.opens==1&&codec.closes==1&&codec.reads==0,"interrupt closes output without starting input");
}
static void direction_wait_expiry(){
 CodecTasks tasks;Codec codec;start(tasks,codec);int steps=0;
 codec_fixture::delay=[&]{if(steps++==0)codec_fixture::clock_ms=2100;else {require(!tasks.IsActive(),"two second watchdog expires while awaiting direction");tasks.Stop(0);}};
 codec_fixture::entry(codec_fixture::argument);codec_fixture::delay={};
 require(codec.reads==0&&codec.input_opens==0&&codec.opens==0,"expiry remains hardware muted");
}
int main(int argc,char** argv){require(argc==2,"test mode");const std::string mode=argv[1];if(mode=="unknown")unknown_direction();else if(mode=="speaking")continuous_speaker();else if(mode=="interrupt")interrupted_speaker();else if(mode=="expiry")direction_wait_expiry();else require(false,"known mode");std::puts("codec lifecycle passed");}
