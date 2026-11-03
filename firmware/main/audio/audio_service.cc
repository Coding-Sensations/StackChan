#include "audio_service.h"
#include <esp_log.h>
#include <cstring>
#include <algorithm>

#define RATE_CVT_CFG(_src_rate, _dest_rate, _channel)        \
    (esp_ae_rate_cvt_cfg_t)                                  \
    {                                                        \
        .src_rate        = (uint32_t)(_src_rate),            \
        .dest_rate       = (uint32_t)(_dest_rate),           \
        .channel         = (uint8_t)(_channel),              \
        .bits_per_sample = ESP_AUDIO_BIT16,                  \
        .complexity      = 2,                                \
        .perf_type       = ESP_AE_RATE_CVT_PERF_TYPE_SPEED,  \
    }

#define OPUS_DEC_CFG(_sample_rate, _frame_duration_ms)                                                    \
    (esp_opus_dec_cfg_t)                                                                                  \
    {                                                                                                     \
        .sample_rate    = (uint32_t)(_sample_rate),                                                       \
        .channel        = ESP_AUDIO_MONO,                                                                 \
        .frame_duration = (esp_opus_dec_frame_duration_t)AS_OPUS_GET_FRAME_DRU_ENUM(_frame_duration_ms),  \
        .self_delimited = false,                                                                          \
    }

#if CONFIG_USE_AUDIO_PROCESSOR
#include "processors/afe_audio_processor.h"
#else
#include "processors/no_audio_processor.h"
#endif

#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32P4
#include "wake_words/afe_wake_word.h"
#include "wake_words/custom_wake_word.h"
#else
#include "wake_words/esp_wake_word.h"
#endif

#define TAG "AudioService"
namespace {
constexpr uint32_t kMaxPcmSamples=8192,kMaxOpusBytes=8192;
struct WakeReaderDone{std::atomic<unsigned>& count;~WakeReaderDone(){--count;}};
template<class T>void WipeAudioBuffer(std::vector<T>& data){volatile T* p=data.data();for(auto n=data.size();n;--n)*p++=0;data.clear();}
}

AudioService::AudioService() {
    event_group_ = xEventGroupCreate();
}

companion::AudioLease AudioService::SnapshotLease()const {
    std::lock_guard<std::mutex> lock(lease_mutex_);return lease_;
}
bool AudioService::LeaseCurrent()const {
    return companion::CommonAudioAuthority().Current(SnapshotLease());
}
bool AudioService::BindAudioLease(companion::AudioLease lease){
    if(initialized_ || !checked_stopped_ || active_workers_)return false;
    std::lock_guard<std::mutex> io(hardware_mutex_);
    if(initialized_ || !checked_stopped_ || active_workers_ || !companion::CommonAudioAuthority().Current(lease) ||
       (lease.owner()!=companion::AudioOwner::Stock && lease.owner()!=companion::AudioOwner::Native))return false;
    {std::lock_guard<std::mutex> lock(lease_mutex_);lease_=lease;}return true;
}
void AudioService::RequestStop(){
    std::lock_guard<std::mutex> lock(audio_queue_mutex_);
    service_stopped_=true;
    xEventGroupSetBits(event_group_,AS_EVENT_AUDIO_TESTING_RUNNING|AS_EVENT_WAKE_WORD_RUNNING|AS_EVENT_AUDIO_PROCESSOR_RUNNING);
    audio_queue_cv_.notify_all();
}
void AudioService::CloseSoftwareCodecs(){
    if(opus_encoder_){esp_opus_enc_close(opus_encoder_);opus_encoder_=nullptr;}
    if(opus_decoder_){esp_opus_dec_close(opus_decoder_);opus_decoder_=nullptr;}
    if(input_resampler_){esp_ae_rate_cvt_close(input_resampler_);input_resampler_=nullptr;}
    if(output_resampler_){esp_ae_rate_cvt_close(output_resampler_);output_resampler_=nullptr;}
}
AudioService::~AudioService() {
    configASSERT(checked_stopped_ && active_workers_.load()==0 && wake_readers_.load()==0);
    configASSERT(audio_power_timer_==nullptr && audio_shutdown_timer_==nullptr);
    if (event_group_ != nullptr) {
        vEventGroupDelete(event_group_);
    }
    if (opus_encoder_ != nullptr) {
        esp_opus_enc_close(opus_encoder_);
    }
    if (opus_decoder_ != nullptr) {
        esp_opus_dec_close(opus_decoder_);
    }
    if (input_resampler_ != nullptr) {
        esp_ae_rate_cvt_close(input_resampler_);
    }
    if (output_resampler_ != nullptr) {
        esp_ae_rate_cvt_close(output_resampler_);
    }
}

