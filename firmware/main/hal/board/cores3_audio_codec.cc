#include "cores3_audio_codec.h"
#include <companion/muse_quiesce_diagnostic.hpp>
#include <esp_memory_utils.h>
#include <companion/diagnostics.h>
#include <companion/audio_owner.hpp>
namespace {
void companion_codec_stage(companion::diagnostic::StageCode stage){
    if(companion::CurrentAudioOwner()==companion::AudioOwner::Mira)
        companion::diagnostic::Stage(companion::diagnostic::Task::Codec,stage);
}
void companion_codec_fault(companion::diagnostic::FaultCode fault,int32_t status){
    if(status!=0 && companion::CurrentAudioOwner()==companion::AudioOwner::Mira){
        companion::diagnostic::Fault(companion::diagnostic::Task::Codec,fault,status);
        companion::diagnostic::Sample(companion::diagnostic::Task::Codec);
    }
}
}

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/i2s_tdm.h>

#define TAG "CoreS3AudioCodec"

CoreS3AudioCodec::CoreS3AudioCodec(void* i2c_master_handle, int input_sample_rate, int output_sample_rate,
    gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
    uint8_t aw88298_addr, uint8_t es7210_addr, bool input_reference) {
    duplex_ = true; // 是否双工
    input_reference_ = input_reference; // 是否使用参考输入，实现回声消除
    input_channels_ = input_reference_ ? 2 : 1; // 输入通道数
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;
    input_gain_ = 60;

    CreateDuplexChannels(mclk, bclk, ws, dout, din);

    // Do initialize of related interface: data_if, ctrl_if and gpio_if
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = rx_handle_,
        .tx_handle = tx_handle_,
    };
    data_if_ = audio_codec_new_i2s_data(&i2s_cfg);
    assert(data_if_ != NULL);

    // Audio Output(Speaker)
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = (i2c_port_t)1,
        .addr = aw88298_addr,
        .bus_handle = i2c_master_handle,
    };
    out_ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(out_ctrl_if_ != NULL);

    gpio_if_ = audio_codec_new_gpio();
    assert(gpio_if_ != NULL);

    aw88298_codec_cfg_t aw88298_cfg = {};
    aw88298_cfg.ctrl_if = out_ctrl_if_;
    aw88298_cfg.gpio_if = gpio_if_;
    aw88298_cfg.reset_pin = GPIO_NUM_NC;
    aw88298_cfg.hw_gain.pa_voltage = 5.0;
    aw88298_cfg.hw_gain.codec_dac_voltage = 3.3;
    aw88298_cfg.hw_gain.pa_gain = 1;
    out_codec_if_ = aw88298_codec_new(&aw88298_cfg);
    assert(out_codec_if_ != NULL);

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = out_codec_if_,
        .data_if = data_if_,
    };
    output_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(output_dev_ != NULL);

    // Audio Input(Microphone)
    i2c_cfg.addr = es7210_addr;
    in_ctrl_if_ = audio_codec_new_i2c_ctrl(&i2c_cfg);
    assert(in_ctrl_if_ != NULL);

    es7210_codec_cfg_t es7210_cfg = {};
    es7210_cfg.ctrl_if = in_ctrl_if_;
    es7210_cfg.mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3;
    in_codec_if_ = es7210_codec_new(&es7210_cfg);
    assert(in_codec_if_ != NULL);

    dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
    dev_cfg.codec_if = in_codec_if_;
    input_dev_ = esp_codec_dev_new(&dev_cfg);
    assert(input_dev_ != NULL);

    ESP_LOGI(TAG, "CoreS3AudioCodec initialized");
}

CoreS3AudioCodec::~CoreS3AudioCodec() {
    // Cold instances are program-lifetime. Never synthesize recovery by freeing
    // resources from a partially initialized/retained diagnostic binding.
    if(ColdOutputOnlyInstance())return;
    // Never free ISR user_data while its channel can invoke our callback.
    if (direct_callback_ready_) {
        auto stopped = i2s_channel_disable(tx_handle_);
        ESP_ERROR_CHECK(stopped == ESP_ERR_INVALID_STATE ? ESP_OK : stopped);
        i2s_event_callbacks_t none = {};
        // Registration rejects RUNNING; success independently confirms no TX ISR.
        ESP_ERROR_CHECK(i2s_channel_register_event_callback(tx_handle_, &none, nullptr));
        direct_callback_ready_ = false;
    }

    ESP_ERROR_CHECK(esp_codec_dev_close(output_dev_));
    esp_codec_dev_delete(output_dev_);
    ESP_ERROR_CHECK(esp_codec_dev_close(input_dev_));
    esp_codec_dev_delete(input_dev_);

    audio_codec_delete_codec_if(in_codec_if_);
    audio_codec_delete_ctrl_if(in_ctrl_if_);
    audio_codec_delete_codec_if(out_codec_if_);
    audio_codec_delete_ctrl_if(out_ctrl_if_);
    audio_codec_delete_gpio_if(gpio_if_);
    audio_codec_delete_data_if(data_if_);
}

