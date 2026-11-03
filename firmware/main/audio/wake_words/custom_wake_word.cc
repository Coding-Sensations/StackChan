#include "custom_wake_word.h"
#include "audio_service.h"
#include "system_info.h"
#include "assets.h"

#include <esp_log.h>
#include <esp_mn_iface.h>
#include <esp_mn_models.h>
#include <esp_mn_speech_commands.h>
#include <cJSON.h>

#define TAG "CustomWakeWord"

CustomWakeWord::CustomWakeWord()
    : wake_word_pcm_() {
}

CustomWakeWord::~CustomWakeWord(){
    configASSERT(life_.Joined()&&!running_&&!encoder_.Active());
    if(multinet_model_data_&&multinet_)multinet_->destroy(multinet_model_data_);
    if(owns_models_&&models_)esp_srmodel_deinit(models_);
}
bool CustomWakeWord::BindAudioLease(companion::AudioLease lease){std::lock_guard<std::mutex> lock(lifecycle_mutex_);return !initializing_&&!running_&&!stopping_&&life_.Bind(lease);}

void CustomWakeWord::ParseWakenetModelConfig() {
    // Read index.json
    auto& assets = Assets::GetInstance();
    void* ptr = nullptr;
    size_t size = 0;
    if (!assets.GetAssetData("index.json", ptr, size)) {
        ESP_LOGE(TAG, "Failed to read index.json");
        return;
    }
    cJSON* root = cJSON_ParseWithLength(static_cast<char*>(ptr), size);
    if (root == nullptr) {
        ESP_LOGE(TAG, "Failed to parse index.json");
        return;
    }
    cJSON* multinet_model = cJSON_GetObjectItem(root, "multinet_model");
    if (cJSON_IsObject(multinet_model)) {
        cJSON* language = cJSON_GetObjectItem(multinet_model, "language");
        cJSON* duration = cJSON_GetObjectItem(multinet_model, "duration");
        cJSON* threshold = cJSON_GetObjectItem(multinet_model, "threshold");
        cJSON* commands = cJSON_GetObjectItem(multinet_model, "commands");
        if (cJSON_IsString(language)) {
            language_ = language->valuestring;
        }
        if (cJSON_IsNumber(duration)) {
            duration_ = duration->valueint;
        }
        if (cJSON_IsNumber(threshold)) {
            threshold_ = threshold->valuedouble;
        }
        if (cJSON_IsArray(commands)) {
            for (int i = 0; i < cJSON_GetArraySize(commands); i++) {
                cJSON* command = cJSON_GetArrayItem(commands, i);
                if (cJSON_IsObject(command)) {
                    cJSON* command_name = cJSON_GetObjectItem(command, "command");
                    cJSON* text = cJSON_GetObjectItem(command, "text");
                    cJSON* action = cJSON_GetObjectItem(command, "action");
                    if (cJSON_IsString(command_name) && cJSON_IsString(text) && cJSON_IsString(action)) {
                        commands_.push_back({command_name->valuestring, text->valuestring, action->valuestring});
                        ESP_LOGI(TAG, "Command: %s, Text: %s, Action: %s", command_name->valuestring, text->valuestring, action->valuestring);
                    }
                }
            }
        }
    }
    cJSON_Delete(root);
}


