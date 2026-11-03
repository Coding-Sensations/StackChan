#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

// Single serialized audio-worker owner. Loading/serialization happen off the
// render path. No hardware APIs, dynamic render allocation or voice stealing.
namespace stackchan_instruments {
enum class Waveform : uint8_t { Triangle, Square };
struct Preset { Waveform waveform=Waveform::Triangle; uint16_t gain=2048; };
struct Step { uint8_t pad_mask=0; bool accent=false; };
struct Project {
 uint16_t bpm=120; Preset preset; std::array<Step,16> steps{};
 std::array<uint8_t,4> pad_ids{0,1,2,3};
};
inline bool MakePattern(unsigned id,Project& out){
 if(id>2){return false;}
 Project p;
 for(unsigned step=0;step<16;++step){
  unsigned mask=step%2==0?4:0;
  if(id==0){if(step%4==0)mask|=1;if(step==4||step==12)mask|=2;}
  else if(id==1){if(step==0||step==6||step==10)mask|=1;if(step==4||step==12)mask|=2;if(step==15)mask|=8;}
  else {if(step==0||step==10)mask|=1;if(step==8)mask|=2;if(step%4==3)mask|=8;}
  p.steps[step]={uint8_t(mask),step%4==0};
 }
 out=p;return true;
}
inline Project DefaultProject(){Project p;MakePattern(0,p);return p;}
inline bool ValidProject(const Project& p) {
 if(p.bpm<20||p.bpm>300||unsigned(p.preset.waveform)>1||p.preset.gain>8192)return false;
 for(auto s:p.steps)if(s.pad_mask>15)return false;
 for(auto id:p.pad_ids)if(id>7)return false;
 return true;
}
inline uint32_t Crc32(const uint8_t* data,size_t size) {
 uint32_t crc=~0u;for(size_t i=0;i<size;++i){crc^=data[i];for(unsigned j=0;j<8;++j)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));}return ~crc;
}
inline std::array<uint8_t,32> EncodeProject(const Project& p) {
 std::array<uint8_t,32> b{};if(!ValidProject(p))return b;
 b[0]='S';b[1]='C';b[2]='I';b[3]='P';b[4]=1;b[5]=uint8_t(p.bpm);b[6]=uint8_t(p.bpm>>8);b[7]=uint8_t(p.preset.waveform);b[8]=uint8_t(p.preset.gain);b[9]=uint8_t(p.preset.gain>>8);
 for(size_t i=0;i<16;++i)b[10+i]=p.steps[i].pad_mask|(p.steps[i].accent?16:0);
 b[26]=p.pad_ids[0]|(p.pad_ids[1]<<4);b[27]=p.pad_ids[2]|(p.pad_ids[3]<<4);
 auto crc=Crc32(b.data(),28);for(unsigned i=0;i<4;++i)b[28+i]=uint8_t(crc>>(8*i));return b;
}
inline bool DecodeProject(const uint8_t* b,size_t size,Project& out) {
 if(!b||size!=32||std::memcmp(b,"SCIP",4)||b[4]!=1)return false;
 uint32_t crc=0;for(unsigned i=0;i<4;++i)crc|=uint32_t(b[28+i])<<(8*i);if(crc!=Crc32(b,28))return false;
 Project p;p.bpm=uint16_t(b[5]|(unsigned(b[6])<<8));p.preset={Waveform(b[7]),uint16_t(b[8]|(unsigned(b[9])<<8))};
 for(size_t i=0;i<16;++i){if(b[10+i]&0xe0)return false;p.steps[i]={uint8_t(b[10+i]&15),bool(b[10+i]&16)};}
 p.pad_ids={uint8_t(b[26]&15),uint8_t(b[26]>>4),uint8_t(b[27]&15),uint8_t(b[27]>>4)};
 if(!ValidProject(p)){return false;}
 out=p;return true;
}
struct WavView { const uint8_t* pcm=nullptr;size_t frames=0; };
inline uint32_t Le32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline uint16_t Le16(const uint8_t* p){return uint16_t(p[0]|(unsigned(p[1])<<8));}
inline bool ParseWav(const uint8_t* b,size_t size,WavView& out,unsigned rate=24000) {
 if(!b||size<44||size>1048576+4096||std::memcmp(b,"RIFF",4)||std::memcmp(b+8,"WAVE",4)||uint64_t(Le32(b+4))+8!=size||rate!=24000)return false;
 bool fmt=false,data=false;WavView candidate;size_t pos=12;
 while(pos<size){if(size-pos<8)return false;auto n=Le32(b+pos+4);const auto* id=b+pos;pos+=8;if(n>size-pos)return false;
  if(!std::memcmp(id,"fmt ",4)){if(fmt||n!=16||Le16(b+pos)!=1||Le16(b+pos+2)!=1||Le32(b+pos+4)!=rate||Le32(b+pos+8)!=rate*2||Le16(b+pos+12)!=2||Le16(b+pos+14)!=16)return false;fmt=true;}
  else if(!std::memcmp(id,"data",4)){if(data||!fmt||!n||(n&1))return false;data=true;candidate={b+pos,n/2};}
  pos+=n;if(n&1){if(pos==size)return false;++pos;}
 }
 if(!fmt||!data){return false;}
 out=candidate;return true;
}
class Sequencer {
 uint32_t accumulator_=0;uint8_t step_=0;bool running_=false,first_=true;uint16_t bpm_=120;
public:
 bool Start(unsigned bpm){if(bpm<20||bpm>300)return false;bpm_=uint16_t(bpm);accumulator_=0;step_=0;first_=true;running_=true;return true;}
 void Stop(){running_=false;accumulator_=0;step_=0;first_=true;}
 bool Running()const{return running_;}
 int CurrentStep()const{return !running_||first_?-1:int((step_+15)&15);}
 bool Tick(uint8_t& step){if(!running_)return false;bool hit=first_||accumulator_>=1440000u;if(hit){if(!first_)accumulator_-=1440000u;step=step_;step_=uint8_t((step_+1)&15);first_=false;}accumulator_+=unsigned(bpm_)*4;return hit;}
};
class Engine {
 struct SynthVoice {bool active=false,down=false;uint8_t channel=0,note=0,velocity=0;uint32_t phase=0,increment=0;uint16_t envelope=0;uint8_t release=0;};
 struct PadVoice {bool active=false,accent=false;uint8_t pad=0,velocity=0;size_t cursor=0;};
 std::array<SynthVoice,4> synth_{};std::array<PadVoice,4> pads_{};std::array<bool,16> sustain_{};
 std::array<std::vector<int16_t>,4> bank_{};size_t budget_=1048576,bank_bytes_=0;unsigned rate_;Project project_=DefaultProject();Sequencer sequence_;uint64_t clipped_=0,rejected_=0;
 static uint32_t Increment(unsigned note);
 int32_t SynthSample(SynthVoice& v){
  if(!v.active){return 0;}
  unsigned env=v.envelope;
  if(v.release){env=unsigned(v.envelope)*v.release/64;if(!--v.release)v.active=false;}
  else {v.envelope=uint16_t(std::min(32768u,unsigned(v.envelope)+1024));env=v.envelope;}
  const uint32_t p=v.phase>>16;int32_t wave=project_.preset.waveform==Waveform::Square?(p<32768?32767:-32768):(p<32768?int32_t(p)*2-32768:98303-int32_t(p)*2);
  v.phase+=v.increment;return int32_t(int64_t(wave)*project_.preset.gain*v.velocity*env/(32768LL*127*32768));
 }
 int32_t PadSample(PadVoice& v){if(!v.active)return 0;const auto& b=bank_[v.pad];if(v.cursor>=b.size()){v.active=false;return 0;}auto x=b[v.cursor++];if(v.cursor==b.size())v.active=false;return int32_t(x)*v.velocity*(v.accent?3:2)/254;}
public:
 static constexpr size_t MaxBankBytes=1048576,MaxRenderFrames=4096;
 explicit Engine(unsigned rate=24000,size_t budget=MaxBankBytes):budget_(std::min(budget,MaxBankBytes)),rate_(rate){}
 bool NoteOn(unsigned channel,unsigned note,unsigned velocity){
  if(rate_!=24000||channel>15||note>126||velocity>127){return false;}
  if(!velocity){NoteOff(channel,note);return true;}
  SynthVoice* chosen=nullptr;for(auto& v:synth_)if(v.active&&v.channel==channel&&v.note==note){chosen=&v;break;}
  if(!chosen)for(auto& v:synth_)if(!v.active){chosen=&v;break;}
  if(!chosen){++rejected_;return false;}*chosen={true,true,uint8_t(channel),uint8_t(note),uint8_t(velocity),0,Increment(note),0,0};return true;
 }
 void NoteOff(unsigned channel,unsigned note){if(channel>15||note>126)return;for(auto& v:synth_)if(v.active&&v.channel==channel&&v.note==note){v.down=false;if(!sustain_[channel]&&!v.release)v.release=64;}}
 void Sustain(unsigned channel,bool held){if(channel>15)return;sustain_[channel]=held;if(!held)for(auto& v:synth_)if(v.active&&v.channel==channel&&!v.down&&!v.release)v.release=64;}
 void AllNotesOff(){synth_={};sustain_={};}
 void Stop(){AllNotesOff();pads_={};sequence_.Stop();}
 bool SetProject(const Project& p){if(!ValidProject(p))return false;Stop();project_=p;return true;}
 const Project& CurrentProject()const{return project_;}
 bool StartSequence(){return rate_==24000&&sequence_.Start(project_.bpm);}
 void StopSequence(){sequence_.Stop();pads_={};}
 bool SequenceRunning()const{return sequence_.Running();}
 int SequenceStep()const{return sequence_.CurrentStep();}
 unsigned SynthVoices()const{unsigned n=0;for(auto v:synth_)n+=v.active;return n;}
 unsigned SampleVoices()const{unsigned n=0;for(auto v:pads_)n+=v.active;return n;}
 size_t BankBytes()const{return bank_bytes_;}uint64_t ClippedFrames()const{return clipped_;}uint64_t RejectedEvents()const{return rejected_;}
 bool ClearPad(unsigned pad){if(pad>3||SampleVoices()||sequence_.Running())return false;bank_bytes_-=bank_[pad].size()*2;std::vector<int16_t>().swap(bank_[pad]);return true;}
 bool ClearBank(){if(SampleVoices()||sequence_.Running())return false;for(unsigned p=0;p<4;++p)ClearPad(p);return true;}
 bool LoadPadWav(unsigned pad,const uint8_t* b,size_t size){
  if(rate_!=24000||pad>3||SampleVoices()||sequence_.Running()){return false;}
  WavView w;if(!ParseWav(b,size,w)||w.frames>budget_/2)return false;
  // Charge old and replacement PCM simultaneously: allocation peak is bounded.
  // A completely full bank must explicitly clear a pad before replacing it.
  const size_t bytes=w.frames*2;if(bytes>budget_-bank_bytes_)return false;
  std::vector<int16_t> next(w.frames);for(size_t i=0;i<w.frames;++i){uint16_t raw=Le16(w.pcm+i*2);next[i]=raw<32768?int16_t(raw):int16_t(int32_t(raw)-65536);}
  bank_bytes_=bank_bytes_-bank_[pad].size()*2+bytes;bank_[pad]=std::move(next);return true;
 }
 bool TriggerPad(unsigned pad,unsigned velocity=100,bool accent=false){if(pad>3||velocity>127||!velocity||bank_[pad].empty())return false;for(auto& v:pads_)if(!v.active){v={true,accent,uint8_t(pad),uint8_t(velocity),0};return true;}++rejected_;return false;}
 bool Render(int16_t* output,size_t frames){
  if(!output||frames>MaxRenderFrames||rate_!=24000)return false;
  for(size_t i=0;i<frames;++i){uint8_t step=0;if(sequence_.Tick(step)){auto s=project_.steps[step];for(unsigned pad=0;pad<4;++pad)if(s.pad_mask&(1u<<pad))TriggerPad(pad,100,s.accent);}
   int32_t mixed=0;for(auto& v:synth_)mixed+=SynthSample(v);for(auto& v:pads_)mixed+=PadSample(v);if(mixed<-32768||mixed>32767)++clipped_;output[i]=int16_t(std::clamp<int32_t>(mixed,-32768,32767));}
  return true;
 }
 bool InstallBuiltinBank(); // Deterministic original drums; off-render loading.
};
}