void CoreS3AudioCodec::CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din) {
    assert(input_sample_rate_ == output_sample_rate_);

    ESP_LOGI(TAG, "Audio IOs: mclk: %d, bclk: %d, ws: %d, dout: %d, din: %d", mclk, bclk, ws, dout, din);

    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM,
        .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)output_sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = true,
            .big_endian = false,
            .bit_order_lsb = false
        },
        .gpio_cfg = {
            .mclk = mclk,
            .bclk = bclk,
            .ws = ws,
            .dout = dout,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };

    i2s_tdm_config_t tdm_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)input_sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            .bclk_div = 8,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = i2s_tdm_slot_mask_t(I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3),
            .ws_width = I2S_TDM_AUTO_WS_WIDTH,
            .ws_pol = false,
            .bit_shift = true,
            .left_align = false,
            .big_endian = false,
            .bit_order_lsb = false,
            .skip_mask = false,
            .total_slot = I2S_TDM_AUTO_SLOT_NUM
        },
        .gpio_cfg = {
            .mclk = mclk,
            .bclk = bclk,
            .ws = ws,
            .dout = I2S_GPIO_UNUSED,
            .din = din,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_tdm_mode(rx_handle_, &tdm_cfg));
    // Normal behavior stays available if internal ISR context cannot be bound.
    if (esp_ptr_internal(this)) {
        i2s_event_callbacks_t callbacks = {};
        callbacks.on_sent = DirectOnSent;
        direct_callback_ready_ = i2s_channel_register_event_callback(tx_handle_, &callbacks, this) == ESP_OK;
    }
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));
    ESP_LOGI(TAG, "Duplex channels created");
}

void CoreS3AudioCodec::Start(){if(!ColdOutputOnlyInstance())AudioCodec::Start();}

void CoreS3AudioCodec::SetOutputVolume(int volume) {
    if(ColdOutputOnlyInstance())return;
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);RefreshHardwareBinding();
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
    // The mutex is recursive: driver callbacks must not change amplifier
    // state while a stopped-DMA transaction is still on the stack.
    if(muse_bound_&&(muse_quiescent_||muse_discarding_))return;
    struct Active{bool&flag;bool previous;explicit Active(bool&f):flag(f),previous(f){flag=true;}~Active(){flag=previous;}} active(muse_volume_active_);
#endif
    ESP_ERROR_CHECK(esp_codec_dev_set_out_vol(output_dev_, volume));
    AudioCodec::SetOutputVolume(volume);
}

void CoreS3AudioCodec::EnableInput(bool enable) {
    if(ColdOutputOnlyInstance())return;
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    RefreshHardwareBinding();
    if (direct_generation_ && !direct_call_) return;
    if (enable == input_enabled_) {
        return;
    }
    if (enable) {
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 2,
            .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),
            .sample_rate = (uint32_t)output_sample_rate_,
            .mclk_multiple = 0,
        };
        if (input_reference_) {
            fs.channel_mask |= ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1);
        }
        // A new open attempt invalidates earlier stop/close receipts, even on failure.
        direct_input_stopped_ = direct_input_closed_ = false;
        // RX format setup can restart paired TX for its clock without opening its codec.
        direct_output_stopped_ = false;
        companion_codec_stage(companion::diagnostic::StageCode::DriverInputOpen);
        {
            auto status=esp_codec_dev_open(input_dev_, &fs);
            companion_codec_fault(companion::diagnostic::FaultCode::DriverInputOpen,static_cast<int32_t>(status));
            ESP_ERROR_CHECK(status);
        }
        {
            auto status=esp_codec_dev_set_in_channel_gain(input_dev_, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), input_gain_);
            companion_codec_fault(companion::diagnostic::FaultCode::DriverInputGain,static_cast<int32_t>(status));
            ESP_ERROR_CHECK(status);
        }
    } else {
        companion_codec_stage(companion::diagnostic::StageCode::DriverInputClose);
        {
            auto status=esp_codec_dev_close(input_dev_);
            companion_codec_fault(companion::diagnostic::FaultCode::DriverInputClose,static_cast<int32_t>(status));
            ESP_ERROR_CHECK(status);
        }
    }
    AudioCodec::EnableInput(enable);
}

void CoreS3AudioCodec::EnableOutput(bool enable) {
    if(ColdOutputOnlyInstance())return;
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    RefreshHardwareBinding();
    if (direct_generation_ && !direct_call_) return;
    if (enable == output_enabled_) {
        return;
    }
    if (enable) {
        // Play 16bit 1 channel
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 1,
            .channel_mask = 0,
            .sample_rate = (uint32_t)output_sample_rate_,
            .mclk_multiple = 0,
        };
        // A new open attempt invalidates earlier stop/close receipts, even on failure.
        direct_output_stopped_ = direct_output_closed_ = false;
        companion_codec_stage(companion::diagnostic::StageCode::DriverOutputOpen);
        {
            auto status=esp_codec_dev_open(output_dev_, &fs);
            companion_codec_fault(companion::diagnostic::FaultCode::DriverOutputOpen,static_cast<int32_t>(status));
            ESP_ERROR_CHECK(status);
        }
        {
            auto status=esp_codec_dev_set_out_vol(output_dev_, output_volume_);
            companion_codec_fault(companion::diagnostic::FaultCode::DriverOutputVolume,static_cast<int32_t>(status));
            ESP_ERROR_CHECK(status);
        }
    } else {
        companion_codec_stage(companion::diagnostic::StageCode::DriverOutputClose);
        {
            auto status=esp_codec_dev_close(output_dev_);
            companion_codec_fault(companion::diagnostic::FaultCode::DriverOutputClose,static_cast<int32_t>(status));
            ESP_ERROR_CHECK(status);
        }
    }
    AudioCodec::EnableOutput(enable);
}

