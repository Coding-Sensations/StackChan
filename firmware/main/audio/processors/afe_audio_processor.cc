#include "afe_audio_processor.h"
#include <esp_log.h>

#define PROCESSOR_RUNNING 0x01

#define TAG "AfeAudioProcessor"

AfeAudioProcessor::AfeAudioProcessor()
    : afe_data_(nullptr) {
    event_group_ = xEventGroupCreate();
}

bool AfeAudioProcessor::BindAudioLease(companion::AudioLease lease){std::lock_guard<std::mutex> control(lifecycle_mutex_);return !initializing_&&!stopping_&&!running_&&life_.Bind(lease);}
void AfeAudioProcessor::Initialize(AudioCodec* codec,int duration,srmodel_list_t* models){(void)InitializeChecked(codec,duration,models);}
bool AfeAudioProcessor::InitializeChecked(AudioCodec* codec, int frame_duration_ms, srmodel_list_t* models_list) {
    {std::lock_guard<std::mutex> control(lifecycle_mutex_);
    if(initializing_||stopping_||!life_.Authorized()||!life_.Joined()||!event_group_||!codec||afe_data_||frame_duration_ms<=0||frame_duration_ms>120||codec->input_channels()<1||codec->input_channels()>2)return false;
    if(!life_.Resume()){return false;}initializing_=true;life_.ReserveOperation();}
    checked_audio_child::Lifecycle::Operation preparation(life_,true);
    auto finish=checked_audio_child::Finally([this]{life_.RequestStop();initializing_=false;});
    if(!preparation)return false;
    codec_ = codec;
    frame_samples_ = frame_duration_ms * 16000 / 1000;

    // Pre-allocate output buffer capacity
    output_buffer_.reserve(frame_samples_);

    int ref_num = codec_->input_reference() ? 1 : 0;

    std::string input_format;
    for (int i = 0; i < codec_->input_channels() - ref_num; i++) {
        input_format.push_back('M');
    }
    for (int i = 0; i < ref_num; i++) {
        input_format.push_back('R');
    }

    srmodel_list_t *models;
    if (models_list == nullptr) {
        models = owned_models_ = esp_srmodel_init("model");
    } else {
        models = models_list;
    }

    if(!models)return false;
    char* ns_model_name = esp_srmodel_filter(models, ESP_NSNET_PREFIX, NULL);
    char* vad_model_name = esp_srmodel_filter(models, ESP_VADN_PREFIX, NULL);
    
    afe_config_t* afe_config = afe_config_init(input_format.c_str(), NULL, AFE_TYPE_VC, AFE_MODE_HIGH_PERF);
    if(!afe_config)return false;
    afe_config->aec_mode = AEC_MODE_VOIP_HIGH_PERF;
    afe_config->vad_mode = VAD_MODE_0;
    afe_config->vad_min_noise_ms = 100;
    if (vad_model_name != nullptr) {
        afe_config->vad_model_name = vad_model_name;
    }

    if (ns_model_name != nullptr) {
        afe_config->ns_init = true;
        afe_config->ns_model_name = ns_model_name;
        afe_config->afe_ns_mode = AFE_NS_MODE_NET;
    } else {
        afe_config->ns_init = false;
    }

    afe_config->agc_init = false;
    afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;

#ifdef CONFIG_USE_DEVICE_AEC
    afe_config->aec_init = true;
    afe_config->vad_init = false;
#else
    afe_config->aec_init = false;
    afe_config->vad_init = true;
#endif

    afe_iface_ = esp_afe_handle_from_config(afe_config);
    if(afe_iface_&&afe_iface_->create_from_config&&afe_iface_->fetch_with_delay&&afe_iface_->reset_buffer&&afe_iface_->destroy)afe_data_=afe_iface_->create_from_config(afe_config);
    afe_config_free(afe_config);
    if(!afe_data_)return false;
    const int feed=afe_iface_->get_feed_chunksize(afe_data_);
    return initialized_=feed>0&&feed<=4096&&life_.Current();
}

AfeAudioProcessor::~AfeAudioProcessor() {
    configASSERT(life_.Joined()&&!running_);
    if(afe_data_)afe_iface_->destroy(afe_data_);
    if(owned_models_)esp_srmodel_deinit(owned_models_);
    if(event_group_)vEventGroupDelete(event_group_);
}

size_t AfeAudioProcessor::GetFeedSize() {
    checked_audio_child::Lifecycle::Operation operation(life_);if(!operation)return 0;
    if (afe_data_ == nullptr) {
        return 0;
    }
    return afe_iface_->get_feed_chunksize(afe_data_);
}

void AfeAudioProcessor::Feed(std::vector<int16_t>&& data){
    checked_audio_child::Lifecycle::Operation operation(life_);
    if(!operation||!afe_data_||data.size()>8192){checked_audio_child::Wipe(data);return;}
    std::lock_guard<std::mutex> lock(input_buffer_mutex_);
    if(!IsRunning()){checked_audio_child::Wipe(data);return;}
    const size_t chunk=afe_iface_->get_feed_chunksize(afe_data_)*codec_->input_channels();
    if(!chunk||chunk>8192||input_buffer_.size()+data.size()>16384){checked_audio_child::Wipe(data);return;}
    input_buffer_.insert(input_buffer_.end(),data.begin(),data.end());checked_audio_child::Wipe(data);
    while(input_buffer_.size()>=chunk&&IsRunning()){
        if(afe_iface_->feed(afe_data_,input_buffer_.data())<=0){running_=false;life_.RequestStop();break;}
        std::fill_n(input_buffer_.begin(),chunk,0);input_buffer_.erase(input_buffer_.begin(),input_buffer_.begin()+chunk);
    }
    if(!IsRunning())checked_audio_child::Wipe(input_buffer_);
}