bool CustomWakeWord::Initialize(AudioCodec* codec, srmodel_list_t* models_list) {
    {std::lock_guard<std::mutex> control(lifecycle_mutex_);
    if(initializing_||stopping_||!life_.Authorized()||!life_.Joined()||!codec||multinet_model_data_||codec->input_channels()<1||codec->input_channels()>2)return false;
    if(!life_.Resume()){return false;}initializing_=true;life_.ReserveOperation();}
    checked_audio_child::Lifecycle::Operation preparation(life_,true);
    auto finish=checked_audio_child::Finally([this]{life_.RequestStop();initializing_=false;});
    if(!preparation)return false;
    codec_ = codec;
    commands_.clear();

    if (models_list == nullptr) {
        language_ = "cn";
        owns_models_=true;models_ = esp_srmodel_init("model");
#ifdef CONFIG_CUSTOM_WAKE_WORD
        threshold_ = CONFIG_CUSTOM_WAKE_WORD_THRESHOLD / 100.0f;
        commands_.push_back({CONFIG_CUSTOM_WAKE_WORD, CONFIG_CUSTOM_WAKE_WORD_DISPLAY, "wake"});
#endif
    } else {
        models_ = models_list;
        ParseWakenetModelConfig();
    }

    if (models_ == nullptr || models_->num == -1) {
        ESP_LOGE(TAG, "Failed to initialize wakenet model");
        return false;
    }

    // 初始化 multinet (命令词识别)
    mn_name_ = esp_srmodel_filter(models_, ESP_MN_PREFIX, language_.c_str());
    if (mn_name_ == nullptr) {
        ESP_LOGW(TAG, "Language '%s' multinet not found, falling back to any multinet model", language_.c_str());
        mn_name_ = esp_srmodel_filter(models_, ESP_MN_PREFIX, NULL);
    }
    if (mn_name_ == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize multinet, mn_name is nullptr");
        ESP_LOGI(TAG, "Please refer to https://pcn7cs20v8cr.feishu.cn/wiki/CpQjwQsCJiQSWSkYEvrcxcbVnwh to add custom wake word");
        return false;
    }

    multinet_ = esp_mn_handle_from_name(mn_name_);
    if(!multinet_||!multinet_->create||!multinet_->detect||!multinet_->destroy)return false;
    multinet_model_data_ = multinet_->create(mn_name_, duration_);
    if(!multinet_model_data_)return false;
    multinet_->set_det_threshold(multinet_model_data_, threshold_);
    esp_mn_commands_clear();
    for (int i = 0; i < commands_.size(); i++) {
        esp_mn_commands_add(i + 1, commands_[i].command.c_str());
    }
    esp_mn_commands_update();
    
    multinet_->print_active_speech_commands(multinet_model_data_);
    return initialized_=!commands_.empty()&&multinet_->get_samp_chunksize(multinet_model_data_)>0&&multinet_->get_samp_chunksize(multinet_model_data_)<=4096&&life_.Current();
}

void CustomWakeWord::OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) {
    std::lock_guard<std::mutex> control(lifecycle_mutex_);if(!life_.Joined())return;wake_word_detected_callback_ = callback;
}

bool CustomWakeWord::StartChecked(){
    std::lock_guard<std::mutex> control(lifecycle_mutex_);
    if(initializing_||stopping_||!reset_.Success()||encoder_.Active())return false;
    if(running_)return life_.Current();
    if(!initialized_||!multinet_model_data_||!life_.Resume())return false;
    reset_.Dirty();running_=true;return true;
}
void CustomWakeWord::Start(){(void)StartChecked();}
bool CustomWakeWord::StopChecked(uint32_t timeout_ms){
    const TickType_t start=xTaskGetTickCount();
    {std::lock_guard<std::mutex> control(lifecycle_mutex_);stopping_=true;running_=false;life_.RequestStop();encoder_.NotifyStop();}
    if(!life_.Join(checked_audio_child::Remaining(start,timeout_ms)))return false;
    if(!reset_.Success()){
        if(!reset_.Start([](void* p){auto* self=static_cast<CustomWakeWord*>(p);if(self->multinet_model_data_)self->multinet_->clean(self->multinet_model_data_);return true;},this))return false;
        if(!life_.Join(checked_audio_child::Remaining(start,timeout_ms))||!reset_.Success())return false;
    }
    std::lock_guard<std::mutex> control(lifecycle_mutex_);
    std::lock_guard<std::mutex> input(input_buffer_mutex_);checked_audio_child::Wipe(input_buffer_);
    {std::lock_guard<std::mutex> lock(wake_word_mutex_);for(auto& pcm:wake_word_pcm_){checked_audio_child::Wipe(pcm);}wake_word_pcm_.clear();}encoder_.Clear();
    stopping_=false;return true;
}

bool CustomWakeWord::PauseChecked(uint32_t timeout_ms){
    const TickType_t start=xTaskGetTickCount();
    {std::lock_guard<std::mutex> control(lifecycle_mutex_);if(initializing_)return false;stopping_=true;running_=false;}
    // Encoder and cached PCM remain authorized; join only detection/Feed/callback.
    if(!checked_audio_child::Wait([this]{return life_.OperationsJoined();},start,timeout_ms))return false;
    if(!reset_.Success()){
        if(!reset_.Start([](void* p){auto* self=static_cast<CustomWakeWord*>(p);if(self->multinet_model_data_)self->multinet_->clean(self->multinet_model_data_);return true;},this))return false;
        if(!checked_audio_child::Wait([this]{return !reset_.Active();},start,timeout_ms)||!reset_.Success())return false;
    }
    std::lock_guard<std::mutex> control(lifecycle_mutex_);
    std::lock_guard<std::mutex> input(input_buffer_mutex_);checked_audio_child::Wipe(input_buffer_);stopping_=false;return true;
}