int CoreS3AudioCodec::Read(int16_t* dest, int samples) {
    if(ColdOutputOnlyInstance())return 0;
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    RefreshHardwareBinding();
    if (direct_generation_ && !direct_call_) return 0;
    if (input_enabled_) {
        companion_codec_stage(companion::diagnostic::StageCode::DriverRead);
        {
            auto status=esp_codec_dev_read(input_dev_, (void*)dest, samples * sizeof(int16_t));
            companion_codec_fault(companion::diagnostic::FaultCode::DriverRead,static_cast<int32_t>(status));
            ESP_ERROR_CHECK_WITHOUT_ABORT(status);
            if(status!=ESP_CODEC_DEV_OK)return 0;
        }
    }
    return input_enabled_ ? samples : 0;
}

int CoreS3AudioCodec::Write(const int16_t* data, int samples) {
    if(ColdOutputOnlyInstance())return 0;
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    RefreshHardwareBinding();
    if (direct_generation_ && (!direct_call_ || direct_draining_)) return 0;
    if (output_enabled_) {
        companion_codec_stage(companion::diagnostic::StageCode::DriverWrite);
        {
            auto status=esp_codec_dev_write(output_dev_, (void*)data, samples * sizeof(int16_t));
            companion_codec_fault(companion::diagnostic::FaultCode::DriverWrite,static_cast<int32_t>(status));
            ESP_ERROR_CHECK_WITHOUT_ABORT(status);
            if(status!=ESP_OK)return 0;
        }
        return samples;
    }
    return 0;
}

bool CoreS3AudioCodec::EnableInputChecked(bool enable) {
    if(ColdOutputOnlyInstance())return false;
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    RefreshHardwareBinding();
    if (direct_generation_ && !direct_call_) return false;
    if (enable == input_enabled_) {
        return true;
    }
    if (enable) {
        AudioCodec::EnableInput(true); // Retain owned state on partial open failure until checked close succeeds.
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 2,
            .channel_mask = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),
            .sample_rate = (uint32_t)output_sample_rate_,
            .mclk_multiple = 0,
        };
        if (input_reference_) {
            fs.channel_mask |= ESP_CODEC_DEV_MAKE_CHANNEL_MASK(1);
        }
        // A new open attempt invalidates earlier stop/close receipts, even on failure.
        direct_input_stopped_ = direct_input_closed_ = false;
        // RX format setup can restart paired TX for its clock without opening its codec.
        direct_output_stopped_ = false;
        companion_codec_stage(companion::diagnostic::StageCode::DriverInputOpen);
        {
            auto status=esp_codec_dev_open(input_dev_, &fs);
            companion_codec_fault(companion::diagnostic::FaultCode::DriverInputOpen,static_cast<int32_t>(status));
            ESP_ERROR_CHECK_WITHOUT_ABORT(status);
            if(status!=ESP_CODEC_DEV_OK)return false;
        }
        {
            auto status=esp_codec_dev_set_in_channel_gain(input_dev_, ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0), input_gain_);
            companion_codec_fault(companion::diagnostic::FaultCode::DriverInputGain,static_cast<int32_t>(status));
            ESP_ERROR_CHECK_WITHOUT_ABORT(status);
            if(status!=ESP_CODEC_DEV_OK)return false;
        }
    } else {
        companion_codec_stage(companion::diagnostic::StageCode::DriverInputClose);
        {
            auto status=esp_codec_dev_close(input_dev_);
            companion_codec_fault(companion::diagnostic::FaultCode::DriverInputClose,static_cast<int32_t>(status));
            ESP_ERROR_CHECK_WITHOUT_ABORT(status);
            if(status!=ESP_CODEC_DEV_OK)return false;
        }
    }
    AudioCodec::EnableInput(enable);
    return true;
}
bool CoreS3AudioCodec::EnableOutputChecked(bool enable) {
    if(ColdOutputOnlyInstance())return false;
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    RefreshHardwareBinding();
    if (direct_generation_ && !direct_call_) return false;
    if (enable == output_enabled_) {
        return true;
    }
    if (enable) {
        AudioCodec::EnableOutput(true); // Retain owned state on partial open failure until checked close succeeds.
        // Play 16bit 1 channel
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel = 1,
            .channel_mask = 0,
            .sample_rate = (uint32_t)output_sample_rate_,
            .mclk_multiple = 0,
        };
        // A new open attempt invalidates earlier stop/close receipts, even on failure.
        direct_output_stopped_ = direct_output_closed_ = false;
        companion_codec_stage(companion::diagnostic::StageCode::DriverOutputOpen);
        {
            auto status=esp_codec_dev_open(output_dev_, &fs);
            companion_codec_fault(companion::diagnostic::FaultCode::DriverOutputOpen,static_cast<int32_t>(status));
            ESP_ERROR_CHECK_WITHOUT_ABORT(status);
            if(status!=ESP_CODEC_DEV_OK)return false;
        }
        {
            auto status=esp_codec_dev_set_out_vol(output_dev_, output_volume_);
            companion_codec_fault(companion::diagnostic::FaultCode::DriverOutputVolume,static_cast<int32_t>(status));
            ESP_ERROR_CHECK_WITHOUT_ABORT(status);
            if(status!=ESP_CODEC_DEV_OK)return false;
        }
    } else {
        companion_codec_stage(companion::diagnostic::StageCode::DriverOutputClose);
        {
            auto status=esp_codec_dev_close(output_dev_);
            companion_codec_fault(companion::diagnostic::FaultCode::DriverOutputClose,static_cast<int32_t>(status));
            ESP_ERROR_CHECK_WITHOUT_ABORT(status);
            if(status!=ESP_CODEC_DEV_OK)return false;
        }
    }
    AudioCodec::EnableOutput(enable);
    return true;
}

