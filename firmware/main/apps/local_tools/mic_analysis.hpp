#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
namespace stackchan_tools {
struct PitchEstimate {bool valid=false;double hz=0,confidence=0,cents=0;int midi_note=-1;};
// Sample-clock timestamps, not wall time. Input is mono PCM16 at 24 kHz.
struct SpectrumFrame {std::array<uint8_t,64> intensity{};uint64_t end_sample=0;};
struct SpectrogramCache {
 static constexpr unsigned Bands=64,Columns=96,Rate=24000,Hop=480,Fft=1024;
 std::array<SpectrumFrame,Columns> columns{};unsigned count=0,next=0;uint64_t sequence=0;
 bool frozen=false,live=false;
 void Push(const SpectrumFrame& f){if(frozen)return;columns[next]=f;next=(next+1)%Columns;count=std::min(count+1,Columns);++sequence;}
 const SpectrumFrame* Chronological(unsigned i)const{return i<count?&columns[(next+Columns-count+i)%Columns]:nullptr;}
 // Logarithmic band edges in Hz; magnitude is normalized peak-amplitude dBFS,
 // display-clipped to -90..0, never calibrated SPL or acoustic power density.
 static double FrequencyEdge(unsigned edge){return (24000.0/Fft)*std::pow(512.0,double(std::min(edge,Bands))/Bands);}
 static double Dbfs(uint8_t value){return -90.0+90.0*value/255;}
};
class MicAnalysis {
 std::array<int16_t,2048> pitch_pcm_{};size_t pitch_count_=0;
 std::array<float,402> correlation_{};
 std::array<int16_t,1024> ring_{};size_t head_=0,filled_=0,hop_=0;
 std::array<float,1024> real_{},imag_{},window_{};float window_sum_=0;
 std::array<uint8_t,513> band_map_{};std::array<float,11> step_real_{},step_imag_{};
 uint64_t samples_=0,pitch_end_=0;PitchEstimate pitch_{};SpectrumFrame spectrum_{};
 bool pitch_updated_=false,spectrum_updated_=false;
 void Pitch(){
  pitch_={};pitch_end_=samples_;pitch_updated_=true;
  float mean=0;for(auto x:pitch_pcm_)mean+=x;mean/=pitch_pcm_.size();
  float energy=0;for(auto x:pitch_pcm_){float d=x-mean;energy+=d*d;}
  if(energy/pitch_pcm_.size()<16)return;
  constexpr size_t low=24,high=400;float best=0;
  for(size_t k=low-1;k<=high+1;++k){float dot=0,a=0,b=0;
   for(size_t i=0;i+k<pitch_pcm_.size();++i){float x=pitch_pcm_[i]-mean,y=pitch_pcm_[i+k]-mean;dot+=x*y;a+=x*x;b+=y*y;}
   correlation_[k]=a*b>0?dot/std::sqrt(a*b):0;best=std::max(best,correlation_[k]);}
  if(best<.8)return;
  for(size_t k=low;k<=high;++k)if(correlation_[k]>=best-.02&&correlation_[k]>=correlation_[k-1]&&correlation_[k]>correlation_[k+1]){
   float denominator=correlation_[k-1]-2*correlation_[k]+correlation_[k+1];
   float offset=denominator?std::clamp(.5f*(correlation_[k-1]-correlation_[k+1])/denominator,-.5f,.5f):0;
   double hz=24000/(k+offset);if(hz<60||hz>1000)return;
   double midi=69+12*std::log2(hz/440);int nearest=int(std::lround(midi));
   pitch_={true,hz,std::clamp(correlation_[k],0.f,1.f),100*(midi-nearest),nearest};return;
  }
 }
 void Spectrum(){
  float mean=0;for(auto x:ring_)mean+=x;mean/=1024;
  for(size_t i=0;i<1024;++i){real_[i]=(ring_[(head_+i)%1024]-mean)/32768.*window_[i];imag_[i]=0;}
  for(size_t i=1,j=0;i<1024;++i){size_t bit=512;for(;j&bit;bit>>=1)j^=bit;j^=bit;if(i<j){std::swap(real_[i],real_[j]);std::swap(imag_[i],imag_[j]);}}
  for(size_t length=2,stage=1;length<=1024;length*=2,++stage){
   const float step_r=step_real_[stage],step_i=step_imag_[stage];
   for(size_t base=0;base<1024;base+=length){float wr=1,wi=0;
    for(size_t j=0;j<length/2;++j){size_t a=base+j,b=a+length/2;float tr=wr*real_[b]-wi*imag_[b],ti=wr*imag_[b]+wi*real_[b];
     real_[b]=real_[a]-tr;imag_[b]=imag_[a]-ti;real_[a]+=tr;imag_[a]+=ti;
     float next=wr*step_r-wi*step_i;wi=wr*step_i+wi*step_r;wr=next;}}}
  spectrum_={};spectrum_.end_sample=samples_;
  std::array<float,64> amplitudes{};
  // Bin centres map into logarithmic bands. Empty sub-bin bands stay dark;
  // no interpolation can manufacture resolution below the 23.4375 Hz bins.
  for(unsigned bin=1;bin<=512;++bin){unsigned band=band_map_[bin];
   float scale=bin==512?1:2;amplitudes[band]=std::max(amplitudes[band],scale*std::hypot(real_[bin],imag_[bin])/window_sum_);}
  for(unsigned band=0;band<64;++band){double db=amplitudes[band]>0?20*std::log10(amplitudes[band]):-90;
   spectrum_.intensity[band]=uint8_t(std::lround(std::clamp((db+90)/90,0.,1.)*255));}
  spectrum_updated_=true;
 }
public:
 MicAnalysis(){for(size_t i=0;i<1024;++i){window_[i]=.5-.5*std::cos(6.283185307179586*i/1023);window_sum_+=window_[i];}for(unsigned bin=1;bin<=512;++bin)band_map_[bin]=uint8_t(std::min(63u,unsigned(std::log(double(bin))/std::log(512.)*64)));for(unsigned stage=1;stage<=10;++stage){float angle=float(-6.283185307179586/(1u<<stage));step_real_[stage]=std::cos(angle);step_imag_[stage]=std::sin(angle);}}
 void Push(const int16_t* pcm,size_t count,bool spectrogram=true,bool pitch=true){
  pitch_updated_=spectrum_updated_=false;
  if(!pcm||count>480)return;
  for(size_t i=0;i<count;++i){++samples_;if(pitch){pitch_pcm_[pitch_count_++]=pcm[i];
   if(pitch_count_==2048){Pitch();pitch_count_=0;}}
   if(!spectrogram)continue;
   ring_[head_]=pcm[i];head_=(head_+1)%1024;
   if(filled_<1024){++filled_;if(filled_==1024){hop_=0;if(spectrogram)Spectrum();}}
   else if(++hop_==480){hop_=0;if(spectrogram)Spectrum();}
  }
 }
 bool PitchUpdated()const{return pitch_updated_;}const PitchEstimate& Estimate()const{return pitch_;}
 bool SpectrumUpdated()const{return spectrum_updated_;}const SpectrumFrame& Frame()const{return spectrum_;}
 uint64_t Samples()const{return samples_;}uint64_t PitchEndSample()const{return pitch_end_;}
};
}
