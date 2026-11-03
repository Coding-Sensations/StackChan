#include "local_audio.h"
#include <companion/audio_owner.hpp>
#include <algorithm>
#if defined(ESP_PLATFORM)
#include <esp_timer.h>
#else
#include <chrono>
#endif
namespace local_tools_ui {
static std::atomic<bool> native_client{false};
static uint64_t AnalysisClockUs(){
#if defined(ESP_PLATFORM)
 return uint64_t(esp_timer_get_time());
#else
 return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
#endif
}
LocalAudio& AudioTools(){static LocalAudio tools;return tools;}
bool LocalAudio::Start(std::function<AudioCodec*()> resolve,AudioMode mode,unsigned bpm,unsigned note,unsigned initial_velocity){
 if(!done_||lease_)return false;
 {std::lock_guard<std::mutex> l(mutex_);snapshot_={};}
 auto fail=[this](AudioFault f){std::lock_guard<std::mutex> l(mutex_);snapshot_.fault=f;return false;};
 if(!resolve||unsigned(mode)>unsigned(AudioMode::PitchPipe)||bpm<20||bpm>300||note>126||initial_velocity>127)return fail(AudioFault::Format);
 // Storage uses this object's joined worker custody, not the global codec lease.
 // Independent Mira/codec clients may continue; SD/display owns its real bus lock.
 codec_lease_=mode!=AudioMode::Storage;
 if(codec_lease_){
  bool expected=false;
  if(!native_client.compare_exchange_strong(expected,true))return fail(AudioFault::Busy);
  if(!companion::AcquireAudio(companion::AudioOwner::Native,companion::AudioPurpose::NativeRuntime,1,audio_lease_)){native_client=false;return fail(AudioFault::Busy);}
 }
 lease_=true;
 if(mode!=AudioMode::Storage)storage_work_={};
 {std::lock_guard<std::mutex> l(mutex_);instrument_commands_.Clear();spectrogram_.reset();}
 resolve_=std::move(resolve);codec_=nullptr;mode_=mode;bpm_=bpm;note_velocity_=note|(initial_velocity<<8);quit_=false;retry_cleanup_=false;
 done_=false;
 if(xTaskCreate([](void* p){auto self=static_cast<LocalAudio*>(p);self->Run();self->done_=true;vTaskDelete(nullptr);},"local_audio",16384,this,4,&task_)!=pdPASS){done_=true;if(codec_lease_){companion::ReleaseAudio(audio_lease_,true);native_client=false;}codec_lease_=false;lease_=false;return fail(AudioFault::TaskCreate);}
 return true;
}
bool LocalAudio::PollStopped(){
 if(!done_)return false;
 if(lease_){if(codec_lease_){if(!companion::ReleaseAudio(audio_lease_,true))return false;native_client=false;}codec_lease_=false;lease_=false;codec_=nullptr;task_=nullptr;}
 return true;
}
bool LocalAudio::Note(unsigned note,unsigned velocity){if(note>126||velocity>127)return false;note_velocity_=note|(velocity<<8);return true;}
bool LocalAudio::StartInstruments(std::function<AudioCodec*()> resolve,const stackchan_instruments::Project& project,std::shared_ptr<const PreparedInstrumentBank> bank){
 if(!PollStopped()||!stackchan_instruments::ValidProject(project))return false;
 if(bank){if(bank->pad_ids!=project.pad_ids)return false;for(const auto& wav:bank->wavs)if(wav.empty()||wav.size()>96*1024)return false;}
 else if(project.pad_ids!=std::array<uint8_t,4>{0,1,2,3})return false;
 instrument_project_=project;instrument_bank_=std::move(bank);
 return Start(std::move(resolve),AudioMode::Instruments,project.bpm,69,0);
}
bool LocalAudio::StartStorage(std::function<bool()> work){if(!work||!PollStopped())return false;storage_work_=std::move(work);return Start([]{return static_cast<AudioCodec*>(nullptr);},AudioMode::Storage);}
bool LocalAudio::InstrumentCommand(const stackchan_instruments::Command& command){std::lock_guard<std::mutex> l(mutex_);if(done_||quit_||mode_!=AudioMode::Instruments)return false;const bool ok=instrument_commands_.Push(command);if(!ok)++snapshot_.rejected;return ok;}
AudioSnapshot LocalAudio::Snapshot(){std::lock_guard<std::mutex> l(mutex_);auto s=snapshot_;s.starting=!done_&&!quit_&&!s.running;s.stopping=quit_&&!done_;s.pitch_valid=s.pitch_valid&&s.running&&!quit_&&s.capture_samples>=s.pitch_end_sample&&s.capture_samples-s.pitch_end_sample<=12000;if(!s.running||quit_||!s.key_end_sample||s.capture_samples-s.key_end_sample>24000)s.tonic=-1;return s;}
bool LocalAudio::CopySpectrogramSnapshot(stackchan_tools::SpectrogramCache& out){std::lock_guard<std::mutex> l(mutex_);if(!spectrogram_)return false;out=*spectrogram_;out.live=out.live&&snapshot_.running&&!quit_;return true;}
void LocalAudio::SetSpectrogramFrozen(bool frozen){std::lock_guard<std::mutex> l(mutex_);if(spectrogram_)spectrogram_->frozen=frozen;}
void LocalAudio::Run(){
 auto fail=[this](AudioFault fault){std::lock_guard<std::mutex> l(mutex_);snapshot_.running=false;snapshot_.sequence_running=false;snapshot_.sequence_step=-1;snapshot_.synth_voices=snapshot_.sample_voices=0;snapshot_.fault=fault;};
 if(quit_)return;
 if(mode_==AudioMode::Storage){bool ok=false;{std::lock_guard<std::mutex> l(mutex_);snapshot_.running=true;}try{ok=storage_work_&&storage_work_();}catch(...){ok=false;}{std::lock_guard<std::mutex> l(mutex_);snapshot_.running=false;snapshot_.fault=ok||quit_?AudioFault::None:AudioFault::Storage;}return;}

 // Lazy board/codec construction belongs to this admitted worker, never LVGL.
 codec_=resolve_();
 if(!codec_||codec_->input_sample_rate()!=24000||codec_->output_sample_rate()!=24000||codec_->input_channels()<1||codec_->input_channels()>2||codec_->output_channels()!=1){fail(AudioFault::Format);return;}
 if(codec_->input_enabled()||codec_->output_enabled()){fail(AudioFault::Busy);return;}
 hardware_.assign(480*codec_->input_channels(),0);slice_.assign(480,0);
 const bool capture=mode_==AudioMode::Meter||mode_==AudioMode::Pitch||mode_==AudioMode::Key||mode_==AudioMode::Spectrogram;
 if(capture&&mode_!=AudioMode::Meter){mic_analysis_=std::make_unique<stackchan_tools::MicAnalysis>();if(mode_==AudioMode::Spectrogram){std::lock_guard<std::mutex> l(mutex_);spectrogram_=std::make_unique<stackchan_tools::SpectrogramCache>();}}else mic_analysis_.reset();
 stackchan_instruments::Engine instrument;
 if(mode_==AudioMode::Instruments){bool prepared=instrument.SetProject(instrument_project_);if(instrument_bank_){for(unsigned pad=0;pad<4&&prepared;++pad){const auto& wav=instrument_bank_->wavs[pad];prepared=instrument.LoadPadWav(pad,reinterpret_cast<const uint8_t*>(wav.data()),wav.size());}}else prepared=prepared&&instrument.InstallBuiltinBank();if(!prepared){fail(AudioFault::Format);return;}instrument_bank_.reset();}
 stackchan_tools::Synth synth;stackchan_tools::Metronome metro;stackchan_tools::KeyAnalyzer keys;unsigned key_observations=0;std::array<bool,12> key_classes{};
 unsigned active_note=128,active_velocity=128;size_t click_age=120;double click_hz=1200;
 if(mode_==AudioMode::Metro)metro.Start(bpm_);
 const bool enabled=companion::CommonAudioAuthority().Current(audio_lease_)&&codec_->EnableInputChecked(capture)&&codec_->EnableOutputChecked(!capture);
 {std::lock_guard<std::mutex> l(mutex_);snapshot_.running=enabled;}
 AudioFault fault=enabled?AudioFault::None:AudioFault::Enable;
 while(enabled&&!quit_&&companion::CommonAudioAuthority().Current(audio_lease_)){
  if(capture){
   if(!codec_->InputDataChecked(hardware_)){fault=AudioFault::Input;break;}
   if(quit_||!companion::CommonAudioAuthority().Current(audio_lease_))break;
   for(size_t i=0;i<480;++i)slice_[i]=hardware_[i*codec_->input_channels()];
   const double level=stackchan_tools::Dbfs(slice_.data(),slice_.size());
   if(mode_==AudioMode::Meter){std::lock_guard<std::mutex> l(mutex_);snapshot_.dbfs=level;snapshot_.capture_samples+=slice_.size();continue;}
   const auto analysis_start=AnalysisClockUs();
   mic_analysis_->Push(slice_.data(),slice_.size(),mode_==AudioMode::Spectrogram,mode_==AudioMode::Pitch||mode_==AudioMode::Key);
   const auto analysis_us=AnalysisClockUs()-analysis_start;
   const auto& pitch=mic_analysis_->Estimate();
   if(mode_==AudioMode::Key&&mic_analysis_->PitchUpdated()&&pitch.valid){keys.Observe(pitch.hz,pitch.confidence);++key_observations;key_classes[(pitch.midi_note%12+12)%12]=true;}
   auto key=keys.Estimate();
   {std::lock_guard<std::mutex> l(mutex_);snapshot_.dbfs=level;snapshot_.capture_samples=mic_analysis_->Samples();snapshot_.analysis_last_us=analysis_us;snapshot_.analysis_max_us=std::max(snapshot_.analysis_max_us,analysis_us);if(analysis_us>20000)++snapshot_.analysis_over_budget;
    if(mic_analysis_->PitchUpdated()){snapshot_.hz=pitch.hz;snapshot_.pitch_valid=pitch.valid;snapshot_.confidence=pitch.confidence;snapshot_.cents=pitch.cents;snapshot_.midi_note=pitch.midi_note;snapshot_.pitch_end_sample=mic_analysis_->PitchEndSample();}
    if(mode_==AudioMode::Key){snapshot_.key_observations=key_observations;snapshot_.key_pitch_classes=unsigned(std::count(key_classes.begin(),key_classes.end(),true));snapshot_.tonic=snapshot_.key_pitch_classes>=3?key.tonic:-1;snapshot_.minor=key.minor;snapshot_.margin=key.margin;if(mic_analysis_->PitchUpdated()&&pitch.valid)snapshot_.key_end_sample=snapshot_.pitch_end_sample;}
    if(spectrogram_){spectrogram_->live=true;if(mic_analysis_->SpectrumUpdated())spectrogram_->Push(mic_analysis_->Frame());}
   }
  }else{
   if(mode_==AudioMode::Instruments){
    {std::lock_guard<std::mutex> l(mutex_);stackchan_instruments::Command command;while(instrument_commands_.Pop(command))if(!stackchan_instruments::Apply(instrument,command))++snapshot_.rejected;}
    if(!instrument.Render(slice_.data(),slice_.size())){fault=AudioFault::Format;break;}
    {std::lock_guard<std::mutex> l(mutex_);snapshot_.sequence_running=instrument.SequenceRunning();snapshot_.sequence_step=instrument.SequenceStep();snapshot_.synth_voices=instrument.SynthVoices();snapshot_.sample_voices=instrument.SampleVoices();snapshot_.bank_bytes=instrument.BankBytes();snapshot_.clipped=instrument.ClippedFrames();}
   }else if(mode_==AudioMode::Metro){
    auto clicks=metro.Process(slice_.size());size_t next=0;
    for(size_t i=0;i<slice_.size();++i){if(next<clicks.size()&&clicks[next].first==i){click_age=0;click_hz=clicks[next++].second?2000:1200;}
     slice_[i]=click_age<120?int16_t(std::lround(1024*(1-double(click_age)/120)*std::sin(6.283185307179586*click_hz*click_age/24000))):0;if(click_age<120)++click_age;}
   }else{
    const unsigned command=note_velocity_.load();
    const unsigned next_note=mode_==AudioMode::Fork?69:(command&255),next_velocity=mode_==AudioMode::Fork?64:(command>>8);
    if(next_note!=active_note||next_velocity!=active_velocity){synth.Stop();if(!synth.NoteOn(next_note,next_velocity)){fault=AudioFault::Format;break;}active_note=next_note;active_velocity=next_velocity;}
    synth.Render(slice_.data(),slice_.size());
   }
   if(quit_||!companion::CommonAudioAuthority().Current(audio_lease_))break;
   if(!codec_->OutputDataChecked(slice_)){fault=AudioFault::Output;break;}
  }

 }
 instrument.Stop();
 companion::RevokeAudio(audio_lease_);
 // Joined cleanup remains in this worker; no UI thread performs blocking IO.
 quit_=true;
 for(;;){
  retry_cleanup_=false;
  const bool input_closed=codec_->EnableInputChecked(false);
  const bool output_closed=codec_->EnableOutputChecked(false);
  {std::lock_guard<std::mutex> l(mutex_);snapshot_.running=false;if(spectrogram_)spectrogram_->live=false;snapshot_.sequence_running=false;snapshot_.sequence_step=-1;snapshot_.synth_voices=snapshot_.sample_voices=0;snapshot_.fault=fault;snapshot_.cleanup_failed=!(input_closed&&output_closed);}
  if(input_closed&&output_closed)break;
  // Preserve worker/lease on failed cleanup. A subsequent physical Stop retries.
  while(!retry_cleanup_)vTaskDelay(pdMS_TO_TICKS(10));
 }
}
}