// No authority is issued here: caller already owns the approved HAL codec lease.
bool CoreS3AudioCodec::DirectUsbCodecAcquire(uint64_t generation) {
    if(ColdOutputOnlyInstance())return false;
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    RefreshHardwareBinding();
    if (!direct_callback_ready_ || !generation || generation <= direct_last_generation_ || direct_generation_ || input_enabled_ || output_enabled_) return false;
    if (!companion::AcquireAudio(companion::AudioOwner::DirectUsb,companion::AudioPurpose::DirectUsbSession,generation,direct_common_lease_)) return false;
    direct_last_generation_ = generation;
    direct_generation_ = generation;
    direct_input_stopped_ = direct_output_stopped_ = false;
    direct_input_closed_ = direct_output_closed_ = false;
    portENTER_CRITICAL(&direct_mux_);
    direct_draining_ = false;
    portEXIT_CRITICAL(&direct_mux_);
    return true;
}
bool CoreS3AudioCodec::ScopedEnableInputChecked(uint64_t generation, bool enable) {
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    if (!generation || generation != direct_generation_) return false;
    if (!enable) return DirectStopChecked(generation,false);
    if (!companion::CommonAudioAuthority().Current(direct_common_lease_)) return false;
    if (direct_input_stopped_ && input_enabled_) return false;
    DirectCallScope scoped(direct_call_);bool ok = EnableInputChecked(true);
    direct_input_stopped_ = direct_input_closed_ = false; return ok;
}
bool CoreS3AudioCodec::ScopedEnableOutputChecked(uint64_t generation, bool enable) {
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    if (!generation || generation != direct_generation_ || direct_draining_) return false;
    if (!enable) return DirectStopChecked(generation,true);
    if (!companion::CommonAudioAuthority().Current(direct_common_lease_)) return false;
    if (direct_output_stopped_ && output_enabled_) return false;
    DirectCallScope scoped(direct_call_);bool ok = EnableOutputChecked(true);
    direct_output_stopped_ = direct_output_closed_ = false; return ok;
}
bool CoreS3AudioCodec::ScopedInputDataChecked(uint64_t generation, std::vector<int16_t>& data) {
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    if (!generation || generation != direct_generation_ || direct_input_stopped_) return false;
    if (!companion::CommonAudioAuthority().Current(direct_common_lease_)) return false;
    DirectCallScope scoped(direct_call_);return InputDataChecked(data);
}
bool CoreS3AudioCodec::ScopedOutputDataChecked(uint64_t generation, std::vector<int16_t>& data) {
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    if (!generation || generation != direct_generation_ || direct_output_stopped_ || direct_draining_) return false;
    if (!companion::CommonAudioAuthority().Current(direct_common_lease_)) return false;
    DirectCallScope scoped(direct_call_);return OutputDataChecked(data);
}
bool CoreS3AudioCodec::ScopedDrainBegin(uint64_t generation) {
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    if (!generation || generation != direct_generation_ || !output_enabled_ || direct_output_stopped_ || direct_draining_) return false;
    portENTER_CRITICAL(&direct_mux_);
    bool ok = direct_fence_generation_ != UINT64_MAX && direct_fence_.Begin(++direct_fence_generation_);
    if (ok) direct_draining_ = true;
    portEXIT_CRITICAL(&direct_mux_);
    return ok;
}
AudioCodec::DirectUsbDrain CoreS3AudioCodec::ScopedDrainState(uint64_t generation) {
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    if (!generation || generation != direct_generation_ || !direct_draining_) return DirectUsbDrain::Unavailable;
    portENTER_CRITICAL(&direct_mux_);
    auto state = direct_fence_.state();
    portEXIT_CRITICAL(&direct_mux_);
    if (state == codec_drain::DmaSentFence::State::Sent) return DirectUsbDrain::Sent;
    return state == codec_drain::DmaSentFence::State::Pending ? DirectUsbDrain::Pending : DirectUsbDrain::Failed;
}
bool IRAM_ATTR CoreS3AudioCodec::DirectOnSent(i2s_chan_handle_t handle, i2s_event_data_t* event, void* context) {
    auto* codec = static_cast<CoreS3AudioCodec*>(context);
    if (!codec || handle != codec->tx_handle_) return false;
    portENTER_CRITICAL_ISR(&codec->direct_mux_);
    if (codec->direct_draining_ && event) codec->direct_fence_.OnSent(codec->direct_fence_generation_, reinterpret_cast<uintptr_t>(event->dma_buf), event->size);
    portEXIT_CRITICAL_ISR(&codec->direct_mux_);
    return false;
}
bool CoreS3AudioCodec::DirectStopChecked(uint64_t generation, bool output,int* error,bool* channel_failure,uint8_t* attempts,const companion::AudioLease* expected) {
    if(error)*error=0;
    if(channel_failure)*channel_failure=false;
    if(attempts)*attempts=0;
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
    struct Active{bool&flag;bool previous;explicit Active(bool&f):flag(f),previous(f){flag=true;}~Active(){flag=previous;}} active(muse_stop_active_);
#endif
    if (!generation || generation != direct_generation_) return false;
    if(expected&&!companion::CommonAudioAuthority().Holds(*expected))return false;
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
    if(muse_cold_)return MuseColdStop(output,error,attempts);
#endif
    bool& stopped = output ? direct_output_stopped_ : direct_input_stopped_;
    bool& closed = output ? direct_output_closed_ : direct_input_closed_;
    if (!stopped) {
        if(attempts)*attempts|=1;
        auto status = i2s_channel_disable(output ? tx_handle_ : rx_handle_);
        // INVALID_STATE is not a fresh proof; retain until an actual owner knows.
        if (status != ESP_OK) { if(error)*error=status;if(channel_failure)*channel_failure=true;companion::RevokeAudio(direct_common_lease_); return false; }
        stopped = true;
    }
    if(expected&&!companion::CommonAudioAuthority().Holds(*expected))return false;
    if (!closed) {
        if(attempts)*attempts|=2;
        auto status = esp_codec_dev_close(output ? output_dev_ : input_dev_);
        if (status != ESP_CODEC_DEV_OK) { if(error)*error=status;companion::RevokeAudio(direct_common_lease_); return false; }
        closed = true;
    }
    if(expected&&!companion::CommonAudioAuthority().Holds(*expected))return false;
    if (output) {
        AudioCodec::EnableOutput(false);
        portENTER_CRITICAL(&direct_mux_);
        direct_fence_.Cancel(direct_fence_generation_); direct_draining_ = false;
        portEXIT_CRITICAL(&direct_mux_);
    } else AudioCodec::EnableInput(false);
    return true;
}
bool CoreS3AudioCodec::DirectUsbStopInputChecked(uint64_t generation) { std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);return !IsMuseBinding()&&DirectStopChecked(generation, false); }
bool CoreS3AudioCodec::DirectUsbStopOutputChecked(uint64_t generation) { std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);return !IsMuseBinding()&&DirectStopChecked(generation, true); }
bool CoreS3AudioCodec::DirectUsbCodecRelease(uint64_t generation) {
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    if(IsMuseBinding())return false;
    if (!generation || generation != direct_generation_) return false;
    companion::RevokeAudio(direct_common_lease_);
    if (!direct_input_stopped_ || !direct_output_stopped_ || !direct_input_closed_ || !direct_output_closed_ || input_enabled_ || output_enabled_) return false;
    if (!companion::ReleaseAudio(direct_common_lease_,true)) return false;
    direct_generation_ = 0; return true;
}

