#pragma once
#include "esphome__esp-micro-speech-features/include/frontend_util.h"
#include "frontend_feature_contract.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>
namespace mira_native_frontend {
inline constexpr bool RuntimeAvailable=false;
// Owner-thread only. Keep Bridge and borrowed PCM alive throughout Feed/callback.
// Destruction during a callback and concurrent access are prohibited.
class Bridge final {
public:
 Bridge() noexcept = default; // inert: no allocations, devices or model initialization
 Bridge(const Bridge&)=delete;Bridge& operator=(const Bridge&)=delete;
 ~Bridge(){Close();}
 bool Configure() noexcept {
  if(busy_){cancel_=true;return false;}
  Close();FrontendConfig config{};FrontendFillConfigWithDefaults(&config);
  config.window.size_ms=30;config.window.step_size_ms=10;
  config.filterbank.num_channels=40;config.filterbank.upper_band_limit=7500;
  config.filterbank.lower_band_limit=125;config.filterbank.output_scale_shift=0;
  config.noise_reduction.smoothing_bits=10;config.noise_reduction.even_smoothing=.025f;
  config.noise_reduction.odd_smoothing=.06f;config.noise_reduction.min_signal_remaining=.05f;
  config.pcan_gain_control.enable_pcan=1;config.pcan_gain_control.strength=.95f;
  config.pcan_gain_control.offset=80;config.pcan_gain_control.gain_bits=21;
  config.log_scale.enable_log=1;config.log_scale.scale_shift=6;
  if(!FrontendPopulateState(&config,&state_,16000)) {Close();return false;}
  if(state_.window.size!=480 || state_.window.step!=160 || state_.fft.fft_size!=512 || state_.filterbank.num_channels!=40 || state_.window.coefficients==nullptr) {Close();return false;}
  // Canonical table is mandatory; generated coefficients never escape Configure.
  std::copy(mira_frontend::WindowCoefficients.begin(),mira_frontend::WindowCoefficients.end(),state_.window.coefficients);
  ready_=true;FrontendReset(&state_);return true;
 }
 bool Reset() noexcept {if(busy_){cancel_=true;return false;}if(!ready_)return false;Close();return Configure();}
 void Close() noexcept {
  if(busy_){cancel_=true;return;}
  // Volatile clearing covers PCM/intermediates including FFT scratch temporaries.
  Wipe(state_.window.input,state_.window.size*sizeof(std::int16_t));
  Wipe(state_.window.output,state_.window.size*sizeof(std::int16_t));
  Wipe(state_.fft.input,state_.fft.fft_size*sizeof(std::int16_t));
  Wipe(state_.fft.output,(state_.fft.fft_size/2+1)*sizeof(complex_int16_t)*2);
  Wipe(state_.fft.scratch,state_.fft.scratch_size);
  Wipe(state_.filterbank.work,(static_cast<std::size_t>(state_.filterbank.num_channels)+1)*sizeof(std::uint64_t));
  Wipe(state_.noise_reduction.estimate,static_cast<std::size_t>(state_.noise_reduction.num_channels)*sizeof(std::uint32_t));
  // PCAN noise_estimate aliases the wiped noise estimate; gain_lut is static derived data.
  FrontendFreeStateContents(&state_);Wipe(&state_,sizeof(state_));
  ready_=false;cancel_=false;samples_=frames_=0;stride_.Reset();
 }
 template<class Consumer> bool Feed(const std::int16_t* pcm,std::size_t count,Consumer&& consumer) noexcept {
  static_assert(noexcept(consumer(std::declval<const std::array<std::uint16_t,40>&>())),"raw consumer must be synchronous and noexcept");
  if(busy_){cancel_=true;return false;}
  if(!ready_ || count>512 || (pcm==nullptr && count!=0) || count>std::numeric_limits<std::size_t>::max()-samples_) {Close();return false;}
  busy_=true;cancel_=false;
  std::size_t consumed=0;std::array<std::uint16_t,40> owned{};
  while(consumed<count) {
   std::size_t read=0;const auto out=FrontendProcessSamples(&state_,pcm+consumed,count-consumed,&read);
   if(read==0 || read>count-consumed || (out.size!=0 && (out.size!=40 || out.values==nullptr))) {Wipe(owned.data(),sizeof(owned));busy_=false;Close();return false;}
   consumed+=read;
   if(out.size) {
    std::copy(out.values,out.values+40,owned.begin());++frames_;
    if(!consumer(owned) || cancel_) {Wipe(owned.data(),sizeof(owned));busy_=false;Close();return false;}
    Wipe(owned.data(),sizeof(owned));
   }
  }
  samples_+=consumed;
  if(frames_!=mira_frontend::CompleteFrames(samples_) || cancel_) {busy_=false;Close();return false;}
  busy_=false;return true;
 }
 bool Pack(const std::array<std::uint16_t,40>& raw,std::array<std::int8_t,120>& out) noexcept {
  if(!ready_ || cancel_){out.fill(0);return false;}return stride_.Push(raw.data(),raw.size(),out);
 }
 bool Configured() const noexcept{return ready_;}
 std::size_t Samples() const noexcept{return samples_;}
 std::size_t Frames() const noexcept{return frames_;}
private:
 static void Wipe(void* pointer,std::size_t count) noexcept {
  if(pointer==nullptr){return;}
  auto bytes=static_cast<volatile unsigned char*>(pointer);
  for(std::size_t i=0;i<count;++i)bytes[i]=0;
 }
 FrontendState state_{};bool ready_{false},busy_{false},cancel_{false};std::size_t samples_{0},frames_{0};mira_frontend::StrideAssembler stride_{};
};
} // namespace mira_native_frontend
