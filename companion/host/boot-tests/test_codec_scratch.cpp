// Production Start must fail closed if its heap scratch allocation fails.
#include "companion/codec_tasks.h"
#include "sdk_boundary.hpp"
#include <new>
#include <cstdio>
#include <cstdlib>
static bool deny_scratch=false;
void* operator new(std::size_t bytes,const std::nothrow_t&) noexcept {
 if(deny_scratch&&bytes==8208)return nullptr;
 try{return ::operator new(bytes);}catch(...){return nullptr;}
}
struct Codec:AudioCodec {
 int reads=0,writes=0;
 bool InputData(std::vector<int16_t>&)override{++reads;return false;}
 void OutputData(std::vector<int16_t>&)override{++writes;}
};
static void require(bool ok,const char* reason){if(!ok){std::fprintf(stderr,"FAIL %s\n",reason);std::exit(1);}}
int main(){
 companion::CodecTasks worker;Codec codec;int stops=0;
 auto send=[](const uint8_t*,std::size_t){return true;};auto stopped=[&](uint32_t,bool){++stops;};
 deny_scratch=true;
 require(!worker.Start(&codec,send,stopped),"allocation failure rejects Start before task or codec access");
 require(worker.IsStopped()&&!codec_fixture::entry&&codec.reads==0&&codec.writes==0&&stops==0,"failed allocation leaves worker stopped and hardware untouched");
 deny_scratch=false;codec_fixture::task_create_result=0;
 require(!worker.Start(&codec,send,stopped),"task stack allocation failure rejects Start");
 require(worker.IsStopped()&&!worker.IsActive()&&!codec_fixture::entry&&codec.reads==0&&codec.writes==0&&stops==0,"failed task allocation leaves worker stopped and IO untouched");
 codec_fixture::task_create_result=1;
 require(worker.Start(&codec,send,stopped),"start after heap/task allocation failure");
 codec_fixture::delay=[&]{worker.Stop(0);};codec_fixture::entry(codec_fixture::argument);codec_fixture::delay={};
 require(worker.IsStopped()&&codec.reads==0&&codec.writes==0&&stops==1,"worker stops without activating unarmed codec");
 std::puts("checked codec scratch allocation PASS");
}
