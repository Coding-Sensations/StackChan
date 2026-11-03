#include "afe_wake_word.h"
#include "audio_service.h"
#include <esp_log.h>
#include <sstream>

#define DETECTION_RUNNING_EVENT 1

#define TAG "AfeWakeWord"

AfeWakeWord::AfeWakeWord()
    : afe_data_(nullptr),
      wake_word_pcm_() {

    event_group_ = xEventGroupCreate();
}

AfeWakeWord::~AfeWakeWord(){
    configASSERT(life_.Joined()&&!running_&&!encoder_.Active());
    if(afe_data_)afe_iface_->destroy(afe_data_);
    if(owns_models_&&models_)esp_srmodel_deinit(models_);
    if(event_group_)vEventGroupDelete(event_group_);
}
bool AfeWakeWord::BindAudioLease(companion::AudioLease lease){std::lock_guard<std::mutex> lock(lifecycle_mutex_);return !initializing_&&!running_&&!stopping_&&life_.Bind(lease);}

bool AfeWakeWord::Initialize(AudioCodec* codec, srmodel_list_t* models_list) {
    {std::lock_guard<std::mutex> control(lifecycle_mutex_);
    if(initializing_||stopping_||!life_.Authorized()||!life_.Joined()||!event_group_||!codec||afe_data_||codec->input_channels()<1||codec->input_channels()>2)return false;
    if(!life_.Resume()){return false;}initializing_=true;life_.ReserveOperation();}
    checked_audio_child::Lifecycle::Operation preparation(life_,true);
    auto finish=checked_audio_child::Finally([this]{life_.RequestStop();initializing_=false;});
    if(!preparation)return false;
    codec_ = codec;
    int ref_num = codec_->input_reference() ? 1 : 0;

    if (models_list == nullptr) {
        owns_models_=true;models_ = esp_srmodel_init("model");
    } else {
        models_ = models_list;
    }

    if (models_ == nullptr || models_->num == -1) {
        ESP_LOGE(TAG, "Failed to initialize wakenet model");
        return false;
    }
    for (int i = 0; i < models_->num; i++) {
        ESP_LOGI(TAG, "Model %d: %s", i, models_->model_name[i]);
        if (strstr(models_->model_name[i], ESP_WN_PREFIX) != NULL) {
            wakenet_model_ = models_->model_name[i];
            auto words = esp_srmodel_get_wake_words(models_, wakenet_model_);
            // split by ";" to get all wake words
            if(!words)continue;
            std::stringstream ss(words);
            std::string word;
            while (std::getline(ss, word, ';')) {
                wake_words_.push_back(word);
            }
        }
    }

    std::string input_format;
    for (int i = 0; i < codec_->input_channels() - ref_num; i++) {
        input_format.push_back('M');
    }
    for (int i = 0; i < ref_num; i++) {
        input_format.push_back('R');
    }
    afe_config_t* afe_config = afe_config_init(input_format.c_str(), models_, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
    if(!afe_config||wake_words_.empty()){if(afe_config)afe_config_free(afe_config);return false;}
    afe_config->aec_init = codec_->input_reference();
    afe_config->aec_mode = AEC_MODE_SR_HIGH_PERF;
    afe_config->afe_perferred_core = 1;
    afe_config->afe_perferred_priority = 1;
    afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    
    afe_iface_ = esp_afe_handle_from_config(afe_config);
    if(afe_iface_&&afe_iface_->create_from_config&&afe_iface_->fetch_with_delay&&afe_iface_->reset_buffer&&afe_iface_->destroy)afe_data_=afe_iface_->create_from_config(afe_config);
    afe_config_free(afe_config);
    return initialized_=afe_data_&&afe_iface_->get_feed_chunksize(afe_data_)>0&&afe_iface_->get_feed_chunksize(afe_data_)<=4096&&life_.Current();
}

void AfeWakeWord::OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) {
    std::lock_guard<std::mutex> control(lifecycle_mutex_);if(!life_.Joined())return;wake_word_detected_callback_ = callback;
}

bool AfeWakeWord::StartChecked(){
    std::lock_guard<std::mutex> control(lifecycle_mutex_);
    if(initializing_||stopping_||!reset_.Success())return false;
    if(running_)return life_.Current();
    if(!initialized_||!afe_data_||encoder_.Active()||!life_.Resume())return false;
    {std::lock_guard<std::mutex> lock(wake_word_mutex_);for(auto& pcm:wake_word_pcm_)checked_audio_child::Wipe(pcm);wake_word_pcm_.clear();}
    reset_.Dirty();running_=true;detector_active_=true;life_.WorkerStarted();
    if(xTaskCreate([](void* p){auto* self=static_cast<AfeWakeWord*>(p);self->AudioDetectionTask();self->detector_active_=false;self->life_.WorkerFinished();vTaskDelete(nullptr);},"audio_detection",4096,this,3,nullptr)!=pdPASS){running_=false;detector_active_=false;life_.RequestStop();life_.WorkerFinished();return false;}
    return true;
}
void AfeWakeWord::Start(){(void)StartChecked();}
bool AfeWakeWord::StopChecked(uint32_t timeout_ms){
    const TickType_t start=xTaskGetTickCount();
    {std::lock_guard<std::mutex> control(lifecycle_mutex_);stopping_=true;running_=false;life_.RequestStop();encoder_.NotifyStop();}
    if(!life_.Join(checked_audio_child::Remaining(start,timeout_ms)))return false;
    if(!reset_.Success()){
        if(!reset_.Start([](void* p){auto* self=static_cast<AfeWakeWord*>(p);return !self->afe_data_||self->afe_iface_->reset_buffer(self->afe_data_)==1;},this))return false;
        if(!life_.Join(checked_audio_child::Remaining(start,timeout_ms))||!reset_.Success())return false;
    }
    std::lock_guard<std::mutex> control(lifecycle_mutex_);
    std::lock_guard<std::mutex> input(input_buffer_mutex_);checked_audio_child::Wipe(input_buffer_);
    {std::lock_guard<std::mutex> lock(wake_word_mutex_);for(auto& pcm:wake_word_pcm_){checked_audio_child::Wipe(pcm);}wake_word_pcm_.clear();}encoder_.Clear();
    stopping_=false;return true;
}