bool AudioService::Initialize(AudioCodec* codec) {
    std::lock_guard<std::mutex> io(hardware_mutex_);
    if(!LeaseCurrent() || !codec || initialized_ || !checked_stopped_ || active_workers_ ||
       codec->input_enabled() || codec->output_enabled() || codec->input_channels()<1 || codec->input_channels()>2 ||
       codec->input_sample_rate()<8000 || codec->input_sample_rate()>48000 || codec->output_sample_rate()<8000 || codec->output_sample_rate()>48000)return false;
    checked_stopped_=false;codec_ = codec;
    {std::lock_guard<std::mutex> children(child_mutex_);
        audio_processor_bound_=false;wake_word_bound_=false;
        audio_processor_initialized_=false;wake_word_initialized_=false;
    }
    // Start performs codec configuration; this call also requires captured custody.
    codec_->Start();
    auto fail=[this](){CloseSoftwareCodecs();return false;};

    esp_opus_dec_cfg_t opus_dec_cfg = OPUS_DEC_CFG(codec->output_sample_rate(), OPUS_FRAME_DURATION_MS);
    auto ret = esp_opus_dec_open(&opus_dec_cfg, sizeof(esp_opus_dec_cfg_t), &opus_decoder_);
    if (ret!=ESP_AUDIO_ERR_OK || opus_decoder_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create audio decoder, error code: %d", ret);
        return fail();
    } else {
        decoder_sample_rate_ = codec->output_sample_rate();
        decoder_duration_ms_ = OPUS_FRAME_DURATION_MS;
        decoder_frame_size_ = decoder_sample_rate_ / 1000 * OPUS_FRAME_DURATION_MS;
    }
    esp_opus_enc_config_t opus_enc_cfg = AS_OPUS_ENC_CONFIG();
    ret = esp_opus_enc_open(&opus_enc_cfg, sizeof(esp_opus_enc_config_t), &opus_encoder_);
    if (ret!=ESP_AUDIO_ERR_OK || opus_encoder_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create audio encoder, error code: %d", ret);
        return fail();
    } else {
        encoder_sample_rate_ = 16000;
        encoder_duration_ms_ = OPUS_FRAME_DURATION_MS;
        if(esp_opus_enc_get_frame_size(opus_encoder_, &encoder_frame_size_, &encoder_outbuf_size_)!=ESP_AUDIO_ERR_OK || encoder_frame_size_<=0 || encoder_frame_size_>int(kMaxPcmSamples*2) || encoder_frame_size_%2 || encoder_outbuf_size_<=0 || encoder_outbuf_size_>int(kMaxOpusBytes))return fail();
        encoder_frame_size_ = encoder_frame_size_ / sizeof(int16_t);
    }

    if (codec->input_sample_rate() != 16000) {
        esp_ae_rate_cvt_cfg_t input_resampler_cfg = RATE_CVT_CFG(
            codec->input_sample_rate(), ESP_AUDIO_SAMPLE_RATE_16K, codec->input_channels());
        auto resampler_ret = esp_ae_rate_cvt_open(&input_resampler_cfg, &input_resampler_);
        if (resampler_ret!=ESP_AE_ERR_OK || input_resampler_ == nullptr) {
            ESP_LOGE(TAG, "Failed to create input resampler, error code: %d", resampler_ret);
            return fail();
        }
    }

#if CONFIG_USE_AUDIO_PROCESSOR
    audio_processor_ = std::make_unique<AfeAudioProcessor>();
#else
    audio_processor_ = std::make_unique<NoAudioProcessor>();
#endif

    audio_processor_->OnOutput([this, lease=SnapshotLease()](std::vector<int16_t>&& data) {
        if(!service_stopped_ && companion::CommonAudioAuthority().Current(lease))PushTaskToEncodeQueue(kAudioTaskTypeEncodeToSendQueue, std::move(data));
    });

    audio_processor_->OnVadStateChange([this, lease=SnapshotLease()](bool speaking) {
        if(service_stopped_ || !companion::CommonAudioAuthority().Current(lease))return;
        voice_detected_ = speaking;
        if (callbacks_.on_vad_change) {
            callbacks_.on_vad_change(speaking);
        }
    });

    esp_timer_create_args_t audio_power_timer_args = {
        .callback = [](void* arg) {
            AudioService* audio_service = (AudioService*)arg;
            audio_service->CheckAndUpdateAudioPowerState();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "audio_power_timer",
        .skip_unhandled_events = true,
    };
    if(esp_timer_create(&audio_power_timer_args, &audio_power_timer_)!=ESP_OK)return fail();
    // A one-shot on the same ESP_TIMER_TASK forms a dispatch completion barrier.
    // Its final store is the last callback access to this service.
    esp_timer_create_args_t shutdown_args={};
    shutdown_args.callback=[](void* arg){static_cast<AudioService*>(arg)->timer_barrier_done_.store(true);};
    shutdown_args.arg=this;shutdown_args.dispatch_method=ESP_TIMER_TASK;shutdown_args.name="audio_shutdown_barrier";
    if(esp_timer_create(&shutdown_args,&audio_shutdown_timer_)!=ESP_OK)return fail();
    timer_barrier_done_=false;timer_started_=false;timer_barrier_requested_=false;
    // A previous service instance was fully joined before reinitialization.
    // Rebuild the detector callback with this initialization's captured token.
    if(models_list_)SetModelsList(models_list_);
    initialized_=true;return true;
}

bool AudioService::Start() {
    if(!LeaseCurrent() || !initialized_ || !service_stopped_ || active_workers_)return false;
    service_stopped_ = false;
    xEventGroupClearBits(event_group_, AS_EVENT_AUDIO_TESTING_RUNNING | AS_EVENT_WAKE_WORD_RUNNING | AS_EVENT_AUDIO_PROCESSOR_RUNNING);

    if(esp_timer_start_periodic(audio_power_timer_, 1000000)!=ESP_OK){RequestStop();return false;}
    timer_started_=true;

#if CONFIG_USE_AUDIO_PROCESSOR
    /* Start the audio input task */
    ++active_workers_;
    if(xTaskCreatePinnedToCore([](void* arg) {
        AudioService* audio_service = (AudioService*)arg;
        audio_service->AudioInputTask();
        --audio_service->active_workers_;
        vTaskDelete(NULL);
    }, "audio_input", 2048 * 3, this, 8, &audio_input_task_handle_, 0)!=pdPASS){--active_workers_;RequestStop();return false;}

    /* Start the audio output task */
    ++active_workers_;
    if(xTaskCreate([](void* arg) {
        AudioService* audio_service = (AudioService*)arg;
        audio_service->AudioOutputTask();
        --audio_service->active_workers_;
        vTaskDelete(NULL);
    }, "audio_output", 2048 * 2, this, 4, &audio_output_task_handle_)!=pdPASS){--active_workers_;RequestStop();return false;}
#else
    /* Start the audio input task */
    ++active_workers_;
    if(xTaskCreate([](void* arg) {
        AudioService* audio_service = (AudioService*)arg;
        audio_service->AudioInputTask();
        --audio_service->active_workers_;
        vTaskDelete(NULL);
    }, "audio_input", 2048 * 2, this, 8, &audio_input_task_handle_)!=pdPASS){--active_workers_;RequestStop();return false;}

    /* Start the audio output task */
    ++active_workers_;
    if(xTaskCreate([](void* arg) {
        AudioService* audio_service = (AudioService*)arg;
        audio_service->AudioOutputTask();
        --audio_service->active_workers_;
        vTaskDelete(NULL);
    }, "audio_output", 2048, this, 4, &audio_output_task_handle_)!=pdPASS){--active_workers_;RequestStop();return false;}
#endif

    /* Start the opus codec task */
    ++active_workers_;
    if(xTaskCreate([](void* arg) {
        AudioService* audio_service = (AudioService*)arg;
        audio_service->OpusCodecTask();
        --audio_service->active_workers_;
        vTaskDelete(NULL);
    }, "opus_codec", 2048 * 12, this, 2, &opus_codec_task_handle_)!=pdPASS){--active_workers_;RequestStop();return false;}
    return true;
}

bool AudioService::Stop(uint32_t timeout_ms) {
    RequestStop();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(std::min<uint32_t>(timeout_ms,5000));
    const auto remaining=[&](){const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();return ms>0?static_cast<uint32_t>(ms):0u;};
    {
        std::unique_lock<std::mutex> children(child_mutex_,std::defer_lock);
        while(!children.try_lock()){
            if(std::chrono::steady_clock::now()>=deadline)return false;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        // Both bound children receive cancellation even if the first join fails.
        const bool processor_joined=!audio_processor_bound_ || audio_processor_->StopChecked(remaining());
        const bool wake_joined=!wake_word_bound_ || wake_word_->StopChecked(remaining());
        if(!processor_joined || !wake_joined)return false;
    }
    while((active_workers_ || wake_readers_) && std::chrono::steady_clock::now()<deadline)vTaskDelay(pdMS_TO_TICKS(1));
    if(active_workers_ || wake_readers_)return false;
    std::unique_lock<std::mutex> io(hardware_mutex_,std::defer_lock);
    while(!io.try_lock()){
        if(std::chrono::steady_clock::now()>=deadline)return false;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if(checked_stopped_)return true;
    if(!companion::CommonAudioAuthority().Holds(SnapshotLease()))return false;
    bool timer_closed=true;
    if(audio_power_timer_){
        const auto stopped=esp_timer_stop(audio_power_timer_);
        timer_closed=stopped==ESP_OK || stopped==ESP_ERR_INVALID_STATE;
    }
    // Both directions are attempted; a failed close preserves custody for retry.
    const bool input_closed=codec_ && codec_->EnableInputChecked(false);
    const bool output_closed=codec_ && codec_->EnableOutputChecked(false);
    if(!timer_closed || !input_closed || !output_closed)return false;
    if(timer_started_ && !timer_barrier_requested_){
        if(!audio_shutdown_timer_)return false;
        timer_barrier_done_=false;
        if(esp_timer_start_once(audio_shutdown_timer_,1)!=ESP_OK)return false;
        timer_barrier_requested_=true;
    }
    // ESP-IDF stops/removes the periodic timer under its dispatch list lock.
    // A callback already selected before stop runs before this one-shot on the
    // same ESP_TIMER_TASK. Never hold the IO mutex while waiting for that receipt.
    io.unlock();
    while(timer_started_ && !timer_barrier_done_ && std::chrono::steady_clock::now()<deadline)vTaskDelay(pdMS_TO_TICKS(1));
    if(timer_started_ && !timer_barrier_done_)return false;
    while(!io.try_lock()){
        if(std::chrono::steady_clock::now()>=deadline)return false;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if(audio_power_timer_){
        if(esp_timer_delete(audio_power_timer_)!=ESP_OK)return false;
        audio_power_timer_=nullptr;
    }
    if(audio_shutdown_timer_){
        if(esp_timer_delete(audio_shutdown_timer_)!=ESP_OK)return false;
        audio_shutdown_timer_=nullptr;
    }
    {std::lock_guard<std::mutex> lock(audio_queue_mutex_);
        for(auto& p:audio_decode_queue_)WipeAudioBuffer(p->payload);
        for(auto& p:audio_testing_queue_)WipeAudioBuffer(p->payload);
        for(auto& p:audio_send_queue_)WipeAudioBuffer(p->payload);
        audio_encode_queue_.clear();audio_decode_queue_.clear();audio_playback_queue_.clear();
        audio_testing_queue_.clear();audio_send_queue_.clear();timestamp_queue_.clear();}
    CloseSoftwareCodecs();initialized_=false;checked_stopped_=true;return true;
}

bool AudioService::ReadAudioData(std::vector<int16_t>& data, int sample_rate, int samples) {
    std::lock_guard<std::mutex> io(hardware_mutex_);
    if(service_stopped_ || !LeaseCurrent() || !initialized_ || sample_rate<8000 || sample_rate>48000 || samples<=0 || samples>4096)return false;
    if(sample_rate!=16000 && sample_rate!=codec_->input_sample_rate())return false;
    const auto channels=codec_->input_channels();
    if(channels<1 || channels>2 || codec_->input_sample_rate()<8000 || codec_->input_sample_rate()>48000)return false;
    const uint64_t hardware_samples=uint64_t(samples)*codec_->input_sample_rate()/sample_rate*channels;
    if(!hardware_samples || hardware_samples>kMaxPcmSamples)return false;
    if (!codec_->input_enabled()) {
        esp_timer_stop(audio_power_timer_);
        if(esp_timer_start_periodic(audio_power_timer_, AUDIO_POWER_CHECK_INTERVAL_MS * 1000)!=ESP_OK){RequestStop();return false;}
        if(!LeaseCurrent() || !codec_->EnableInputChecked(true)){RequestStop();return false;}
    }

    if (codec_->input_sample_rate() != sample_rate) {
        data.resize(static_cast<std::size_t>(hardware_samples));
        if (!LeaseCurrent() || !codec_->InputDataChecked(data)) {
            std::fill(data.begin(),data.end(),0);data.clear();RequestStop();return false;
        }
        if (input_resampler_ != nullptr) {
            std::lock_guard<std::mutex> lock(input_resampler_mutex_);
            uint32_t in_sample_num = data.size() / codec_->input_channels();
            uint32_t output_samples = 0;
            if(esp_ae_rate_cvt_get_max_out_sample_num(input_resampler_, in_sample_num, &output_samples)!=ESP_AE_ERR_OK || !output_samples || output_samples>kMaxPcmSamples/channels){WipeAudioBuffer(data);RequestStop();return false;}
            auto resampled = std::vector<int16_t>(output_samples * codec_->input_channels());
            uint32_t actual_output = output_samples;
            const auto status=esp_ae_rate_cvt_process(input_resampler_, (esp_ae_sample_t)data.data(), in_sample_num,
                                   (esp_ae_sample_t)resampled.data(), &actual_output);
            if(status!=ESP_AE_ERR_OK || !actual_output || actual_output>output_samples){WipeAudioBuffer(resampled);WipeAudioBuffer(data);RequestStop();return false;}
            std::fill(resampled.begin()+actual_output*channels,resampled.end(),0);
            resampled.resize(actual_output * channels);WipeAudioBuffer(data);
            data = std::move(resampled);
        }else{WipeAudioBuffer(data);RequestStop();return false;}
    } else {
        data.resize(static_cast<std::size_t>(hardware_samples));
        if (!LeaseCurrent() || !codec_->InputDataChecked(data)) {
            std::fill(data.begin(),data.end(),0);data.clear();RequestStop();return false;
        }
    }

    if(service_stopped_ || !LeaseCurrent()){std::fill(data.begin(),data.end(),0);data.clear();return false;}
    /* Update the last input time */
    last_input_time_ = std::chrono::steady_clock::now();
    debug_statistics_.input_count++;

#if CONFIG_USE_AUDIO_DEBUGGER
    // 音频调试：发送原始音频数据
    if (audio_debugger_ == nullptr) {
        audio_debugger_ = std::make_unique<AudioDebugger>();
    }
    audio_debugger_->Feed(data);
#endif

    return true;
}

void AudioService::AudioInputTask() {
    while (true) {
        EventBits_t bits = xEventGroupWaitBits(event_group_, AS_EVENT_AUDIO_TESTING_RUNNING |
            AS_EVENT_WAKE_WORD_RUNNING | AS_EVENT_AUDIO_PROCESSOR_RUNNING,
            pdFALSE, pdFALSE, portMAX_DELAY);

        if (service_stopped_ || !LeaseCurrent()) {
            break;
        }
        bits&=xEventGroupGetBits(event_group_);
        if(!bits)continue;
        if (audio_input_need_warmup_) {
            audio_input_need_warmup_ = false;
            vTaskDelay(pdMS_TO_TICKS(120));
            continue;
        }

        /* Used for audio testing in NetworkConfiguring mode by clicking the BOOT button */
        if (bits & AS_EVENT_AUDIO_TESTING_RUNNING) {
            if (audio_testing_queue_.size() >= AUDIO_TESTING_MAX_DURATION_MS / OPUS_FRAME_DURATION_MS) {
                ESP_LOGW(TAG, "Audio testing queue is full, stopping audio testing");
                EnableAudioTesting(false);
                continue;
            }
            std::vector<int16_t> data;
            int samples = OPUS_FRAME_DURATION_MS * 16000 / 1000;
            if (ReadAudioData(data, 16000, samples)) {
                // If input channels is 2, we need to fetch the left channel data
                if (codec_->input_channels() == 2) {
                    auto mono_data = std::vector<int16_t>(data.size() / 2);
                    for (size_t i = 0, j = 0; i < mono_data.size(); ++i, j += 2) {
                        mono_data[i] = data[j];
                    }
                    data = std::move(mono_data);
                }
                PushTaskToEncodeQueue(kAudioTaskTypeEncodeToTestingQueue, std::move(data));
                continue;
            }
        }

        /* Feed the wake word and/or audio processor */
        if (bits & (AS_EVENT_WAKE_WORD_RUNNING | AS_EVENT_AUDIO_PROCESSOR_RUNNING)) {
            int samples = 160; // 10ms
            std::vector<int16_t> data;
            if (ReadAudioData(data, 16000, samples)) {
                std::lock_guard<std::mutex> children(child_mutex_);
                const auto current_bits=xEventGroupGetBits(event_group_);
                if(service_stopped_ || !LeaseCurrent()){WipeAudioBuffer(data);continue;}
                if ((bits & current_bits & AS_EVENT_WAKE_WORD_RUNNING) && wake_word_bound_)wake_word_->Feed(data);
                if ((bits & current_bits & AS_EVENT_AUDIO_PROCESSOR_RUNNING) && audio_processor_bound_)audio_processor_->Feed(std::move(data));
                else WipeAudioBuffer(data);
                continue;
            }
        }

        // Read timeout/error should not terminate the input task.
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGW(TAG, "Audio input task stopped");
}

void AudioService::AudioOutputTask() {
    while (true) {
        std::unique_lock<std::mutex> lock(audio_queue_mutex_);
        audio_queue_cv_.wait(lock, [this]() { return !audio_playback_queue_.empty() || service_stopped_; });
        if (service_stopped_ || !LeaseCurrent()) {
            break;
        }

        auto task = std::move(audio_playback_queue_.front());
        audio_playback_queue_.pop_front();
        audio_queue_cv_.notify_all();
        lock.unlock();

        std::lock_guard<std::mutex> io(hardware_mutex_);
        if(service_stopped_ || !LeaseCurrent())continue;
        if (!codec_->output_enabled()) {
            esp_timer_stop(audio_power_timer_);
            if(esp_timer_start_periodic(audio_power_timer_, AUDIO_POWER_CHECK_INTERVAL_MS * 1000)!=ESP_OK ||
               !LeaseCurrent() || !codec_->EnableOutputChecked(true)){RequestStop();continue;}
        }
        if(!LeaseCurrent() || !codec_->OutputDataChecked(task->pcm)){RequestStop();continue;}
        if(service_stopped_ || !LeaseCurrent())continue;

        /* Update the last output time */
        last_output_time_ = std::chrono::steady_clock::now();
        debug_statistics_.playback_count++;

#if CONFIG_USE_SERVER_AEC
        /* Record the timestamp for server AEC */
        if (task->timestamp > 0) {
            lock.lock();
            timestamp_queue_.push_back(task->timestamp);
        }
#endif
    }

    ESP_LOGW(TAG, "Audio output task stopped");
}

void AudioService::OpusCodecTask() {
    while (true) {
        std::unique_lock<std::mutex> lock(audio_queue_mutex_);
        audio_queue_cv_.wait(lock, [this]() {
            return service_stopped_ ||
                (!audio_encode_queue_.empty() && audio_send_queue_.size() < MAX_SEND_PACKETS_IN_QUEUE) ||
                (!audio_decode_queue_.empty() && audio_playback_queue_.size() < MAX_PLAYBACK_TASKS_IN_QUEUE);
        });
        if (service_stopped_ || !LeaseCurrent()) {
            break;
        }

        /* Decode the audio from decode queue */
        if (!audio_decode_queue_.empty() && audio_playback_queue_.size() < MAX_PLAYBACK_TASKS_IN_QUEUE) {
            auto packet = std::move(audio_decode_queue_.front());
            audio_decode_queue_.pop_front();
            audio_queue_cv_.notify_all();
            lock.unlock();

            auto task = std::make_unique<AudioTask>();
            task->type = kAudioTaskTypeDecodeToPlaybackQueue;
            task->timestamp = packet->timestamp;

            SetDecodeSampleRate(packet->sample_rate, packet->frame_duration);
            if(service_stopped_ || !LeaseCurrent())break;
            if (opus_decoder_ != nullptr) {
                task->pcm.resize(decoder_frame_size_);
                esp_audio_dec_in_raw_t raw = {
                    .buffer = (uint8_t *)(packet->payload.data()),
                    .len = (uint32_t)(packet->payload.size()),
                    .consumed = 0,
                    .frame_recover = ESP_AUDIO_DEC_RECOVERY_NONE,
                };
                esp_audio_dec_out_frame_t out_frame = {
                    .buffer = (uint8_t *)(task->pcm.data()),
                    .len = (uint32_t)(task->pcm.size() * sizeof(int16_t)),
                    .decoded_size = 0,
                };
                esp_audio_dec_info_t dec_info = {};
                std::unique_lock<std::mutex> decoder_lock(decoder_mutex_);
                auto ret = esp_opus_dec_decode(opus_decoder_, &raw, &out_frame, &dec_info);
                decoder_lock.unlock();
                if (ret == ESP_AUDIO_ERR_OK && out_frame.decoded_size && out_frame.decoded_size<=out_frame.len && out_frame.decoded_size%sizeof(int16_t)==0 && raw.consumed==raw.len) {
                    std::fill(task->pcm.begin()+out_frame.decoded_size/sizeof(int16_t),task->pcm.end(),0);
                    task->pcm.resize(out_frame.decoded_size / sizeof(int16_t));
                    if (decoder_sample_rate_ != codec_->output_sample_rate() && output_resampler_ != nullptr) {
                        uint32_t target_size = 0;
                        if(esp_ae_rate_cvt_get_max_out_sample_num(output_resampler_, task->pcm.size(), &target_size)!=ESP_AE_ERR_OK || !target_size || target_size>kMaxPcmSamples){RequestStop();break;}
                        std::vector<int16_t> resampled(target_size);
                        uint32_t actual_output = target_size;
                        const auto status=esp_ae_rate_cvt_process(output_resampler_, (esp_ae_sample_t)task->pcm.data(), task->pcm.size(),
                                                (esp_ae_sample_t)resampled.data(), &actual_output);
                        if(status!=ESP_AE_ERR_OK || !actual_output || actual_output>target_size){WipeAudioBuffer(resampled);RequestStop();break;}
                        std::fill(resampled.begin()+actual_output,resampled.end(),0);
                        resampled.resize(actual_output);WipeAudioBuffer(task->pcm);
                        task->pcm = std::move(resampled);
                    }
                    lock.lock();
                    audio_playback_queue_.push_back(std::move(task));
                    audio_queue_cv_.notify_all();
                    debug_statistics_.decode_count++;
                } else {
                    ESP_LOGE(TAG, "Failed to decode audio after resize, error code: %d", ret);
                    RequestStop();break;
                }
            } else {
                ESP_LOGE(TAG, "Audio decoder is not configured");
                lock.lock();
            }
            debug_statistics_.decode_count++;
        }
        /* Encode the audio to send queue */
        if (!audio_encode_queue_.empty() && audio_send_queue_.size() < MAX_SEND_PACKETS_IN_QUEUE) {
            auto task = std::move(audio_encode_queue_.front());
            audio_encode_queue_.pop_front();
            audio_queue_cv_.notify_all();
            lock.unlock();

            auto packet = std::make_unique<AudioStreamPacket>();
            packet->frame_duration = OPUS_FRAME_DURATION_MS;
            packet->sample_rate = 16000;
            packet->timestamp = task->timestamp;

            if (opus_encoder_ != nullptr && task->pcm.size() == encoder_frame_size_) {
                std::vector<uint8_t> buf(encoder_outbuf_size_);
                esp_audio_enc_in_frame_t in = {
                    .buffer = (uint8_t *)(task->pcm.data()),
                    .len = (uint32_t)(encoder_frame_size_ * sizeof(int16_t)),
                };
                esp_audio_enc_out_frame_t out = {
                    .buffer = buf.data(),
                    .len = (uint32_t)encoder_outbuf_size_,
                    .encoded_bytes = 0,
                };
                auto ret = esp_opus_enc_process(opus_encoder_, &in, &out);
                if (ret == ESP_AUDIO_ERR_OK && out.encoded_bytes && out.encoded_bytes<=buf.size()) {
                    packet->payload.assign(buf.data(), buf.data() + out.encoded_bytes);WipeAudioBuffer(buf);

                    if (task->type == kAudioTaskTypeEncodeToSendQueue) {
                        {
                            std::lock_guard<std::mutex> lock2(audio_queue_mutex_);
                            audio_send_queue_.push_back(std::move(packet));
                        }
                        if (callbacks_.on_send_queue_available) {
                            callbacks_.on_send_queue_available();
                        }
                    } else if (task->type == kAudioTaskTypeEncodeToTestingQueue) {
                        std::lock_guard<std::mutex> lock2(audio_queue_mutex_);
                        audio_testing_queue_.push_back(std::move(packet));
                    }
                    debug_statistics_.encode_count++;
                } else {
                    ESP_LOGE(TAG, "Failed to encode audio, error code: %d", ret);WipeAudioBuffer(buf);RequestStop();break;
                }
            } else {
                ESP_LOGE(TAG, "Failed to encode audio: encoder not configured or invalid frame size (got %u, expected %u)",
                         task->pcm.size(), encoder_frame_size_);
            }
            lock.lock();
        }
    }

    ESP_LOGW(TAG, "Opus codec task stopped");
}

void AudioService::SetDecodeSampleRate(int sample_rate, int frame_duration) {
    if(service_stopped_ || !LeaseCurrent())return;
    if(sample_rate<8000 || sample_rate>48000 || frame_duration<=0 || frame_duration>120 ||
       AS_OPUS_GET_FRAME_DRU_ENUM(frame_duration)<0 || uint64_t(sample_rate)*frame_duration/1000>kMaxPcmSamples){RequestStop();return;}
    bool failed=false;
    {
        // Keep construction and publication atomic with ResetDecoder. Never
        // acquire the queue mutex through RequestStop while holding this lock.
        std::unique_lock<std::mutex> decoder_lock(decoder_mutex_);
        if(decoder_sample_rate_==sample_rate && decoder_duration_ms_==frame_duration)return;
        void* replacement=nullptr;esp_ae_rate_cvt_handle_t replacement_rate=nullptr;
        esp_opus_dec_cfg_t cfg=OPUS_DEC_CFG(sample_rate,frame_duration);
        failed=esp_opus_dec_open(&cfg,sizeof(cfg),&replacement)!=ESP_AUDIO_ERR_OK || !replacement;
        if(!failed && sample_rate!=codec_->output_sample_rate()){
            esp_ae_rate_cvt_cfg_t rate=RATE_CVT_CFG(sample_rate,codec_->output_sample_rate(),ESP_AUDIO_MONO);
            failed=esp_ae_rate_cvt_open(&rate,&replacement_rate)!=ESP_AE_ERR_OK || !replacement_rate;
        }
        if(!failed){
            if(opus_decoder_)esp_opus_dec_close(opus_decoder_);
            if(output_resampler_)esp_ae_rate_cvt_close(output_resampler_);
            opus_decoder_=replacement;output_resampler_=replacement_rate;
            decoder_sample_rate_=sample_rate;decoder_duration_ms_=frame_duration;
            decoder_frame_size_=sample_rate/1000*frame_duration;
        }else{
            if(replacement)esp_opus_dec_close(replacement);
            if(replacement_rate)esp_ae_rate_cvt_close(replacement_rate);
        }
    }
    if(failed)RequestStop();
}

void AudioService::PushTaskToEncodeQueue(AudioTaskType type, std::vector<int16_t>&& pcm) {
    if(service_stopped_ || !LeaseCurrent() || pcm.empty() || pcm.size()>kMaxPcmSamples)return;
    auto task = std::make_unique<AudioTask>();
    task->type = type;
    task->pcm = std::move(pcm);
    /* Push the task to the encode queue */
    std::unique_lock<std::mutex> lock(audio_queue_mutex_);

    /* If the task is to send queue, we need to set the timestamp */
    if (type == kAudioTaskTypeEncodeToSendQueue && !timestamp_queue_.empty()) {
        if (timestamp_queue_.size() <= MAX_TIMESTAMPS_IN_QUEUE) {
            task->timestamp = timestamp_queue_.front();
        } else {
            ESP_LOGW(TAG, "Timestamp queue (%u) is full, dropping timestamp", timestamp_queue_.size());
        }
        timestamp_queue_.pop_front();
    }

    audio_queue_cv_.wait(lock, [this]() { return service_stopped_ || !LeaseCurrent() || audio_encode_queue_.size() < MAX_ENCODE_TASKS_IN_QUEUE; });
    if(service_stopped_ || !LeaseCurrent())return;
    audio_encode_queue_.push_back(std::move(task));
    audio_queue_cv_.notify_all();
}

bool AudioService::PushPacketToDecodeQueue(std::unique_ptr<AudioStreamPacket> packet, bool wait) {
    if(service_stopped_ || !LeaseCurrent() || !packet || packet->payload.empty() || packet->payload.size()>kMaxOpusBytes)return false;
    std::unique_lock<std::mutex> lock(audio_queue_mutex_);
    if (audio_decode_queue_.size() >= MAX_DECODE_PACKETS_IN_QUEUE) {
        if (wait) {
            audio_queue_cv_.wait(lock, [this]() { return service_stopped_ || !LeaseCurrent() || audio_decode_queue_.size() < MAX_DECODE_PACKETS_IN_QUEUE; });
        } else {
            return false;
        }
    }
    if(service_stopped_ || !LeaseCurrent())return false;
    audio_decode_queue_.push_back(std::move(packet));
    audio_queue_cv_.notify_all();
    return true;
}

std::unique_ptr<AudioStreamPacket> AudioService::PopPacketFromSendQueue() {
    if(service_stopped_ || !LeaseCurrent())return nullptr;
    std::lock_guard<std::mutex> lock(audio_queue_mutex_);
    if (audio_send_queue_.empty()) {
        return nullptr;
    }
    auto packet = std::move(audio_send_queue_.front());
    audio_send_queue_.pop_front();
    audio_queue_cv_.notify_all();
    return packet;
}

bool AudioService::PrepareProcessorLocked(){
    if(service_stopped_ || !LeaseCurrent() || !audio_processor_ || !audio_processor_->SupportsCheckedLifecycle())return false;
    if(!audio_processor_bound_){
        if(!audio_processor_->BindAudioLease(SnapshotLease()))return false;
        audio_processor_bound_=true; // Partial initialization still needs a join.
    }
    if(!audio_processor_initialized_){
        if(!audio_processor_->InitializeChecked(codec_,OPUS_FRAME_DURATION_MS,models_list_))return false;
        audio_processor_initialized_=true;
    }
    return !service_stopped_ && LeaseCurrent();
}
bool AudioService::PrepareWakeWordLocked(){
    if(service_stopped_ || !LeaseCurrent() || !wake_word_ || !wake_word_->SupportsCheckedLifecycle())return false;
    if(!wake_word_bound_){
        if(!wake_word_->BindAudioLease(SnapshotLease()))return false;
        wake_word_bound_=true;
    }
    if(!wake_word_initialized_){
        if(!wake_word_->Initialize(codec_,models_list_))return false;
        wake_word_initialized_=true;
    }
    return !service_stopped_ && LeaseCurrent();
}
void AudioService::EncodeWakeWord(){
    std::lock_guard<std::mutex> children(child_mutex_);
    if(service_stopped_ || !LeaseCurrent() || !wake_word_bound_)return;
    if(!wake_word_->EncodeWakeWordDataChecked())RequestStop();
}
std::string AudioService::GetLastWakeWord()const{
    std::lock_guard<std::mutex> children(child_mutex_);
    if(service_stopped_ || !LeaseCurrent() || !wake_word_bound_)return {};
    return wake_word_->GetLastDetectedWakeWord();
}
std::unique_ptr<AudioStreamPacket> AudioService::PopWakeWordPacket(){
    WakeWord* captured=nullptr;
    {
        std::lock_guard<std::mutex> children(child_mutex_);
        if(service_stopped_ || !LeaseCurrent() || !wake_word_bound_)return nullptr;
        captured=wake_word_.get();++wake_readers_;
    }
    // Service reservation covers the interval before the child enters its own
    // counted getter; never hold the control mutex during a blocking queue read.
    WakeReaderDone done{wake_readers_};auto packet=std::make_unique<AudioStreamPacket>();
    if(!captured->GetWakeWordOpus(packet->payload) || service_stopped_ || !LeaseCurrent()){WipeAudioBuffer(packet->payload);return nullptr;}
    return packet;
}
bool AudioService::EnableWakeWordDetection(bool enable){
    std::lock_guard<std::mutex> children(child_mutex_);
    if(service_stopped_ || !LeaseCurrent())return false;
    if(!enable){
        xEventGroupClearBits(event_group_,AS_EVENT_WAKE_WORD_RUNNING);
        if(wake_word_bound_ && !wake_word_->PauseChecked()){RequestStop();return false;}
        return true;
    }
    if(!PrepareWakeWordLocked()){RequestStop();return false;}
    {std::lock_guard<std::mutex> lock(input_resampler_mutex_);if(input_resampler_)esp_ae_rate_cvt_reset(input_resampler_);}
    if(service_stopped_ || !LeaseCurrent() || !wake_word_->StartChecked()){RequestStop();return false;}
    xEventGroupSetBits(event_group_,AS_EVENT_WAKE_WORD_RUNNING);return true;
}
bool AudioService::EnableVoiceProcessing(bool enable){
    std::lock_guard<std::mutex> children(child_mutex_);
    if(service_stopped_ || !LeaseCurrent())return false;
    if(!enable){
        xEventGroupClearBits(event_group_,AS_EVENT_AUDIO_PROCESSOR_RUNNING);
        if(audio_processor_bound_ && !audio_processor_->StopChecked()){RequestStop();return false;}
        return true;
    }
    if(!PrepareProcessorLocked()){RequestStop();return false;}
    ResetDecoder();audio_input_need_warmup_=true;
    {std::lock_guard<std::mutex> lock(input_resampler_mutex_);if(input_resampler_)esp_ae_rate_cvt_reset(input_resampler_);}
    if(service_stopped_ || !LeaseCurrent() || !audio_processor_->StartChecked()){RequestStop();return false;}
    xEventGroupSetBits(event_group_,AS_EVENT_AUDIO_PROCESSOR_RUNNING);return true;
}

void AudioService::EnableAudioTesting(bool enable) {
    if(service_stopped_ || !LeaseCurrent())return;
    ESP_LOGI(TAG, "%s audio testing", enable ? "Enabling" : "Disabling");
    if (enable) {
        xEventGroupSetBits(event_group_, AS_EVENT_AUDIO_TESTING_RUNNING);
    } else {
        xEventGroupClearBits(event_group_, AS_EVENT_AUDIO_TESTING_RUNNING);
        /* Copy audio_testing_queue_ to audio_decode_queue_ */
        std::lock_guard<std::mutex> lock(audio_queue_mutex_);
        audio_decode_queue_ = std::move(audio_testing_queue_);
        audio_queue_cv_.notify_all();
    }
}

bool AudioService::EnableDeviceAec(bool enable){
    std::lock_guard<std::mutex> children(child_mutex_);
    if(!PrepareProcessorLocked()){RequestStop();return false;}
    if(service_stopped_ || !LeaseCurrent())return false;
    audio_processor_->EnableDeviceAec(enable);return true;
}

void AudioService::SetCallbacks(AudioServiceCallbacks& callbacks) {
    callbacks_ = callbacks;
}

void AudioService::PlaySound(const std::string_view& ogg) {
    // Prompt decode is queued only; the checked output task owns hardware opens.
    if(service_stopped_ || !LeaseCurrent())return;

    const auto* buf = reinterpret_cast<const uint8_t*>(ogg.data());
    size_t size = ogg.size();

    auto demuxer = std::make_unique<OggDemuxer>();
    demuxer->OnDemuxerFinished([this, lease=SnapshotLease()](const uint8_t* data, int sample_rate, size_t size){
        if(service_stopped_ || !companion::CommonAudioAuthority().Current(lease))return;
        auto packet = std::make_unique<AudioStreamPacket>();
        packet->sample_rate = sample_rate;
        packet->frame_duration = 60;
        packet->payload.resize(size);
        std::memcpy(packet->payload.data(), data, size);
        PushPacketToDecodeQueue(std::move(packet), true);
    });
    demuxer->Reset();
    demuxer->Process(buf, size);
}

bool AudioService::IsIdle() {
    std::lock_guard<std::mutex> lock(audio_queue_mutex_);
    return audio_encode_queue_.empty() && audio_decode_queue_.empty() && audio_playback_queue_.empty() && audio_testing_queue_.empty();
}

void AudioService::WaitForPlaybackQueueEmpty() {
    std::unique_lock<std::mutex> lock(audio_queue_mutex_);
    audio_queue_cv_.wait(lock, [this]() { 
        return service_stopped_ || (audio_decode_queue_.empty() && audio_playback_queue_.empty()); 
    });
}

void AudioService::ResetDecoder() {
    std::lock_guard<std::mutex> io(hardware_mutex_);
    if(service_stopped_ || !LeaseCurrent() || !initialized_)return;
    std::lock_guard<std::mutex> lock(audio_queue_mutex_);
    std::unique_lock<std::mutex> decoder_lock(decoder_mutex_);
    if (opus_decoder_ != nullptr) {
        esp_opus_dec_reset(opus_decoder_);
    }
    decoder_lock.unlock();
    timestamp_queue_.clear();
    audio_decode_queue_.clear();
    audio_playback_queue_.clear();
    audio_testing_queue_.clear();
    audio_queue_cv_.notify_all();
}

void AudioService::CheckAndUpdateAudioPowerState() {
    std::lock_guard<std::mutex> io(hardware_mutex_);
    if(service_stopped_ || !LeaseCurrent())return;
    auto now = std::chrono::steady_clock::now();
    auto input_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_input_time_).count();
    auto output_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_output_time_).count();
    if (input_elapsed > AUDIO_POWER_TIMEOUT_MS && codec_->input_enabled()) {
        if(!LeaseCurrent() || !codec_->EnableInputChecked(false))RequestStop();
    }
    if (output_elapsed > AUDIO_POWER_TIMEOUT_MS && codec_->output_enabled()) {
        // Keep TX clock when duplex RX is active; otherwise RX may stall on some boards.
        if (!(codec_->duplex() && codec_->input_enabled())) {
            if(!LeaseCurrent() || !codec_->EnableOutputChecked(false))RequestStop();
        }
    }
    if (!codec_->input_enabled() && !codec_->output_enabled()) {
        esp_timer_stop(audio_power_timer_);
    }
}

void AudioService::SetModelsList(srmodel_list_t* models_list) {
    std::lock_guard<std::mutex> children(child_mutex_);
    if(!LeaseCurrent())return;
    if(wake_word_bound_){
        xEventGroupClearBits(event_group_,AS_EVENT_WAKE_WORD_RUNNING);
        if(!wake_word_->StopChecked()){RequestStop();return;}
        const auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(2000);
        while(wake_readers_ && std::chrono::steady_clock::now()<until)vTaskDelay(pdMS_TO_TICKS(1));
        if(wake_readers_){RequestStop();return;}
    }
    wake_word_bound_=false;wake_word_initialized_=false;models_list_ = models_list;

#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32P4
    if (esp_srmodel_filter(models_list_, ESP_MN_PREFIX, NULL) != nullptr) {
        wake_word_ = std::make_unique<CustomWakeWord>();
    } else if (esp_srmodel_filter(models_list_, ESP_WN_PREFIX, NULL) != nullptr) {
        wake_word_ = std::make_unique<AfeWakeWord>();
    } else {
        wake_word_ = nullptr;
    }
#else
    if (esp_srmodel_filter(models_list_, ESP_WN_PREFIX, NULL) != nullptr) {
        wake_word_ = std::make_unique<EspWakeWord>();
    } else {
        wake_word_ = nullptr;
    }
#endif

    if (wake_word_) {
        wake_word_->OnWakeWordDetected([this, lease=SnapshotLease()](const std::string& wake_word) {
            if(service_stopped_ || !companion::CommonAudioAuthority().Current(lease))return;
            if (callbacks_.on_wake_word_detected) {
                callbacks_.on_wake_word_detected(wake_word);
            }
        });
    }
}

bool AudioService::IsAfeWakeWord() {
    std::lock_guard<std::mutex> children(child_mutex_);
#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32P4
    return wake_word_ != nullptr && dynamic_cast<AfeWakeWord*>(wake_word_.get()) != nullptr;
#else
    return false;
#endif
}
