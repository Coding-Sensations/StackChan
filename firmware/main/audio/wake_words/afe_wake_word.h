#ifndef AFE_WAKE_WORD_H
#define AFE_WAKE_WORD_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <esp_afe_sr_models.h>
#include <esp_nsn_models.h>
#include <model_path.h>

#include <deque>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>

#include "audio_codec.h"
#include "wake_word.h"
#include "child_audio_lifecycle.hpp"
#include "checked_wake_encoder.hpp"

class AfeWakeWord : public WakeWord {
public:
    AfeWakeWord();
    ~AfeWakeWord();

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
    std::atomic<bool> running_{false},stopping_{false},detector_active_{false};
    bool owns_models_=false;
    srmodel_list_t *models_ = nullptr;
    const esp_afe_sr_iface_t* afe_iface_ = nullptr;
    esp_afe_sr_data_t* afe_data_ = nullptr;
    char* wakenet_model_ = NULL;
    std::vector<std::string> wake_words_;
    EventGroupHandle_t event_group_;
    std::function<void(const std::string& wake_word)> wake_word_detected_callback_;
    AudioCodec* codec_ = nullptr;
    std::string last_detected_wake_word_;
    std::vector<int16_t> input_buffer_;
    std::mutex input_buffer_mutex_;

    std::deque<std::vector<int16_t>> wake_word_pcm_;
    std::mutex wake_word_mutex_;

    void StoreWakeWordData(const int16_t* data, size_t size);
    void AudioDetectionTask();
};

#endif
