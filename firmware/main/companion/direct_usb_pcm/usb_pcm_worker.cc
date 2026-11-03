#include "usb_pcm_worker.h"
#include <algorithm>
#include <exception>
namespace direct_usb_pcm {
Worker::~Worker(){if(!Closed())std::terminate();} // Never destroy unresolved ownership.
void Worker::Fail(Fault f){std::lock_guard<std::mutex> lock(mutex_);if(snapshot_.fault==Fault::None)snapshot_.fault=f;active_=false;quit_=true;session_.mute();count_=0;}
bool Worker::Current(){return admission_&&!quit_&&(!trial_deadline_||now_()<trial_deadline_)&&now_&&now_()<admission_->Bound().deadline_ms&&authority_.Current(*admission_,now_())&&endpoint_.Current(*admission_);}
Snapshot Worker::Status(){std::lock_guard<std::mutex> lock(mutex_);auto s=snapshot_;s.selected=bool(admission_);s.active=active_&&!quit_;s.playing=playing_;s.closing=quit_&&!Closed();s.queued=count_;return s;}
bool Worker::Select(std::function<AudioCodec*()> resolve){
 if(!Closed()||!now_||!resolve)return false;
 auto grant=authority_.Take(now_());if(!grant)return false;
 const auto& id=grant->Bound();if(!id.stream||!id.epoch||id.epoch<=last_epoch_||used_streams_.test(id.stream)||id.deadline_ms<=now_()||!authority_.Current(*grant,now_()))return false;
 // CoreS3 driver reserves the single common DirectUsb lease atomically with its independent driver generation.
 {std::lock_guard<std::mutex> setup_lock(mutex_);admission_=std::move(grant);last_epoch_=id.epoch;used_streams_.set(id.stream);quit_=false;active_=known_=playing_=false;speaker_sequence_=mic_sequence_=0;count_=head_=0;trial_start_=trial_deadline_=0;snapshot_={};session_={};closed_=false;}
 const auto claimed=endpoint_.Claim(*admission_);usb_claim_=claimed!=ClaimResult::Refused;
 if(claimed!=ClaimResult::Owned){Fail(Fault::UsbClaim);Cleanup();return false;}
 if(!Current()){Fail(Fault::Authority);Cleanup();return false;}
 auto resolved=resolve();
 if(!resolved||resolved->input_sample_rate()!=24000||resolved->output_sample_rate()!=24000||resolved->input_channels()<1||resolved->input_channels()>2||resolved->input_enabled()||resolved->output_enabled()){Fail(Fault::Format);Cleanup();return false;}
 codec_=resolved;
 if(!codec_->DirectUsbCodecAcquire(id.epoch)){Fail(Fault::Busy);Cleanup();return false;}
 codec_claim_=true;drain_pending_=false;
 try{pending_.clear();pending_.reserve(832);hardware_.assign(480*codec_->input_channels(),0);mono_.assign(480,0);converted_.assign(512,0);slice_.assign(480,0);}catch(...){Fail(Fault::Format);Cleanup();return false;}
 esp_ae_rate_cvt_cfg_t cfg={};cfg.src_rate=24000;cfg.dest_rate=16000;cfg.channel=1;cfg.bits_per_sample=16;cfg.complexity=2;cfg.perf_type=ESP_AE_RATE_CVT_PERF_TYPE_SPEED;
 if(esp_ae_rate_cvt_open(&cfg,&resampler_)!=ESP_AE_ERR_OK){Fail(Fault::Resampler);Cleanup();return false;}
 // Selection/configuration does not start the active trial or heartbeat.
 // Those begin on the first admitted direction after human preparation.
 {std::lock_guard<std::mutex> lock(mutex_);active_=true;}return true;
}
bool Worker::CurrentSession(uint32_t epoch,uint16_t stream){std::lock_guard<std::mutex> lock(mutex_);return Current()&&epoch==admission_->Bound().epoch&&stream==admission_->Bound().stream;}
bool Worker::Heartbeat(uint32_t epoch,uint16_t stream){std::lock_guard<std::mutex> lock(mutex_);if(!Current()||epoch!=admission_->Bound().epoch||stream!=admission_->Bound().stream)return false;return !known_||session_.heartbeat(epoch,now_());}
bool Worker::BeginMic(uint32_t epoch,uint16_t stream){
 if(!CurrentSession(epoch,stream)||!codec_)return false;
 if(!codec_->DirectUsbStopOutputChecked(epoch)||!codec_->DirectUsbEnableInputChecked(epoch,true)){Fail(Fault::Direction);Cleanup();return false;}
 if(!Playing(epoch,stream,false)){Fail(Fault::Authority);Cleanup();return false;}return true;
}
bool Worker::BeginSpeaker(uint32_t epoch,uint16_t stream){
 if(!CurrentSession(epoch,stream)||!codec_)return false;
 if(!codec_->DirectUsbStopInputChecked(epoch)||!codec_->DirectUsbEnableOutputChecked(epoch,true)){Fail(Fault::Direction);Cleanup();return false;}
 if(!Playing(epoch,stream,true)){Fail(Fault::Authority);Cleanup();return false;}return true;
}
bool Worker::Playing(uint32_t epoch,uint16_t stream,bool playing){
 std::lock_guard<std::mutex> lock(mutex_);if(!Current()||epoch!=admission_->Bound().epoch||stream!=admission_->Bound().stream)return false;
 if(!playing&&count_)return false;
 if(!known_){
  const auto started=now_();
  if(!session_.begin(epoch,started)||!session_.acknowledge(epoch)||!session_.arm(epoch,true))return false;
  trial_start_=started;trial_deadline_=std::min(admission_->Bound().deadline_ms,started+60000);
 }
 if(!session_.playing(epoch,playing))return false;
 if(playing)drain_pending_=false;
 playing_=playing;known_=true;
 pending_.clear();return true;
}
bool Worker::StopMic(uint32_t epoch,uint16_t stream){
 if(!CurrentSession(epoch,stream)||!codec_)return false;
 if(!codec_->DirectUsbStopInputChecked(admission_->Bound().epoch)){Fail(Fault::Direction);Cleanup();return false;}
 pending_.clear();return true;
}
bool Worker::FinishSpeaker(uint32_t epoch,uint16_t stream){
 if(!Current()||epoch!=admission_->Bound().epoch||stream!=admission_->Bound().stream)return false;
 {std::lock_guard<std::mutex> lock(mutex_);if(count_)return false;}
 // Write acceptance is not drain. Keep playing/mic suppression and ownership
 // until the codec-owner supported DMA fence completes. No timer substitute.
 if(!drain_pending_){if(!codec_->DirectUsbDrainBegin(epoch)){Fail(Fault::Write);Cleanup();return false;}drain_pending_=true;}
 const auto drained=codec_->DirectUsbDrainState(epoch);
 if(drained==AudioCodec::DirectUsbDrain::Pending)return false;
 if(drained!=AudioCodec::DirectUsbDrain::Sent){Fail(Fault::Write);Cleanup();return false;}
 if(!codec_->DirectUsbStopOutputChecked(admission_->Bound().epoch)){Fail(Fault::Direction);Cleanup();return false;}
 return Playing(epoch,stream,false);
}
bool Worker::Receive(uint16_t stream,uint32_t sequence,uint32_t rate,const uint8_t* pcm,size_t bytes){
 std::lock_guard<std::mutex> lock(mutex_);if(!Current())return false;
 if(!active_||!known_||!playing_||stream!=admission_->Bound().stream||rate!=24000||bytes!=3840||!pcm||sequence!=speaker_sequence_||speaker_sequence_==UINT32_MAX||count_==2)return false;
 if(!session_.packet(admission_->Bound().epoch,sequence,bytes,true,now_()))return false;
 std::copy_n(pcm,bytes,queue_[(head_+count_)%2].pcm.data());++speaker_sequence_;++count_;return true;
}
bool Worker::Cleanup(){
 {std::lock_guard<std::mutex> lock(mutex_);active_=false;}
 bool input=true,output=true;if(codec_&&codec_claim_){input=codec_->DirectUsbStopInputChecked(admission_->Bound().epoch);output=codec_->DirectUsbStopOutputChecked(admission_->Bound().epoch);}
 if(!input||!output){std::lock_guard<std::mutex> lock(mutex_);snapshot_.cleanup_failed=true;return false;}
 if(resampler_){esp_ae_rate_cvt_close(resampler_);resampler_=nullptr;}
 if(usb_claim_&&!endpoint_.Close()){std::lock_guard<std::mutex> lock(mutex_);snapshot_.cleanup_failed=true;return false;}
 usb_claim_=false;
 if(codec_claim_&&!codec_->DirectUsbCodecRelease(admission_->Bound().epoch)){std::lock_guard<std::mutex> lock(mutex_);snapshot_.cleanup_failed=true;return false;}
 codec_claim_=false;codec_=nullptr;
 {std::lock_guard<std::mutex> lock(mutex_);admission_.reset();count_=0;session_.mute();pending_.clear();}
 {std::lock_guard<std::mutex> lock(mutex_);snapshot_.cleanup_failed=false;closed_=true;}return true;
}
bool Worker::FlushMic(){
 while(pending_.size()>=320){
  if(!Current()){Fail(Fault::Authority);return false;}
  {std::lock_guard<std::mutex> lock(mutex_);if(playing_){pending_.clear();return true;}}
  if(!endpoint_.MicCapacity(640))return true;
  if(mic_sequence_==UINT32_MAX){Fail(Fault::Packet);return false;}
  std::array<uint8_t,640> pcm{};for(unsigned i=0;i<320;++i){uint16_t value=uint16_t(pending_[i]);pcm[2*i]=uint8_t(value);pcm[2*i+1]=uint8_t(value>>8);}
  {std::lock_guard<std::mutex> lock(mutex_);if(!session_.packet(admission_->Bound().epoch,mic_sequence_,640,false,now_())){pending_.clear();return true;}}
  // Sequence is consumed before send; an unknown/partial send never retries.
  if(!Current()||!endpoint_.Mic(admission_->Bound(),mic_sequence_++,pcm.data(),pcm.size())){Fail(Fault::UsbSend);return false;}
  pending_.erase(pending_.begin(),pending_.begin()+320);
  {std::lock_guard<std::mutex> lock(mutex_);snapshot_.mic_frames+=320;}
 }
 return true;
}
bool Worker::Step(){
 if(Closed())return false;
 if(quit_)return !Cleanup();
 if(!Current()){Fail(Fault::Authority);return !Cleanup();}
 bool playing,known;{std::lock_guard<std::mutex> lock(mutex_);playing=playing_;known=known_;if(known&&!session_.tick(now_()))active_=false;}
 if(!active_){Fail(Fault::Authority);return !Cleanup();}
 if(!known)return true; // No implicit microphone start from arming.
 if(playing){
  if(drain_pending_)return true; // Fence waits must not re-enable/write the channel.
  if(!codec_->DirectUsbStopInputChecked(admission_->Bound().epoch)||!codec_->DirectUsbEnableOutputChecked(admission_->Bound().epoch,true)){Fail(Fault::Direction);return !Cleanup();}
  // Keep the queue entry counted/resident through all four writes. No early
  // credit or queue-slot reuse while its PCM is still being consumed.
  unsigned slot;{std::lock_guard<std::mutex> lock(mutex_);if(!count_)return true;slot=head_;}
  for(unsigned part=0;part<4;++part){if(!Current()){Fail(Fault::Authority);return !Cleanup();}{std::lock_guard<std::mutex> lock(mutex_);if(!playing_||!count_)return true;}
   for(unsigned i=0;i<480;++i){unsigned at=(part*480+i)*2;uint16_t value=uint16_t(queue_[slot].pcm[at])|(uint16_t(queue_[slot].pcm[at+1])<<8);slice_[i]=static_cast<int16_t>(value);}
   if(!codec_->DirectUsbOutputDataChecked(admission_->Bound().epoch,slice_)){Fail(Fault::Write);return !Cleanup();}
  }
  {std::lock_guard<std::mutex> lock(mutex_);head_=(head_+1)%2;--count_;snapshot_.speaker_frames+=1920;}
  if(!Current()||!endpoint_.SpeakerCredit(admission_->Bound(),3840)){Fail(Fault::Credit);return !Cleanup();}return true;
 }
 if(!codec_->DirectUsbStopOutputChecked(admission_->Bound().epoch)){Fail(Fault::Direction);return !Cleanup();}
 if(!FlushMic())return !Cleanup();
 if(pending_.size()>=320||!endpoint_.MicCapacity(640))return true;
 if(!codec_->DirectUsbEnableInputChecked(admission_->Bound().epoch,true)){Fail(Fault::Direction);return !Cleanup();}
 if(!codec_->DirectUsbInputDataChecked(admission_->Bound().epoch,hardware_)){Fail(Fault::Read);return !Cleanup();}
 if(!Current())return true;
 {std::lock_guard<std::mutex> lock(mutex_);if(playing_)return true;}
 for(unsigned i=0;i<480;++i)mono_[i]=hardware_[i*codec_->input_channels()];
 uint32_t bound=0;if(esp_ae_rate_cvt_get_max_out_sample_num(resampler_,480,&bound)!=ESP_AE_ERR_OK||bound>converted_.size()){Fail(Fault::Resampler);return !Cleanup();}
 uint32_t samples=bound;if(esp_ae_rate_cvt_process(resampler_,mono_.data(),480,converted_.data(),&samples)!=ESP_AE_ERR_OK||samples>converted_.size()||pending_.size()+samples>832){Fail(Fault::Resampler);return !Cleanup();}
 pending_.insert(pending_.end(),converted_.begin(),converted_.begin()+samples);
 if(!FlushMic())return !Cleanup();
 return true;
}
}
