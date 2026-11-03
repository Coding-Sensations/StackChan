#ifndef CUSTOM_WAKE_WORD_H
#define CUSTOM_WAKE_WORD_H

#include <esp_attr.h>
#include <esp_mn_iface.h>
#include <esp_mn_models.h>
#include <model_path.h>

#include <deque>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include "audio_codec.h"
#include "wake_word.h"
#include "child_audio_lifecycle.hpp"
#include "checked_wake_encoder.hpp"

class CustomWakeWord : public WakeWord {
public:
    CustomWakeWord();
    ~CustomWakeWord();

    bool SupportsCheckedLifecycle() const override {return true;}
    bool BindAudioLease(companion::AudioLease lease) override;
    bool StartChecked() override;
    bool StopChecked(uint32_t timeout_ms=2000) override;
    bool PauseChecked(uint32_t timeout_ms=2000) override;
    bool EncodeWakeWordDataChecked() override;
    bool Initialize(AudioCodec* codec, srmodel_list_t* models_list) override;
    void Feed(const std::vector<int16_t>& data) override;
    void OnWakeWordDetected(std::function<void(const std::string& wake_word)> callback) override;
    void Start() override;
    void Stop() override;
    size_t GetFeedSize() override;
    void EncodeWakeWordData() override;
    bool GetWakeWordOpus(std::vector<uint8_t>& opus) override;
    const std::string& GetLastDetectedWakeWord() const override { return last_detected_wake_word_; }

private:
    checked_audio_child::Lifecycle life_;
    checked_audio_child::ResetWorker reset_{life_};
    std::atomic<bool> initializing_{false},initialized_{false};
    CheckedWakeEncoder encoder_{life_};
    std::mutex lifecycle_mutex_;
    std::atomic<bool> stopping_{false};
    bool owns_models_=false;
    struct Command {
        std::string command;
        std::string text;
        std::string action;
    };

    // multinet 相关成员变量
    esp_mn_iface_t* multinet_ = nullptr;
    model_iface_data_t* multinet_model_data_ = nullptr;
    srmodel_list_t *models_ = nullptr;
    char* mn_name_ = nullptr;
    std::string language_ = "cn";
    int duration_ = 3000;
    float threshold_ = 0.2;
    std::deque<Command> commands_;
 
    std::function<void(const std::string& wake_word)> wake_word_detected_callback_;
    AudioCodec* codec_ = nullptr;
    std::string last_detected_wake_word_;
    std::atomic<bool> running_ = false;
    std::vector<int16_t> input_buffer_;
    std::mutex input_buffer_mutex_;

    std::deque<std::vector<int16_t>> wake_word_pcm_;
    std::mutex wake_word_mutex_;

    void StoreWakeWordData(const std::vector<int16_t>& data);
    void ParseWakenetModelConfig();
};

#endif
