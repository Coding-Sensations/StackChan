#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>
namespace stackchan_tools {
// Rejection sampling: callers supply independent hardware-random words.
inline std::optional<uint32_t> Dice(uint32_t sides,uint32_t random) {
 if(sides<2||sides>1000)return {};
 const uint32_t threshold=uint32_t(-sides)%sides;
 if(random<threshold)return {};
 return 1+random%sides;
}
struct Pet {
 uint8_t hunger=20,happiness=80,energy=80;uint64_t remainder_ms=0;uint8_t decay_phase_minutes=0;
 bool Advance(uint64_t elapsed_ms) {
  if(hunger>100||happiness>100||energy>100||remainder_ms>=60000||decay_phase_minutes>=6||elapsed_ms>UINT64_MAX-remainder_ms)return false;
  auto total=elapsed_ms+remainder_ms;auto minutes=total/60000;remainder_ms=total%60000;
  hunger=uint8_t(std::min<uint64_t>(100,hunger+minutes));
  auto phased=minutes+decay_phase_minutes;auto unhappy=phased/2-decay_phase_minutes/2;auto tired=phased/3-decay_phase_minutes/3;
  happiness=uint8_t(unhappy>=happiness?0:happiness-unhappy);
  energy=uint8_t(tired>=energy?0:energy-tired);decay_phase_minutes=phased%6;return true;
 }
 void Feed(){hunger=uint8_t(hunger>20?hunger-20:0);}
 void Play(){happiness=uint8_t(std::min(100,int(happiness)+15));energy=uint8_t(energy>10?energy-10:0);}
 void Rest(){energy=uint8_t(std::min(100,int(energy)+20));}
};
inline int WifiBars(int rssi,bool connected) {
 if(!connected)return 0;
 return rssi>=-55?4:rssi>=-65?3:rssi>=-75?2:1;
}
inline double MidiFrequency(unsigned note) {
 return note<=127?440.0*std::pow(2.0,(int(note)-69)/12.0):0;
}
// PCM renderer shared by tuning fork, metronome click and MIDI monophonic synth.
// No physical audio API: lifecycle/ownership adapters must opt in separately.
class Synth {
 double phase_=0,frequency_=0,amplitude_=0;unsigned rate_;int note_=-1;
public:
 explicit Synth(unsigned rate=24000):rate_(rate){}
 bool NoteOn(unsigned note,unsigned velocity) {
  if(note>127||velocity>127||rate_<8000||MidiFrequency(note)>=rate_/2.0)return false;
  if(!velocity){NoteOff(note);return true;}
  note_=int(note);frequency_=MidiFrequency(note);amplitude_=2048.0*velocity/127;return true;
 }
 void NoteOff(unsigned note){if(note_==int(note)){frequency_=amplitude_=0;note_=-1;}}
 void Stop(){frequency_=amplitude_=phase_=0;note_=-1;}
 void Render(int16_t* output,size_t frames) {
  if(rate_<8000||rate_>48000){std::fill_n(output,frames,int16_t(0));return;}
  for(size_t i=0;i<frames;++i){output[i]=int16_t(std::lround(std::sin(phase_)*amplitude_));
   phase_+=6.283185307179586*frequency_/rate_;if(phase_>=6.283185307179586)phase_=std::fmod(phase_,6.283185307179586);}
 }
};
class Metronome {
 unsigned rate_;double next_=0,interval_=0;uint64_t cursor_=0;unsigned beat_=0,beats_=4;
public:
 explicit Metronome(unsigned rate=24000):rate_(rate){}
 bool Start(unsigned bpm,unsigned beats=4) {
  if(rate_<8000||bpm<20||bpm>300||beats<1||beats>16)return false;
  interval_=double(rate_)*60/bpm;next_=0;cursor_=0;beat_=0;beats_=beats;return true;
 }
 void Stop(){interval_=0;}
 // Returns offsets+accent flags; no drift from UI tick scheduling.
 std::vector<std::pair<size_t,bool>> Process(size_t frames) {
  std::vector<std::pair<size_t,bool>> result;if(!interval_||frames>rate_*10ULL)return result;
  auto end=cursor_+frames;
  while(next_<double(end)){auto pos=uint64_t(std::llround(next_));if(pos>=end)break;
   if(pos>=cursor_)result.push_back({size_t(pos-cursor_),beat_==0});
   beat_=(beat_+1)%beats_;next_+=interval_;}
  cursor_=end;return result;
 }
};
inline double Dbfs(const int16_t* samples,size_t count) {
 if(!count)return -std::numeric_limits<double>::infinity();
 double sum=0;for(size_t i=0;i<count;++i){double x=samples[i]/32768.0;sum+=x*x;}
 return sum>0?10*std::log10(sum/count):-std::numeric_limits<double>::infinity();
}
struct Pitch {double hz=0,confidence=0;};
// Bounded monophonic autocorrelation. Polyphonic music key is a separate accumulator.
inline Pitch DetectPitch(const int16_t* samples,size_t count,unsigned rate) {
 if(count<512||count>4096||rate<8000||rate>48000)return {};
 double mean=0;for(size_t i=0;i<count;++i)mean+=samples[i];mean/=count;
 double energy=0;for(size_t i=0;i<count;++i){auto x=samples[i]-mean;energy+=x*x;}
 if(energy/count<16)return {};
 const size_t low=rate/1000,high=std::min<size_t>(rate/60,count/2);
 std::vector<double> scores(high+2,0);double best=0;
 for(size_t k=low-1;k<=high+1;++k){double dot=0,a=0,b=0;
  for(size_t i=0;i+k<count;++i){double x=samples[i]-mean,y=samples[i+k]-mean;dot+=x*y;a+=x*x;b+=y*y;}
  scores[k]=a*b>0?dot/std::sqrt(a*b):0;best=std::max(best,scores[k]);}
 if(best<0.8)return {};
 for(size_t k=low;k<=high;++k)if(scores[k]>=best-0.02&&scores[k]>=scores[k-1]&&scores[k]>scores[k+1]){
  double denominator=scores[k-1]-2*scores[k]+scores[k+1];
  double offset=denominator!=0?0.5*(scores[k-1]-scores[k+1])/denominator:0;
  offset=std::clamp(offset,-0.5,0.5);return {rate/(k+offset),scores[k]};}
 return {};
}
class KeyAnalyzer {
 std::array<double,12> chroma_{};unsigned observations_=0;
public:
 bool Observe(double hz,double weight=1) {
  if(!std::isfinite(hz)||hz<60||hz>2000||!std::isfinite(weight)||weight<=0||weight>100)return false;
  int note=int(std::lround(69+12*std::log2(hz/440)));auto pc=(note%12+12)%12;
  chroma_[pc]+=weight;++observations_;return true;
 }
 struct Result {int tonic=-1;bool minor=false;double margin=0;};
 Result Estimate() const {
  if(observations_<12)return {};
  // Krumhansl-style correlation is a heuristic estimate, not a key assertion.
  const std::array<double,12> major{6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88};
  const std::array<double,12> minor{6.33,2.68,3.52,5.38,2.60,3.53,2.54,4.75,3.98,2.69,3.34,3.17};
  double cm=0;for(auto x:chroma_)cm+=x;cm/=12;
  double best=-2,second=-2;Result r;
  for(int m=0;m<2;++m)for(int tonic=0;tonic<12;++tonic){auto& p=m?minor:major;double pm=0;for(auto x:p)pm+=x;pm/=12;
   double dot=0,a=0,b=0;for(int i=0;i<12;++i){double x=chroma_[i]-cm,y=p[(i-tonic+12)%12]-pm;dot+=x*y;a+=x*x;b+=y*y;}
   double s=a*b>0?dot/std::sqrt(a*b):-2;
   if(s>best){second=best;best=s;r.tonic=tonic;r.minor=m;}else second=std::max(second,s);}
  if(best<0.3)return {};
  r.margin=best-second;return r;
 }
};
}
