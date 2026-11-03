#pragma once
#include "local_tools.hpp"
#include <companion/audio_owner.hpp>
#include "mic_analysis.hpp"
#include <apps/instruments/instrument_commands.hpp>
#include <memory>
#include <string>
#include <audio/audio_codec.h>
#include <freertos/task.h>
#include <atomic>
#include <array>
#include <mutex>
#include <functional>
namespace local_tools_ui {
enum class AudioMode { Fork, Metro, Synth, Meter, Pitch, Key, Instruments, Storage, Spectrogram, PitchPipe };
enum class AudioFault { None, Busy, Format, TaskCreate, Enable, Input, Output, Storage };
struct PreparedInstrumentBank {std::array<std::string,4> wavs;std::array<uint8_t,4> pad_ids{0,1,2,3};};
struct AudioSnapshot { unsigned key_observations=0,key_pitch_classes=0;uint64_t analysis_last_us=0,analysis_max_us=0,analysis_over_budget=0;uint64_t capture_samples=0,pitch_end_sample=0,key_end_sample=0; bool pitch_valid=false;double confidence=0,cents=0;int midi_note=-1; int sequence_step=-1;bool sequence_running=false;unsigned synth_voices=0,sample_voices=0;size_t bank_bytes=0;uint64_t clipped=0,rejected=0; bool running=false,starting=false,stopping=false,cleanup_failed=false; AudioFault fault=AudioFault::None; double dbfs=-INFINITY,hz=0;int tonic=-1;bool minor=false;double margin=0; };
// One global Native client. Stop is a nonblocking request; PollStopped joins the
// cleanup receipt and releases the lease only after both codec directions close.
class LocalAudio {
 std::atomic<bool> done_{true},quit_{false},retry_cleanup_{false};
 std::atomic<unsigned> note_velocity_{69|(64<<8)};
 std::mutex mutex_;AudioSnapshot snapshot_;
 std::function<AudioCodec*()> resolve_;
 AudioCodec* codec_=nullptr;AudioMode mode_=AudioMode::Fork;unsigned bpm_=120;
 companion::AudioLease audio_lease_;
 bool lease_=false,codec_lease_=false;TaskHandle_t task_=nullptr;
 std::vector<int16_t> hardware_,slice_;
 std::unique_ptr<stackchan_tools::MicAnalysis> mic_analysis_;
 std::unique_ptr<stackchan_tools::SpectrogramCache> spectrogram_;
 stackchan_instruments::Project instrument_project_;
 std::shared_ptr<const PreparedInstrumentBank> instrument_bank_;
 stackchan_instruments::Commands instrument_commands_;
 std::function<bool()> storage_work_;
 void Run();
public:
 bool Start(std::function<AudioCodec*()> resolve,AudioMode mode,unsigned bpm=120,unsigned note=69,unsigned initial_velocity=64);
 bool Start(AudioCodec* codec,AudioMode mode,unsigned bpm=120,unsigned note=69){return Start([codec]{return codec;},mode,bpm,note);}
 void RequestStop(){companion::RevokeAudio(audio_lease_);quit_=true;retry_cleanup_=true;}
 bool PollStopped();
 bool StopRequested()const{return quit_.load();}
 bool StartInstruments(std::function<AudioCodec*()> resolve,const stackchan_instruments::Project&,std::shared_ptr<const PreparedInstrumentBank> bank={});
 bool StartStorage(std::function<bool()> work);
 bool InstrumentCommand(const stackchan_instruments::Command&);
 bool Note(unsigned note,unsigned velocity);
 AudioSnapshot Snapshot();
 bool CopySpectrogramSnapshot(stackchan_tools::SpectrogramCache& destination);
 void SetSpectrogramFrozen(bool frozen);
};
LocalAudio& AudioTools();
}