namespace stackchan_instruments {
inline uint32_t Engine::Increment(unsigned note){
 static constexpr uint32_t increments[127]={
 1463116u,1550118u,1642292u,1739948u,1843411u,1953026u,2069159u,2192197u,
 2322552u,2460658u,2606977u,2761996u,2926232u,3100235u,3284585u,3479896u,
 3686822u,3906052u,4138318u,4384395u,4645104u,4921317u,5213953u,5523991u,
 5852465u,6200470u,6569170u,6959793u,7373644u,7812103u,8276635u,8768789u,
 9290209u,9842633u,10427907u,11047982u,11704930u,12400941u,13138339u,13919586u,
 14747287u,15624207u,16553270u,17537579u,18580418u,19685267u,20855814u,22095965u,
 23409859u,24801882u,26276679u,27839171u,29494575u,31248413u,33106541u,35075158u,
 37160835u,39370534u,41711627u,44191930u,46819719u,49603764u,52553357u,55678342u,
 58989149u,62496826u,66213081u,70150316u,74321671u,78741067u,83423255u,88383859u,
 93639437u,99207528u,105106715u,111356685u,117978298u,124993653u,132426162u,140300631u,
 148643341u,157482134u,166846509u,176767719u,187278874u,198415056u,210213429u,222713370u,
 235956596u,249987305u,264852324u,280601263u,297286682u,314964268u,333693018u,353535438u,
 374557749u,396830112u,420426858u,445426740u,471913192u,499974611u,529704648u,561202526u,
 594573365u,629928537u,667386037u,707070876u,749115498u,793660223u,840853716u,890853480u,
 943826385u,999949222u,1059409297u,1122405052u,1189146729u,1259857073u,1334772074u,1414141751u,
 1498230996u,1587320447u,1681707432u,1781706960u,1887652769u,1999898444u,2118818594u
 };return increments[note];
}
inline bool Engine::InstallBuiltinBank(){
 if(rate_!=24000||SampleVoices()||sequence_.Running())return false;
 constexpr std::array<size_t,4> sizes{3600,3000,1200,2400};
 constexpr size_t total=20400;if(total>budget_-bank_bytes_)return false;
 std::array<std::vector<int16_t>,4> next;
 for(unsigned pad=0;pad<4;++pad){next[pad].resize(sizes[pad]);uint32_t noise=0x12345678u+pad,phase=0;
  for(size_t i=0;i<sizes[pad];++i){noise=noise*1664525u+1013904223u;int32_t wave=int32_t(noise>>16)-32768;
   if(!pad){phase+=uint32_t((uint64_t(45+(sizes[pad]-i)*100/sizes[pad])<<32)/24000);auto p=phase>>16;wave=p<32768?int32_t(p)*2-32768:98303-int32_t(p)*2;}
   if(pad==3&&(i/240)%2)wave=0;
   next[pad][i]=int16_t(int64_t(wave)*(sizes[pad]-i)/sizes[pad]/3);
  }
 }
 bank_=std::move(next);bank_bytes_=total;return true;
}
}