// Public USB entrypoints cannot operate a Muse binding by guessing its number.
bool CoreS3AudioCodec::DirectUsbEnableInputChecked(uint64_t g,bool enable){std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);return !IsMuseBinding()&&ScopedEnableInputChecked(g,enable);}
bool CoreS3AudioCodec::DirectUsbEnableOutputChecked(uint64_t g,bool enable){std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);return !IsMuseBinding()&&ScopedEnableOutputChecked(g,enable);}
bool CoreS3AudioCodec::DirectUsbInputDataChecked(uint64_t g,std::vector<int16_t>& data){std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);return !IsMuseBinding()&&ScopedInputDataChecked(g,data);}
bool CoreS3AudioCodec::DirectUsbOutputDataChecked(uint64_t g,std::vector<int16_t>& data){std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);return !IsMuseBinding()&&ScopedOutputDataChecked(g,data);}
bool CoreS3AudioCodec::DirectUsbDrainBegin(uint64_t g){std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);return !IsMuseBinding()&&ScopedDrainBegin(g);}
AudioCodec::DirectUsbDrain CoreS3AudioCodec::DirectUsbDrainState(uint64_t g){std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);return IsMuseBinding()?DirectUsbDrain::Unavailable:ScopedDrainState(g);}

#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
bool CoreS3AudioCodec::MuseMatch(const companion::AudioLease& lease)const{
    return muse_bound_&&lease.owner()==companion::AudioOwner::Muse&&lease.purpose()==companion::AudioPurpose::MusePushToTalk&&
        lease.generation()==direct_common_lease_.generation()&&lease.epoch()==direct_common_lease_.epoch()&&
        companion::CommonAudioAuthority().Holds(lease);
}
bool CoreS3AudioCodec::MuseCodecAcquire(const companion::AudioLease& lease){
    if(ColdOutputOnlyInstance())return false;
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);RefreshHardwareBinding();
    if(!direct_callback_ready_||direct_generation_||input_enabled_||output_enabled_||
       input_sample_rate_!=24000||output_sample_rate_!=24000||
       input_channels_!=(input_reference_?2:1)||output_channels_!=1||
       lease.owner()!=companion::AudioOwner::Muse||lease.purpose()!=companion::AudioPurpose::MusePushToTalk||
       !lease.epoch()||!companion::CommonAudioAuthority().Current(lease))return false;
    direct_common_lease_=lease;direct_generation_=lease.generation();muse_bound_=true;muse_retired_=false;
    muse_quiescent_=muse_quiescing_=muse_quiesce_retained_=false;muse_last_quiesce_ticket_=0;
    muse_drain_ticket_=muse_last_drain_ticket_=muse_last_discard_ticket_=muse_last_reset_ticket_=0;
    portENTER_CRITICAL(&direct_mux_);direct_draining_=false;portEXIT_CRITICAL(&direct_mux_);return true;
}
bool CoreS3AudioCodec::MuseEnableInputChecked(const companion::AudioLease& lease,bool enable){
    if(ColdOutputOnlyInstance())return false;
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    if(!MuseMatch(lease)||muse_retired_||muse_discarding_||muse_quiescing_||(enable&&muse_quiescent_))return false;
    const bool okay=ScopedEnableInputChecked(direct_generation_,enable)&&(!enable||companion::CommonAudioAuthority().Current(lease));if(!okay)companion::RevokeAudio(lease);return okay;
}
bool CoreS3AudioCodec::MuseEnableOutputChecked(const companion::AudioLease& lease,bool enable){
    if(ColdOutputOnlyInstance())return false;
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    if(!MuseMatch(lease)||muse_retired_||muse_discarding_||muse_quiescing_||(enable&&muse_quiescent_))return false;
    const bool okay=ScopedEnableOutputChecked(direct_generation_,enable)&&(!enable||companion::CommonAudioAuthority().Current(lease));if(!okay)companion::RevokeAudio(lease);return okay;
}
bool CoreS3AudioCodec::MuseInputDataChecked(const companion::AudioLease& lease,std::vector<int16_t>& data){
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    if(data.empty()||data.size()>MuseRawSampleLimit)return false;
    if(!MuseMatch(lease)||muse_retired_||muse_quiescent_){for(volatile auto& sample:data)sample=0;return false;}
    const bool okay=ScopedInputDataChecked(direct_generation_,data)&&companion::CommonAudioAuthority().Current(lease);
    if(!okay){for(volatile auto& sample:data)sample=0;companion::RevokeAudio(lease);}return okay;
}
bool CoreS3AudioCodec::MuseOutputDataChecked(const companion::AudioLease& lease,std::vector<int16_t>& data){
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    if(data.empty()||data.size()>MuseRawSampleLimit||!MuseMatch(lease)||muse_retired_||muse_quiescent_)return false;
    const bool okay=ScopedOutputDataChecked(direct_generation_,data)&&companion::CommonAudioAuthority().Current(lease);if(!okay)companion::RevokeAudio(lease);return okay;
}
bool CoreS3AudioCodec::MuseDrainBegin(const companion::AudioLease& lease,uint64_t ticket){
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    if(!MuseMatch(lease)||muse_retired_||!companion::CommonAudioAuthority().Current(lease)||!ticket||ticket<=muse_last_drain_ticket_)return false;
    if(!ScopedDrainBegin(direct_generation_))return false;
    muse_drain_ticket_=muse_last_drain_ticket_=ticket;return true;
}
CoreS3AudioCodec::MuseDrain CoreS3AudioCodec::MuseDrainState(const companion::AudioLease& lease,uint64_t ticket){
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    if(!MuseMatch(lease)||muse_retired_||!ticket||ticket!=muse_drain_ticket_)return MuseDrain::Unavailable;
    switch(ScopedDrainState(direct_generation_)){
        case DirectUsbDrain::Pending:return MuseDrain::Pending;
        case DirectUsbDrain::Sent:return MuseDrain::Sent;
        case DirectUsbDrain::Failed:return MuseDrain::Failed;
        default:return MuseDrain::Unavailable;
    }
}
CoreS3AudioCodec::MuseDiscard CoreS3AudioCodec::MuseDiscardOutputChecked(const companion::AudioLease& lease,uint64_t ticket){
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    // Exclude callback reentry before consuming a discard ticket. Terminal
    // quiesce uses the private helper after establishing its own exclusion.
    return (muse_volume_active_||muse_quiescing_||muse_quiescent_)?MuseDiscard::Unavailable:MuseDiscardStopped(lease,ticket);
}
CoreS3AudioCodec::MuseDiscard CoreS3AudioCodec::MuseDiscardStopped(const companion::AudioLease& lease,uint64_t ticket,MuseDiscardEvidence* detail){
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);const auto original=lease;
    // Both directions: input format setup can restart paired TX for its clock.
    const auto stopped=[&]{return MuseMatch(original)&&!muse_retired_&&MuseInputQuiescentProof()&&MuseOutputQuiescentProof()&&!input_enabled_&&!output_enabled_;};
    if(muse_discarding_||!ticket||ticket<=muse_last_discard_ticket_||!stopped())return MuseDiscard::Unavailable;
    struct Busy{bool& flag;explicit Busy(bool& f):flag(f){flag=true;}~Busy(){flag=false;}} busy(muse_discarding_);
    muse_last_discard_ticket_=ticket; // every attempted request is single-use, including failures
    const auto failed=[&]{if(muse_cold_){muse_cold_output_confirmed_=false;muse_quiescent_=muse_quiesce_retained_=true;}companion::RevokeAudio(original);return MuseDiscard::Failed;};
    i2s_chan_info_t info{};
    if(detail)detail->phase=MuseDiscardPhase::BeforeInfo;
    const auto info_status=i2s_channel_get_info(tx_handle_,&info);
    if(detail){detail->status=info_status;detail->total_dma_bytes=info.total_dma_buf_size;detail->direction=info.dir;detail->mode=info.mode;detail->enabled_before=info.is_enabled;}
    if(info_status==ESP_OK&&info.is_enabled)direct_output_stopped_=false; // contradictory hardware evidence needs a fresh stop
    if(info_status!=ESP_OK||!stopped()||info.dir!=I2S_DIR_TX||info.mode!=I2S_COMM_MODE_STD||
       info.is_enabled||!info.total_dma_buf_size||info.total_dma_buf_size%4||info.total_dma_buf_size>MuseDiscardByteLimit)return failed();
    // Normal stop resets the pinned preload cursor; cold construction owns a
    // fresh never-preloaded channel. Neither permits replay after partial write.
    const std::array<unsigned char,256> zeros{};
    for(size_t offset=0;offset<info.total_dma_buf_size;){
        const size_t count=std::min(zeros.size(),size_t(info.total_dma_buf_size)-offset);size_t loaded=0;
        if(!stopped())return failed();
        if(detail){detail->phase=MuseDiscardPhase::Preload;detail->requested=count;detail->loaded=0;}
        const auto status=i2s_channel_preload_data(tx_handle_,zeros.data(),count,&loaded);
        if(detail){detail->status=status;detail->loaded=loaded;detail->total_loaded+=loaded;}
        if(status!=ESP_OK||loaded!=count||!stopped())return failed();
        offset+=count;
    }
    i2s_chan_info_t after{};
    if(detail)detail->phase=MuseDiscardPhase::AfterInfo;
    const auto after_status=i2s_channel_get_info(tx_handle_,&after);
    if(detail){detail->status=after_status;detail->enabled_after=after.is_enabled;}
    if(after_status==ESP_OK&&after.is_enabled)direct_output_stopped_=false;
    if(after_status!=ESP_OK||!stopped()||after.dir!=info.dir||after.mode!=info.mode||
       after.is_enabled||after.total_dma_buf_size!=info.total_dma_buf_size)return failed();
    return MuseDiscard::StoppedDmaCleared; // not DMA-sent, acoustic silence or native authorization
}
CoreS3AudioCodec::MuseQuiesceEvidence CoreS3AudioCodec::MuseQuiesceForRetirementChecked(const companion::AudioLease& lease,uint64_t ticket,companion::muse::QuiesceDiagnostic* diagnostic){
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);MuseQuiesceEvidence result;const auto captured=lease;
    using Reason=companion::muse::QuiesceDiagnostic::Reason;Reason reason=Reason::NotObserved;
    if(diagnostic&&!diagnostic->Begin(captured,ticket))return result;
    const auto finish=[&]{if(diagnostic){const bool related=reason!=Reason::BindingUnavailable;diagnostic->Finish(ticket,result,reason,MuseMatch(diagnostic->lease_),related&&muse_quiesce_retained_,related&&muse_quiescent_);}};
    struct Publish{const decltype(finish)&call;~Publish(){call();}} publish{finish};
    if(!MuseMatch(captured)){reason=Reason::BindingUnavailable;return result;}
    if(muse_retired_){reason=Reason::Retired;return result;}
    if(direct_call_||muse_stop_active_||muse_volume_active_||muse_quiescing_||muse_discarding_){reason=Reason::ActiveOperation;return result;}
    if(muse_quiescent_||muse_last_quiesce_ticket_){reason=Reason::AlreadyAttempted;return result;}
    if(!ticket){reason=Reason::InvalidTicket;return result;}
    if(muse_last_discard_ticket_){reason=Reason::PriorDiscard;return result;}
    if(!codec_quiet::Available(out_codec_if_)){reason=Reason::MissingInterface;return result;}
    reason=Reason::Attempted;
    muse_last_quiesce_ticket_=ticket;muse_quiescent_=muse_quiesce_retained_=true;companion::RevokeAudio(captured);
    struct Busy{bool&flag;explicit Busy(bool&f):flag(f){flag=true;}~Busy(){flag=false;}} busy(muse_quiescing_);
    const auto* original=out_codec_if_;const auto current=[&]{const bool match=MuseMatch(captured)&&!muse_retired_&&muse_quiescent_&&out_codec_if_==original;if(!match)result.binding_lost=true;return match;};
    result.outcome=MuseQuiesce::Failed;result.phase=MuseQuiescePhase::Amplifier;
    result.amplifier=codec_quiet::Quiesce(original,current);if(!result.amplifier.Complete())return result;
    result.input_never_created=muse_cold_&&MuseColdInputAbsent();
    result.output_never_started=muse_cold_;
    result.phase=MuseQuiescePhase::InputStop;
    if(!current()||!DirectStopChecked(direct_generation_,false,&result.stop_status,&result.channel_disable_failure,&result.input_stop_attempts,&captured)){(void)current();return result;}
    result.phase=MuseQuiescePhase::OutputStop;
    if(!current()||!DirectStopChecked(direct_generation_,true,&result.stop_status,&result.channel_disable_failure,&result.output_stop_attempts,&captured)){(void)current();return result;}
    result.phase=MuseQuiescePhase::DigitalDiscard;result.discard=MuseDiscardStopped(captured,ticket,&result.discard_detail);
    const bool still_bound=current();if(result.discard!=MuseDiscard::StoppedDmaCleared||!still_bound)return result;
    result.phase=MuseQuiescePhase::FinalReadback;result.amplifier=codec_quiet::Verify(original,current,result.amplifier);
    if(!result.amplifier.Complete()||!current())return result;
    muse_quiesce_retained_=false;result.phase=MuseQuiescePhase::Complete;result.outcome=MuseQuiesce::RegistersAndStoppedDma;return result;
}
CoreS3AudioCodec::MuseResetEvidence CoreS3AudioCodec::MuseResetAmplifierChecked(const companion::AudioLease& lease,uint64_t ticket){
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);MuseResetEvidence result;const auto captured=lease;
    auto*const port=muse_reset_port_;
    // Admission: every rejection happens before IO and leaves ticket and ownership untouched.
    if(ColdOutputOnlyInstance()||!port||!MuseMatch(captured)||muse_retired_||!companion::CommonAudioAuthority().Current(captured))return result;
    if(direct_call_||muse_stop_active_||muse_volume_active_||muse_quiescing_||muse_quiescent_||muse_quiesce_retained_||muse_discarding_)return result;
    if(!ticket||ticket<=muse_last_reset_ticket_||ticket<=muse_last_discard_ticket_||!codec_quiet::Available(out_codec_if_))return result;
    muse_last_reset_ticket_=ticket; // single use, including failures; the discard step below consumes the same value itself
    // quiescent+quiescing fence every Muse entry point (and nested callbacks) until success.
    muse_quiescent_=muse_quiesce_retained_=true;
    struct Busy{bool&flag;explicit Busy(bool&f):flag(f){flag=true;}~Busy(){flag=false;}} busy(muse_quiescing_);
    const auto* original=out_codec_if_;
    const auto current=[&]{const bool match=MuseMatch(captured)&&!muse_retired_&&muse_quiescent_&&out_codec_if_==original&&muse_reset_port_==port&&companion::CommonAudioAuthority().Current(captured);if(!match)result.binding_or_authority_lost=true;return match;};
    result.outcome=MuseReset::Failed;
    // A failure past this point never unfences: the amplifier may be mid-reset, so custody is retained and the owner must stay muted.
    const auto fail=[&]{companion::RevokeAudio(captured);return result;};
    result.phase=MuseResetPhase::Mute;
    result.amplifier=codec_quiet::Quiesce(original,current);if(!result.amplifier.Complete())return fail();
    result.phase=MuseResetPhase::InputStop;
    if(!current()||!DirectStopChecked(direct_generation_,false,&result.stop_status,&result.channel_disable_failure,&result.input_stop_attempts,&captured))return fail();
    result.phase=MuseResetPhase::OutputStop;
    if(!current()||!DirectStopChecked(direct_generation_,true,&result.stop_status,&result.channel_disable_failure,&result.output_stop_attempts,&captured))return fail();
    result.phase=MuseResetPhase::DigitalDiscard;
    result.discard=MuseDiscardStopped(captured,ticket,&result.discard_detail);
    if(result.discard!=MuseDiscard::StoppedDmaCleared||!current())return fail();
    result.phase=MuseResetPhase::Assert;result.assert_attempted=true;
    result.port_status=port->SetResetLine(false);if(result.port_status!=0||!current())return fail();
    port->DelayMs(MuseResetHoldMs);
    result.phase=MuseResetPhase::Release;result.release_attempted=true;
    result.port_status=port->SetResetLine(true);if(result.port_status!=0||!current())return fail();
    port->DelayMs(MuseResetSettleMs);
    // Reset returned amplifier registers to defaults; mute again and verify. The codec device is closed
    // by the stops above, so the next checked output enable reopens and reprograms it.
    result.phase=MuseResetPhase::Requiesce;
    result.amplifier=codec_quiet::Quiesce(original,current);if(!result.amplifier.Complete()||!current())return fail();
    muse_quiescent_=muse_quiesce_retained_=false;result.phase=MuseResetPhase::Complete;result.outcome=MuseReset::AmplifierResetMutedVerified;return result;
}
bool CoreS3AudioCodec::MuseStopInputChecked(const companion::AudioLease& lease){
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);return MuseMatch(lease)&&!muse_retired_&&!muse_quiescing_&&!(muse_cold_&&muse_stop_active_)&&DirectStopChecked(direct_generation_,false);
}
bool CoreS3AudioCodec::MuseStopOutputChecked(const companion::AudioLease& lease){
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);return MuseMatch(lease)&&!muse_retired_&&!muse_quiescing_&&!(muse_cold_&&muse_stop_active_)&&DirectStopChecked(direct_generation_,true);
}
bool CoreS3AudioCodec::MuseCodecRelease(const companion::AudioLease& lease){
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    if(!MuseMatch(lease)||muse_discarding_||muse_quiescing_||muse_quiesce_retained_)return false;
    companion::RevokeAudio(lease);
    if(!MuseInputQuiescentProof()||!MuseOutputQuiescentProof()||input_enabled_||output_enabled_)return false;
    // Do not release the original logical lease. Its owner still owes worker
    // join. Keep the legacy IO guard until that owner releases common custody.
    muse_retired_=true;return true;
}
#endif
