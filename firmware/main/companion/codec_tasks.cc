#include "codec_tasks.h"
#include "diagnostics.h"
#include <esp_opus_enc.h>
#include <esp_opus_dec.h>
#include <esp_timer.h>
#include <algorithm>
#include <new>
namespace companion {
template<class T> static void wipe(T& buffer){
    volatile unsigned char* p=reinterpret_cast<volatile unsigned char*>(buffer.data());
    for(std::size_t n=buffer.size()*sizeof(typename T::value_type);n;--n)*p++=0;
}
static uint64_t now_ms(){return esp_timer_get_time()/1000;}
static void codec_fault(diagnostic::FaultCode code,int32_t detail=0,diagnostic::Task caller=diagnostic::Task::Codec){
    diagnostic::Fault(caller,code,detail);diagnostic::Sample(caller);
}
CodecTasks::~CodecTasks(){configASSERT(done_.load());CloseCodecs();if(scratch_){wipe(scratch_->opus);wipe(scratch_->wire);}}
void CodecTasks::WipePending(){wipe(pending_);pending_.clear();}
bool CodecTasks::BindAudioLease(AudioLease lease){
    std::lock_guard<std::mutex> control(control_mutex_);if(stop_admission_)return false;
    if(!done_ || !CommonAudioAuthority().Current(lease) || lease.owner()!=AudioOwner::Mira ||
        (lease.purpose()!=AudioPurpose::Session && lease.purpose()!=AudioPurpose::PhysicalListen))return false;
    lease_=lease;return true;
}
bool CodecTasks::StageBorrowedPhysicalProof(AudioLease lease,uint32_t media,wake_runtime::ExplicitLocalConsent proof){
    std::lock_guard<std::mutex> control(control_mutex_);if(stop_admission_)return false;
    if(!wake_runtime::kLiveBorrowedProcessingAvailable||!borrowed_cpu_||!CommonAudioAuthority().Current(lease)||lease.generation()!=lease_.generation()||lease.epoch()!=lease_.epoch())return false;
    std::lock_guard<std::mutex> lock(mutex_);
    if(media!=epoch_.load()||quit_||!enabled_)return false;
    borrowed_proof_=proof;borrowed_proof_epoch_=media;borrowed_proof_pending_=true;return true;
}
bool CodecTasks::Input(bool enable){
    if(!(enable?CommonAudioAuthority().Current(lease_):CommonAudioAuthority().Holds(lease_)))return false;
    if(codec_->EnableInputChecked(enable))return true;
    codec_fault(enable?diagnostic::FaultCode::DriverInputOpen:diagnostic::FaultCode::DriverInputClose);return false;
}
bool CodecTasks::Output(bool enable){
    if(!(enable?CommonAudioAuthority().Current(lease_):CommonAudioAuthority().Holds(lease_)))return false;
    if(codec_->EnableOutputChecked(enable))return true;
    codec_fault(enable?diagnostic::FaultCode::DriverOutputOpen:diagnostic::FaultCode::DriverOutputClose);return false;
}
bool CodecTasks::CloseDirections(){
    // Attempt both closes, even if the first fails. Neither failure proves silence.
    const bool input=Input(false);const bool output=Output(false);return input&&output;
}
bool CodecTasks::ConfigureLevelObserver(uint64_t generation,LevelObserver observer){
    std::lock_guard<std::mutex> control(control_mutex_);if(stop_admission_)return false;
    if(!done_ || (observer && !generation))return false;
    level_generation_=generation;level_observer_=std::move(observer);return true;
}
void CodecTasks::ObserveLevel(uint32_t epoch,const std::vector<int16_t>& pcm,bool playback){
    if(!level_observer_ || !Current(epoch) || playing_.load()!=playback)return;
    uint16_t peak=0;
    for(auto sample:pcm){const int32_t value=sample;peak=std::max<uint16_t>(peak,static_cast<uint16_t>(value<0?-value:value));}
    if(Current(epoch) && playing_.load()==playback)level_observer_(level_generation_,epoch,peak,playback);
}
bool CodecTasks::OpenCodecs(){
    diagnostic::Stage(diagnostic::Task::Codec,diagnostic::StageCode::CodecOpen);
    CloseCodecs();
    esp_opus_enc_config_t enc={};enc.sample_rate=16000;enc.channel=ESP_AUDIO_MONO;enc.bits_per_sample=ESP_AUDIO_BIT16;
    enc.bitrate=24000;enc.frame_duration=ESP_OPUS_ENC_FRAME_DURATION_60_MS;enc.application_mode=ESP_OPUS_ENC_APPLICATION_VOIP;
    enc.complexity=0;enc.enable_dtx=false;enc.enable_vbr=true;
    auto enc_status=esp_opus_enc_open(&enc,sizeof(enc),&encoder_);
    if(enc_status!=ESP_AUDIO_ERR_OK){codec_fault(diagnostic::FaultCode::EncoderOpen,static_cast<int32_t>(enc_status));return false;}
    esp_opus_dec_cfg_t dec={};dec.sample_rate=24000;dec.channel=ESP_AUDIO_MONO;dec.frame_duration=ESP_OPUS_DEC_FRAME_DURATION_60_MS;
    auto dec_status=esp_opus_dec_open(&dec,sizeof(dec),&decoder_);
    if(dec_status!=ESP_AUDIO_ERR_OK){codec_fault(diagnostic::FaultCode::DecoderOpen,static_cast<int32_t>(dec_status));return false;}
    esp_ae_rate_cvt_cfg_t rate={};rate.src_rate=24000;rate.dest_rate=16000;rate.channel=1;rate.bits_per_sample=16;
    rate.complexity=2;rate.perf_type=ESP_AE_RATE_CVT_PERF_TYPE_SPEED;
    auto rate_status=esp_ae_rate_cvt_open(&rate,&resampler_);
    if(rate_status!=ESP_AE_ERR_OK){codec_fault(diagnostic::FaultCode::ResamplerOpen,static_cast<int32_t>(rate_status));return false;}
    WipePending();pending_.reserve(1440);mic_sequence_=0;return true;
}
void CodecTasks::CloseCodecs(){
    if(encoder_){esp_opus_enc_close(encoder_);encoder_=nullptr;}
    if(decoder_){esp_opus_dec_close(decoder_);decoder_=nullptr;}
    if(resampler_){esp_ae_rate_cvt_close(resampler_);resampler_=nullptr;}
}
bool CodecTasks::Start(AudioCodec* codec,Send send,Stopped stopped){
    std::lock_guard<std::mutex> control(control_mutex_);if(stop_admission_)return false;
    if(!done_ || !CommonAudioAuthority().Current(lease_) || !codec || !send || !stopped || codec->input_enabled() || codec->output_enabled() || codec->input_sample_rate()!=24000 || codec->output_sample_rate()!=24000 || codec->input_channels()<1 || codec->input_channels()>2)return false;
    std::unique_ptr<Scratch> scratch(new(std::nothrow) Scratch);
    if(!scratch)return false; // No task, codec IO or armed state on allocation failure.
    scratch_=std::move(scratch);
    codec_=codec;send_=std::move(send);stopped_=std::move(stopped);quit_=false;enabled_=false;done_=false;
    // Native SILK encoding uses a 9520-byte frame plus dynamic PCM/work arrays
    // and nested analysis calls. Keep headroom beyond the decoder-only budget.
    if(xTaskCreate([](void* arg){auto self=static_cast<CodecTasks*>(arg);self->Run();self->done_.store(true);vTaskDelete(nullptr);},"companion_codec",32768,this,4,&task_)!=pdPASS){done_=true;scratch_.reset();task_=nullptr;return false;}
    return true;
}
bool CodecTasks::ResetStoppedConnection(){
    if(!done_)return false;
    std::lock_guard<std::mutex> lock(mutex_);session_=AudioSession{};incoming_.clear();epoch_=0;enabled_=false;playing_=false;playback_known_=false;return true;
}
bool CodecTasks::Stop(uint32_t timeout_ms){
    {std::lock_guard<std::mutex> control(control_mutex_);stop_admission_=true;enabled_=false;quit_=true;}auto until=now_ms()+std::min<uint32_t>(timeout_ms,5000);
    while(!done_ && now_ms()<until)vTaskDelay(pdMS_TO_TICKS(10));
    std::lock_guard<std::mutex> control(control_mutex_);if(done_)stop_admission_=false;return done_;
}
bool CodecTasks::Begin(uint32_t epoch){
    enabled_=false;std::lock_guard<std::mutex> lock(mutex_);
    if(quit_ || done_ || !CommonAudioAuthority().Current(lease_) || !session_.begin(epoch,now_ms()))return false;
    incoming_.clear();playing_=false;playback_known_=false;epoch_.store(epoch);return true;
}
bool CodecTasks::Acknowledge(uint32_t epoch){std::lock_guard<std::mutex> lock(mutex_);return session_.acknowledge(epoch);}
bool CodecTasks::Consent(uint32_t epoch,bool physical,const std::function<bool()>& permission){std::lock_guard<std::mutex> lock(mutex_);auto result=!quit_ && !done_ && CommonAudioAuthority().Current(lease_) && (!permission||permission())&&session_.arm(epoch,physical);enabled_=result;return result;}
bool CodecTasks::Heartbeat(uint32_t epoch){std::lock_guard<std::mutex> lock(mutex_);auto result=session_.heartbeat(epoch,now_ms());if(!result)enabled_=false;return result;}
bool CodecTasks::Playing(uint32_t epoch,bool playing){std::lock_guard<std::mutex> lock(mutex_);auto result=session_.playing(epoch,playing);if(result){playing_=playing;playback_known_=true;if(!playing)incoming_.clear();}return result;}
void CodecTasks::Interrupt(uint32_t epoch){
    if(epoch!=epoch_.load())return;
    enabled_=false;playing_=false;playback_known_=false;std::lock_guard<std::mutex> lock(mutex_);enabled_=false;session_.interrupt(epoch);incoming_.clear();
}
bool CodecTasks::Current(uint32_t epoch){
    if(!enabled_ || epoch!=epoch_.load() || quit_ || !CommonAudioAuthority().Current(lease_))return false;
    std::lock_guard<std::mutex> lock(mutex_);if(quit_ || !enabled_ || epoch!=epoch_.load() || !CommonAudioAuthority().Current(lease_))return false;auto active=session_.tick(now_ms());if(!active)enabled_=false;return active;
}
bool CodecTasks::Receive(const uint8_t* data,std::size_t bytes){
    WireFrame frame{};if(!decode_wire(data,bytes,frame) || !frame.speaker){codec_fault(diagnostic::FaultCode::PacketRejected,0,diagnostic::Task::AudioTls);return false;}
    // Two 60ms packets remain the memory bound. Backpressure the socket task
    // instead of muting on a transient full queue; never hold mutex_ while waiting.
    constexpr uint64_t queue_wait_ms=120;
    const auto deadline=now_ms()+queue_wait_ms;
    bool waited=false;
    for(;;){
        {
            std::lock_guard<std::mutex> lock(mutex_);
            // Stop/interrupt/new epoch cancels an in-flight wait without touching
            // the replacement session or recording a spurious queue fault.
            if(waited && (quit_ || !enabled_ || frame.epoch!=epoch_.load()))return false;
            if(!enabled_ || quit_){codec_fault(diagnostic::FaultCode::PacketRejected,0,diagnostic::Task::AudioTls);return false;}
            if(now_ms()>=deadline){
                codec_fault(diagnostic::FaultCode::QueueFull,static_cast<int32_t>(incoming_.size()),diagnostic::Task::AudioTls);
                enabled_=false;session_.mute();incoming_.clear();return false;
            }
            if(incoming_.size()<2){
                if(!session_.packet(frame.epoch,frame.sequence,frame.size,true,now_ms())){
                    codec_fault(diagnostic::FaultCode::PacketRejected,0,diagnostic::Task::AudioTls);
                    enabled_=false;session_.mute();incoming_.clear();return false;
                }
                incoming_.push_back({frame.epoch,std::vector<uint8_t>(frame.payload,frame.payload+frame.size)});return true;
            }
        }
        waited=true;
        vTaskDelay(1); // One RTOS tick, so the codec consumer can run.
    }
}
void CodecTasks::Run(){
    diagnostic::Stage(diagnostic::Task::Codec,diagnostic::StageCode::WorkerStart);diagnostic::Sample(diagnostic::Task::Codec);
    uint32_t codec_epoch=0;bool was_active=false;bool failed=false,first_decode=true,first_encode=true;
    std::vector<int16_t> hardware(480*codec_->input_channels()),mono(480),converted(512),playback(1440),slice(480);
    auto& opus=scratch_->opus;auto& wire=scratch_->wire;
    while(!quit_){
        auto epoch=epoch_.load();
        if(!Current(epoch)){
            if(borrowed_cpu_&&borrowed_cpu_armed_){if(!borrowed_cpu_->CloseCpu()){vTaskDelay(pdMS_TO_TICKS(10));continue;}borrowed_cpu_armed_=false;}
            if(!CloseDirections()){failed=true;break;}WipePending();
            if(was_active){diagnostic::Stage(diagnostic::Task::Codec,diagnostic::StageCode::WorkerStop);diagnostic::Sample(diagnostic::Task::Codec);CloseCodecs();codec_epoch=0;stopped_(epoch,false);was_active=false;} // false: physical silence not attested
            vTaskDelay(pdMS_TO_TICKS(10));continue;
        }
        was_active=true;
        // Arming authorizes the session, but does not choose capture vs playback.
        // Await the host's first explicit playing value without touching input.
        if(!playback_known_){
            if(!CloseDirections()){failed=true;break;}WipePending();
            vTaskDelay(pdMS_TO_TICKS(10));continue;
        }
        if(!playing_ && !Output(false)){failed=true;break;}
        if(codec_epoch!=epoch){if(!OpenCodecs()){failed=true;break;}codec_epoch=epoch;}
        Packet packet{};
        {std::lock_guard<std::mutex> lock(mutex_);if(!incoming_.empty()){packet=std::move(incoming_.front());incoming_.pop_front();}}
        if(!packet.payload.empty()){
            if(!Input(false)){failed=true;break;}WipePending();
            if(packet.epoch!=epoch || !playing_)continue;
            esp_audio_dec_in_raw_t raw={};raw.buffer=packet.payload.data();raw.len=packet.payload.size();raw.frame_recover=ESP_AUDIO_DEC_RECOVERY_NONE;
            esp_audio_dec_out_frame_t out={};out.buffer=reinterpret_cast<uint8_t*>(playback.data());out.len=playback.size()*2;
            esp_audio_dec_info_t info={};
            if(first_decode){diagnostic::Stage(diagnostic::Task::Codec,diagnostic::StageCode::FirstDecode);diagnostic::Sample(diagnostic::Task::Codec);first_decode=false;}
            diagnostic::Stage(diagnostic::Task::Codec,diagnostic::StageCode::CodecDecode);
            auto decode_status=esp_opus_dec_decode(decoder_,&raw,&out,&info);
            if(decode_status!=ESP_AUDIO_ERR_OK){codec_fault(diagnostic::FaultCode::DecodeError,static_cast<int32_t>(decode_status));failed=true;break;}
            if(out.decoded_size!=2880){codec_fault(diagnostic::FaultCode::DecodedSize,static_cast<int32_t>(out.decoded_size));failed=true;break;}
            if(raw.consumed!=raw.len){codec_fault(diagnostic::FaultCode::PartialConsumption,static_cast<int32_t>(raw.consumed));failed=true;break;}
            if(!playing_ || !Current(epoch))continue;
            diagnostic::Stage(diagnostic::Task::Codec,diagnostic::StageCode::CodecOutput);
            if(!Current(epoch) || !playing_)continue;
            if(!Output(true)){failed=true;break;}
            for(std::size_t i=0;i<playback.size() && playing_ && Current(epoch);i+=480){
                std::copy_n(playback.data()+i,480,slice.data());
                if(!Current(epoch) || !codec_->OutputDataChecked(slice)){
                    codec_fault(diagnostic::FaultCode::Write);failed=true;break;
                }
                ObserveLevel(epoch,slice,true);
            }
            if(failed)break;
            // Keep the device open through a continuous speaking interval,
            // including bounded gaps between Opus packets. Close on direction
            // change, interruption or worker teardown instead of every packet.
            if(!playing_ && !Output(false)){failed=true;break;}
            esp_ae_rate_cvt_reset(resampler_);continue;
        }
        if(playing_){if(!Input(false)){failed=true;break;}WipePending();vTaskDelay(pdMS_TO_TICKS(10));continue;}
        if(!Current(epoch) || playing_)continue;
        if(!Input(true)){failed=true;break;}
        if(!Current(epoch) || !codec_->InputDataChecked(hardware)){codec_fault(diagnostic::FaultCode::InputRead);failed=true;break;}
        if(!Current(epoch) || playing_)continue;
        for(std::size_t i=0;i<480;++i)mono[i]=hardware[i*codec_->input_channels()];
        ObserveLevel(epoch,mono,false);
        uint32_t size=0;
        auto bound_status=esp_ae_rate_cvt_get_max_out_sample_num(resampler_,480,&size);
        if(bound_status!=ESP_AE_ERR_OK || size>converted.size()){codec_fault(diagnostic::FaultCode::ResamplerBound,bound_status!=ESP_AE_ERR_OK?static_cast<int32_t>(bound_status):static_cast<int32_t>(size));failed=true;break;}
        auto convert_status=esp_ae_rate_cvt_process(resampler_,mono.data(),480,converted.data(),&size);
        if(convert_status!=ESP_AE_ERR_OK || pending_.size()+size>1440){codec_fault(diagnostic::FaultCode::ResamplerProcess,convert_status!=ESP_AE_ERR_OK?static_cast<int32_t>(convert_status):static_cast<int32_t>(size));failed=true;break;}
        if(wake_runtime::kLiveBorrowedProcessingAvailable&&borrowed_cpu_){
            wake_runtime::ExplicitLocalConsent proof;bool staged=false;
            {std::lock_guard<std::mutex> lock(mutex_);if(borrowed_proof_pending_&&borrowed_proof_epoch_==epoch){proof=borrowed_proof_;staged=true;borrowed_proof_pending_=false;}}
            if(staged){if(!borrowed_cpu_->AcceptPhysicalProof(lease_,epoch,proof)){failed=true;break;}borrowed_cpu_armed_=true;}
            if(borrowed_cpu_armed_&&(!Current(epoch)||!borrowed_cpu_->Feed16k(lease_,epoch,converted.data(),size))){failed=true;break;}
        }
        pending_.insert(pending_.end(),converted.begin(),converted.begin()+size);
        if(pending_.size()>=960){
            esp_audio_enc_in_frame_t in={};in.buffer=reinterpret_cast<uint8_t*>(pending_.data());in.len=1920;
            esp_audio_enc_out_frame_t out={};out.buffer=opus.data();out.len=opus.size();
            if(first_encode){diagnostic::Stage(diagnostic::Task::Codec,diagnostic::StageCode::FirstEncode);diagnostic::Sample(diagnostic::Task::Codec);first_encode=false;}
            diagnostic::Stage(diagnostic::Task::Codec,diagnostic::StageCode::CodecEncodeEnter);
            auto encode_status=esp_opus_enc_process(encoder_,&in,&out);
            diagnostic::Stage(diagnostic::Task::Codec,diagnostic::StageCode::CodecEncodeExit);
            if(encode_status!=ESP_AUDIO_ERR_OK || !out.encoded_bytes || out.encoded_bytes>opus.size()){codec_fault(diagnostic::FaultCode::EncodeError,encode_status!=ESP_AUDIO_ERR_OK?static_cast<int32_t>(encode_status):static_cast<int32_t>(out.encoded_bytes));failed=true;break;}
            const auto remaining=pending_.size()-960;
            std::move(pending_.begin()+960,pending_.end(),pending_.begin());
            std::fill(pending_.begin()+remaining,pending_.end(),0);
            pending_.resize(remaining);
            bool allowed=false;{std::lock_guard<std::mutex> lock(mutex_);allowed=session_.packet(epoch,mic_sequence_,out.encoded_bytes,false,now_ms());}
            if(!allowed || !Current(epoch))continue;
            auto bytes=encode_wire({epoch,mic_sequence_++,false,opus.data(),out.encoded_bytes},wire.data(),wire.size());
            if(!bytes || !send_(wire.data(),bytes)){codec_fault(diagnostic::FaultCode::AudioSend);failed=true;break;}
        }
    }
    diagnostic::Stage(diagnostic::Task::Codec,diagnostic::StageCode::WorkerStop);diagnostic::Sample(diagnostic::Task::Codec);
    // A failed operation revokes this worker's authorization. Retain task, object
    // and codec custody through failed closes; Stop timeout never means release.
    enabled_=false;quit_=true;
    {std::lock_guard<std::mutex> lock(mutex_);incoming_.clear();session_.mute();}
    WipePending();wipe(hardware);wipe(mono);wipe(converted);wipe(playback);wipe(slice);
    wipe(opus);wipe(wire);CloseCodecs();
    // CPU failure retains worker/object custody; it cannot attest hardware closure.
    while(borrowed_cpu_&&!borrowed_cpu_->CloseCpu())vTaskDelay(pdMS_TO_TICKS(20));
    while(!CloseDirections())vTaskDelay(pdMS_TO_TICKS(20));
    // Checked close confirms driver closure only; DMA/physical drain is unattested.
    stopped_(epoch_.load(),false);(void)failed;
}
}