void CustomWakeWord::Stop(){(void)StopChecked();}

void CustomWakeWord::Feed(const std::vector<int16_t>& data) {
    checked_audio_child::Lifecycle::Operation operation(life_);
    if(!operation||data.size()>8192)return;
    if (multinet_model_data_ == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(input_buffer_mutex_);
    // Check running state inside lock to avoid TOCTOU race with Stop()
    if (!running_||!life_.Current()) {
        return;
    }

    // If input channels is 2, we need to fetch the left channel data
    if (codec_->input_channels() == 2) {
        for (size_t i = 0; i < data.size(); i += 2) {
            input_buffer_.push_back(data[i]);
        }
    } else {
        input_buffer_.insert(input_buffer_.end(), data.begin(), data.end());
    }
    
    int chunksize = multinet_->get_samp_chunksize(multinet_model_data_);
    if(chunksize<=0||chunksize>4096||input_buffer_.size()>16384){checked_audio_child::Wipe(input_buffer_);return;}
    while (input_buffer_.size() >= size_t(chunksize)&&running_&&life_.Current()) {
        std::vector<int16_t> chunk(input_buffer_.begin(), input_buffer_.begin() + chunksize);
        StoreWakeWordData(chunk);
        
        esp_mn_state_t mn_state = multinet_->detect(multinet_model_data_, chunk.data());
        
        if(!life_.Current()||!running_){checked_audio_child::Wipe(chunk);break;}
        if (mn_state == ESP_MN_STATE_DETECTED) {
            esp_mn_results_t *mn_result = multinet_->get_results(multinet_model_data_);
            for (int i = 0; mn_result&&i < mn_result->num && running_&&life_.Current(); i++) {
                ESP_LOGI(TAG, "Custom wake word detected: command_id=%d, string=%s, prob=%f", 
                        mn_result->command_id[i], mn_result->string, mn_result->prob[i]);
                const int index=mn_result->command_id[i]-1;if(index<0||size_t(index)>=commands_.size())continue;
                auto& command = commands_[index];
                if (command.action == "wake") {
                    last_detected_wake_word_ = command.text;
                    running_ = false;
                    checked_audio_child::Wipe(input_buffer_);
                    
                    if (life_.Current()&&wake_word_detected_callback_) {
                        wake_word_detected_callback_(last_detected_wake_word_);
                    }
                }
            }
            multinet_->clean(multinet_model_data_);
        } else if (mn_state == ESP_MN_STATE_TIMEOUT) {
            ESP_LOGD(TAG, "Command word detection timeout, cleaning state");
            multinet_->clean(multinet_model_data_);
        }
        
        if (!running_||!life_.Current()) {
            break;
        }
        std::fill_n(input_buffer_.begin(),chunksize,0);input_buffer_.erase(input_buffer_.begin(), input_buffer_.begin() + chunksize);
        checked_audio_child::Wipe(chunk);
    }
}

size_t CustomWakeWord::GetFeedSize() {
    checked_audio_child::Lifecycle::Operation operation(life_);if(!operation)return 0;
    if (multinet_model_data_ == nullptr) {
        return 0;
    }
    return multinet_->get_samp_chunksize(multinet_model_data_);
}

void CustomWakeWord::StoreWakeWordData(const std::vector<int16_t>& data){
    if(data.size()>4096||!life_.Current())return;
    std::lock_guard<std::mutex> lock(wake_word_mutex_);if(!life_.Current())return;
    wake_word_pcm_.push_back(data);
    while(wake_word_pcm_.size()>66){checked_audio_child::Wipe(wake_word_pcm_.front());wake_word_pcm_.pop_front();}
}
bool CustomWakeWord::EncodeWakeWordDataChecked(){
    std::lock_guard<std::mutex> control(lifecycle_mutex_);
    if(running_||stopping_||!life_.Current()||encoder_.Active())return false;
    std::deque<std::vector<int16_t>> snapshot;
    {std::lock_guard<std::mutex> lock(wake_word_mutex_);snapshot.swap(wake_word_pcm_);}
    return encoder_.Start(std::move(snapshot));
}
void CustomWakeWord::EncodeWakeWordData(){(void)EncodeWakeWordDataChecked();}
bool CustomWakeWord::GetWakeWordOpus(std::vector<uint8_t>& opus){checked_audio_child::Lifecycle::Operation operation(life_);if(!operation){checked_audio_child::Wipe(opus);return false;}return encoder_.Get(opus);}
