#ifndef AFE_AUDIO_PROCESSOR_H
#define AFE_AUDIO_PROCESSOR_H

#include <esp_afe_sr_models.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <string>
#include <vector>
#include <functional>
#include <mutex>

#include "audio_processor.h"
#include "child_audio_lifecycle.hpp"
#include "audio_codec.h"

class AfeAudioProcessor : public AudioProcessor {
public:
    AfeAudioProcessor();
    ~AfeAudioProcessor();

    bool SupportsCheckedLifecycle() const override { return true; }
    bool BindAudioLease(companion::AudioLease lease) override;
    bool InitializeChecked(AudioCodec*,int,srmodel_list_t*) override;
    bool StartChecked() override;
    bool StopChecked(uint32_t timeout_ms=2000) override;
    void Initialize(AudioCodec* codec, int frame_duration_ms, srmodel_list_t* models_list) override;
    void Feed(std::vector<int16_t>&& data) override;
    void Start() override;
    void Stop() override;
    bool IsRunning() override;
    void OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) override;
    void OnVadStateChange(std::function<void(bool speaking)> callback) override;
    size_t GetFeedSize() override;
    void EnableDeviceAec(bool enable) override;

private:
    checked_audio_child::Lifecycle life_;
    checked_audio_child::ResetWorker reset_{life_};
    std::atomic<bool> initializing_{false},initialized_{false};
    std::mutex lifecycle_mutex_;
    std::atomic<bool> running_{false},stopping_{false};
    srmodel_list_t* owned_models_=nullptr;
    EventGroupHandle_t event_group_ = nullptr;
    const esp_afe_sr_iface_t* afe_iface_ = nullptr;
    esp_afe_sr_data_t* afe_data_ = nullptr;
    std::function<void(std::vector<int16_t>&& data)> output_callback_;
    std::function<void(bool speaking)> vad_state_change_callback_;
    AudioCodec* codec_ = nullptr;
    int frame_samples_ = 0;
    bool is_speaking_ = false;
    std::vector<int16_t> input_buffer_;
    std::mutex input_buffer_mutex_;
    std::vector<int16_t> output_buffer_;

    void AudioProcessorTask();
};

#endif 