bool AfeWakeWord::PauseChecked(uint32_t timeout_ms){
    const TickType_t start=xTaskGetTickCount();
    {std::lock_guard<std::mutex> control(lifecycle_mutex_);if(initializing_)return false;stopping_=true;running_=false;}
    // Encoder and cached PCM remain authorized; join only detection/Feed/callback.
    if(!checked_audio_child::Wait([this]{return !detector_active_&&life_.OperationsJoined();},start,timeout_ms))return false;
    if(!reset_.Success()){
        if(!reset_.Start([](void* p){auto* self=static_cast<AfeWakeWord*>(p);return !self->afe_data_||self->afe_iface_->reset_buffer(self->afe_data_)==1;},this))return false;
        if(!checked_audio_child::Wait([this]{return !reset_.Active();},start,timeout_ms)||!reset_.Success())return false;
    }
    std::lock_guard<std::mutex> control(lifecycle_mutex_);
    std::lock_guard<std::mutex> input(input_buffer_mutex_);checked_audio_child::Wipe(input_buffer_);stopping_=false;return true;
}

void AfeWakeWord::Stop(){(void)StopChecked();}
void AfeWakeWord::Feed(const std::vector<int16_t>& data){
    checked_audio_child::Lifecycle::Operation operation(life_);
    if(!operation||!running_||!afe_data_||data.size()>8192)return;
    std::lock_guard<std::mutex> lock(input_buffer_mutex_);
    if(!running_||!life_.Current())return;
    const size_t chunk=afe_iface_->get_feed_chunksize(afe_data_)*codec_->input_channels();
    if(!chunk||chunk>8192||input_buffer_.size()+data.size()>16384)return;
    input_buffer_.insert(input_buffer_.end(),data.begin(),data.end());
    while(input_buffer_.size()>=chunk&&running_&&life_.Current()){
        if(afe_iface_->feed(afe_data_,input_buffer_.data())<=0){running_=false;life_.RequestStop();break;}
        std::fill_n(input_buffer_.begin(),chunk,0);input_buffer_.erase(input_buffer_.begin(),input_buffer_.begin()+chunk);
    }
    if(!running_||!life_.Current())checked_audio_child::Wipe(input_buffer_);
}

size_t AfeWakeWord::GetFeedSize() {
    checked_audio_child::Lifecycle::Operation operation(life_);if(!operation)return 0;
    if (afe_data_ == nullptr) {
        return 0;
    }
    return afe_iface_->get_feed_chunksize(afe_data_);
}

void AfeWakeWord::AudioDetectionTask(){
    while(running_&&life_.Current()){
        auto* res=afe_iface_->fetch_with_delay(afe_data_,pdMS_TO_TICKS(20)>0?pdMS_TO_TICKS(20):1);
        if(!running_||!life_.Current())break;
        if(!res||res->ret_value==ESP_FAIL||!res->data||res->data_size<=0||res->data_size%sizeof(int16_t)||res->data_size>8192)continue;
        StoreWakeWordData(res->data,res->data_size/sizeof(int16_t));
        if(res->wakeup_state==WAKENET_DETECTED){
            const int index=res->wakenet_model_index-1;
            if(index<0||size_t(index)>=wake_words_.size())continue;
            last_detected_wake_word_=wake_words_[index];running_=false;
            // Pause detector but retain cache authority for the existing stock
            // wake-data encoder. Whole-child StopChecked cancels and joins it.
            if(life_.Current()&&wake_word_detected_callback_)wake_word_detected_callback_(last_detected_wake_word_);
            break;
        }
    }
    running_=false;
}
void AfeWakeWord::StoreWakeWordData(const int16_t* data,size_t samples){
    if(!data||samples>4096||!life_.Current())return;
    std::lock_guard<std::mutex> lock(wake_word_mutex_);if(!life_.Current())return;
    wake_word_pcm_.emplace_back(data,data+samples);
    while(wake_word_pcm_.size()>66){checked_audio_child::Wipe(wake_word_pcm_.front());wake_word_pcm_.pop_front();}
}
bool AfeWakeWord::EncodeWakeWordDataChecked(){
    std::lock_guard<std::mutex> control(lifecycle_mutex_);
    if(running_||stopping_||!life_.Current()||encoder_.Active())return false;
    std::deque<std::vector<int16_t>> snapshot;
    {std::lock_guard<std::mutex> lock(wake_word_mutex_);snapshot.swap(wake_word_pcm_);}
    return encoder_.Start(std::move(snapshot));
}
void AfeWakeWord::EncodeWakeWordData(){(void)EncodeWakeWordDataChecked();}
bool AfeWakeWord::GetWakeWordOpus(std::vector<uint8_t>& opus){checked_audio_child::Lifecycle::Operation operation(life_);if(!operation){checked_audio_child::Wipe(opus);return false;}return encoder_.Get(opus);}