bool AfeAudioProcessor::StartChecked(){
    std::lock_guard<std::mutex> control(lifecycle_mutex_);
    if(initializing_||stopping_||!reset_.Success())return false;
    if(running_)return life_.Current();
    if(!initialized_||!afe_data_||!life_.Resume())return false;
    reset_.Dirty();running_=true;life_.WorkerStarted();
    if(xTaskCreate([](void* p){auto* self=static_cast<AfeAudioProcessor*>(p);self->AudioProcessorTask();self->life_.WorkerFinished();vTaskDelete(nullptr);},"audio_communication",4096,this,3,nullptr)!=pdPASS){
        running_=false;life_.RequestStop();life_.WorkerFinished();return false;
    }
    return true;
}
void AfeAudioProcessor::Start(){(void)StartChecked();}
bool AfeAudioProcessor::StopChecked(uint32_t timeout_ms){
    const TickType_t start=xTaskGetTickCount();
    {std::lock_guard<std::mutex> control(lifecycle_mutex_);stopping_=true;running_=false;life_.RequestStop();}
    if(!life_.Join(checked_audio_child::Remaining(start,timeout_ms)))return false;
    if(!reset_.Success()){
        if(!reset_.Start([](void* p){auto* self=static_cast<AfeAudioProcessor*>(p);return !self->afe_data_||self->afe_iface_->reset_buffer(self->afe_data_)==1;},this))return false;
        if(!life_.Join(checked_audio_child::Remaining(start,timeout_ms))||!reset_.Success())return false;
    }
    std::lock_guard<std::mutex> control(lifecycle_mutex_);
    std::lock_guard<std::mutex> input(input_buffer_mutex_);checked_audio_child::Wipe(input_buffer_);
    checked_audio_child::Wipe(output_buffer_);is_speaking_=false;
    stopping_=false;return true;
}

void AfeAudioProcessor::Stop(){(void)StopChecked();}
bool AfeAudioProcessor::IsRunning(){return running_&&life_.Current();}

void AfeAudioProcessor::OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) {
    std::lock_guard<std::mutex> control(lifecycle_mutex_);if(!life_.Joined())return;output_callback_ = callback;
}

void AfeAudioProcessor::OnVadStateChange(std::function<void(bool speaking)> callback) {
    std::lock_guard<std::mutex> control(lifecycle_mutex_);if(!life_.Joined())return;vad_state_change_callback_ = callback;
}

void AfeAudioProcessor::AudioProcessorTask(){
    while(IsRunning()){
        auto* res=afe_iface_->fetch_with_delay(afe_data_,pdMS_TO_TICKS(20)>0?pdMS_TO_TICKS(20):1);
        if(!IsRunning())break;
        if(!res||res->ret_value==ESP_FAIL||!res->data||res->data_size<=0||res->data_size%sizeof(int16_t)||res->data_size>8192)continue;
        const bool speaking=res->vad_state==VAD_SPEECH;
        if(vad_state_change_callback_&&speaking!=is_speaking_&&IsRunning()){is_speaking_=speaking;vad_state_change_callback_(speaking);}
        if(!IsRunning())break;
        if(output_callback_){
            const size_t samples=res->data_size/sizeof(int16_t);
            if(output_buffer_.size()+samples>8192){checked_audio_child::Wipe(output_buffer_);continue;}
            output_buffer_.insert(output_buffer_.end(),res->data,res->data+samples);
            while(output_buffer_.size()>=size_t(frame_samples_)&&IsRunning()){
                std::vector<int16_t> frame(output_buffer_.begin(),output_buffer_.begin()+frame_samples_);
                std::fill_n(output_buffer_.begin(),frame_samples_,0);output_buffer_.erase(output_buffer_.begin(),output_buffer_.begin()+frame_samples_);
                if(IsRunning()){output_callback_(std::move(frame));}
                checked_audio_child::Wipe(frame);
            }
        }
    }
    running_=false;
}

void AfeAudioProcessor::EnableDeviceAec(bool enable) {
    bool paused=false;
    {
        std::lock_guard<std::mutex> control(lifecycle_mutex_);
        // Software configuration needs genuine current owner custody, but does
        // not admit Feed/capture or create a processing worker before Start.
        if(initializing_||stopping_||!initialized_||!afe_data_||!afe_iface_||!life_.Authorized())return;
        paused=!running_;
        if(paused){if(!life_.Joined()||!life_.Resume())return;}
        else if(!life_.Current())return;
        life_.ReserveOperation();
    }
    checked_audio_child::Lifecycle::Operation configuration(life_,true);
    auto finish=checked_audio_child::Finally([this,paused]{if(paused)life_.RequestStop();});
    if(!configuration||!life_.Current())return;
    bool configured=false;
    if(enable){
#if CONFIG_USE_DEVICE_AEC
        configured=afe_iface_->disable_vad&&afe_iface_->enable_aec&&
            afe_iface_->disable_vad(afe_data_)==0&&life_.Current()&&afe_iface_->enable_aec(afe_data_)==1&&life_.Current();
#else
        ESP_LOGE(TAG, "Device AEC is not supported");return;
#endif
    }else{
        configured=afe_iface_->disable_aec&&afe_iface_->enable_vad&&
            afe_iface_->disable_aec(afe_data_)==0&&life_.Current()&&afe_iface_->enable_vad(afe_data_)==1&&life_.Current();
    }
    if(!configured){
        // Partial/failed vendor configuration cannot silently activate voice.
        initialized_=false;running_=false;reset_.Dirty();life_.RequestStop();
    }
}
