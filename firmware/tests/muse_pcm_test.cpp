#include <companion/muse_voice_pcm.hpp>
#include <cassert>
#include <functional>
#include <algorithm>
#include <cstdio>
using namespace companion;
static int opens=0,closes=0,processes=0,bound_calls=0,error=0;
static uint32_t bound=512,actual=320;
static std::function<void()> during;
static int hook_stage=0;
static std::vector<esp_ae_rate_cvt_cfg_t> configs;
static std::vector<int16_t> input;
static int16_t* dsp_output=nullptr;static size_t dsp_capacity=0;
static void Hook(int stage){if(hook_stage&&hook_stage!=stage)return;auto f=std::move(during);during={};if(f)f();}
esp_ae_err_t esp_ae_rate_cvt_open(esp_ae_rate_cvt_cfg_t* c,void** h){++opens;configs.push_back(*c);*h=reinterpret_cast<void*>(uintptr_t(opens));Hook(1);return static_cast<esp_ae_err_t>(error);}
void esp_ae_rate_cvt_close(void*){++closes;}
esp_ae_err_t esp_ae_rate_cvt_get_max_out_sample_num(void*,uint32_t,uint32_t* n){++bound_calls;*n=bound;Hook(2);return static_cast<esp_ae_err_t>(error);}
esp_ae_err_t esp_ae_rate_cvt_process(void*,void* in,uint32_t n,void* out,uint32_t* count){++processes;input.assign(static_cast<int16_t*>(in),static_cast<int16_t*>(in)+n);dsp_output=static_cast<int16_t*>(out);dsp_capacity=*count;std::fill_n(dsp_output,dsp_capacity,73);*count=actual;Hook(3);return static_cast<esp_ae_err_t>(error);}
struct Core {
 int channels=2,reads=0,writes=0;bool okay=true;AudioLease seen;std::function<void()> during_io;
 int input_sample_rate()const{return 24000;}int output_sample_rate()const{return 24000;}int input_channels()const{return channels;}int output_channels()const{return 1;}
 bool MuseInputDataChecked(const AudioLease& l,std::vector<int16_t>& data){++reads;seen=l;for(size_t i=0;i<data.size();++i)data[i]=(channels==2&&i%2)?999:17;if(during_io)during_io();return okay;}
 bool MuseOutputDataChecked(const AudioLease& l,std::vector<int16_t>& data){++writes;seen=l;assert(data.size()<=768);if(during_io)during_io();return okay;}
};
static AudioLease Take(){AudioLease l;assert(AcquireAudio(AudioOwner::Muse,AudioPurpose::MusePushToTalk,8,l));return l;}
static void Reset(){opens=closes=processes=bound_calls=error=0;bound=512;actual=320;configs.clear();input.clear();during={};hook_stage=0;dsp_output=nullptr;dsp_capacity=0;}
int main(){
 {
  Reset();Core core;muse::PcmAdapter<Core> pcm(core);assert(!opens&&!core.reads&&!core.writes);auto l=Take();assert(pcm.Prepare(l,{true,true}));assert(opens==2&&!core.reads&&!core.writes);assert(configs[0].src_rate==24000&&configs[0].dest_rate==16000&&configs[1].src_rate==16000&&configs[1].dest_rate==24000);assert(configs[0].channel==1&&configs[1].channel==1);
  int16_t out[512];size_t n=0;assert(pcm.Read16k(l,out,512,n)&&n==320&&out[0]==73&&out[511]==0);assert(input.size()==480&&std::all_of(input.begin(),input.end(),[](auto v){return v==17;}));assert(core.seen.generation()==l.generation());
  bound=768;actual=480;int16_t data[320]={};size_t used=0;assert(pcm.Write16k(l,data,320,used)&&used==320&&core.writes==1);assert(pcm.Close(l));assert(!pcm.Prepare(l,{true,true}));assert(CommonAudioAuthority().Current(l));assert(ReleaseAudio(l,true));assert(closes==2);
 }
 for(int where=0;where<5;++where){
  Reset();Core core;muse::PcmAdapter<Core> pcm(core);auto l=Take();assert(pcm.Prepare(l,{true,true}));int16_t out[512];std::fill_n(out,512,9);size_t n=99;
  if(where==0)core.during_io=[&]{RevokeAudio(l);};
  if(where==1)during=[&]{RevokeAudio(l);};
  if(where==2){bound=513;}
  if(where==3){actual=513;}
  if(where==4){hook_stage=3;during=[&]{RevokeAudio(l);};}
  assert(!pcm.Read16k(l,out,512,n)&&n==0&&std::all_of(out,out+512,[](auto v){return v==0;}));assert(CommonAudioAuthority().Holds(l)&&!CommonAudioAuthority().Current(l));assert(pcm.Close(l));assert(ReleaseAudio(l,true));
 }
 {
  Reset();Core core;muse::PcmAdapter<Core> pcm(core);auto l=Take();during=[&]{RevokeAudio(l);};assert(!pcm.Prepare(l,{true,true}));assert(!core.reads&&!core.writes);assert(pcm.Close(l));assert(ReleaseAudio(l,true));
 }
 {
  Reset();Core core;muse::PcmAdapter<Core> pcm(core);AudioOwnerAuthority foreign;AudioLease l;assert(foreign.Acquire(AudioOwner::Muse,AudioPurpose::MusePushToTalk,8,l));assert(!pcm.Prepare(l,{true,true})&&!opens);
 }
 {
  Reset();Core core;core.channels=1;muse::PcmAdapter<Core> pcm(core);auto l=Take();assert(pcm.Prepare(l,{true,false}));int16_t out[512];size_t n=0;actual=0;assert(pcm.Read16k(l,out,512,n)&&n==0);assert(pcm.Close(l));auto stale=l;assert(ReleaseAudio(l,true));auto fresh=Take();assert(pcm.Prepare(fresh,{false,true}));assert(!pcm.Close(stale));bound=768;actual=0;int16_t data[320]={};assert(pcm.Write16k(fresh,data,320,n)&&n==320&&!core.writes);assert(dsp_capacity==768&&std::all_of(dsp_output,dsp_output+dsp_capacity,[](auto v){return v==0;}));assert(pcm.Close(fresh));assert(ReleaseAudio(fresh,true));
 }

 for(int where=0;where<6;++where){
  Reset();Core core;muse::PcmAdapter<Core> pcm(core);auto l=Take();assert(pcm.Prepare(l,{false,true}));bound=768;actual=480;int16_t data[320]={};size_t n=99;
  if(where==0){hook_stage=2;during=[&]{RevokeAudio(l);};}
  if(where==1){hook_stage=3;during=[&]{RevokeAudio(l);};}
  if(where==2)core.during_io=[&]{RevokeAudio(l);};
  if(where==3)core.okay=false;
  if(where==4)bound=769;
  if(where==5)actual=769;
  assert(!pcm.Write16k(l,data,320,n)&&n==0);assert(CommonAudioAuthority().Holds(l)&&!CommonAudioAuthority().Current(l));const int writes=core.writes;assert(!pcm.Write16k(l,data,320,n)&&core.writes==writes);assert(pcm.Close(l));assert(ReleaseAudio(l,true));
 }
 for(int where=0;where<3;++where){
  Reset();Core core;muse::PcmAdapter<Core> pcm(core);auto l=Take();if(where==0)error=-2;else {assert(pcm.Prepare(l,{true,true}));error=-1;}
  if(where==0)assert(!pcm.Prepare(l,{true,true}));
  else if(where==1){int16_t out[512];size_t n;assert(!pcm.Read16k(l,out,512,n)&&!n);}
  else {int16_t in[320]={};size_t n;assert(!pcm.Write16k(l,in,320,n)&&!n);}
  assert(!CommonAudioAuthority().Current(l));assert(pcm.Close(l));assert(ReleaseAudio(l,true));
 }
 std::puts("PASS actual bounded Muse PCM adapter; synthetic DSP/codec only, no numeric/audio-quality claim");
}
