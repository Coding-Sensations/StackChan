#ifndef _BOX_AUDIO_CODEC_H
#define _BOX_AUDIO_CODEC_H

#include "audio_codec.h"
#include <companion/audio_owner.hpp>
#include <build_profiles/features.hpp>

#include <mutex>
#include <esp_attr.h>
#define CODEC_FENCE_IRAM IRAM_ATTR
#include "dma_sent_fence.hpp"
#undef CODEC_FENCE_IRAM
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>

class CoreS3AudioCodec : public AudioCodec {
private:
    companion::AudioLease direct_common_lease_;
    std::recursive_mutex direct_io_mutex_;
    portMUX_TYPE direct_mux_ = portMUX_INITIALIZER_UNLOCKED;
    codec_drain::DmaSentFence direct_fence_;
    uint64_t direct_generation_ = 0, direct_last_generation_ = 0, direct_fence_generation_ = 0;
    bool direct_callback_ready_ = false, direct_call_ = false, direct_draining_ = false;
    bool direct_input_stopped_ = false, direct_output_stopped_ = false;
    bool direct_input_closed_ = false, direct_output_closed_ = false;
    static bool IRAM_ATTR DirectOnSent(i2s_chan_handle_t, i2s_event_data_t*, void*);
    bool DirectStopChecked(uint64_t generation, bool output);
    // Shared checked IO, always fenced by the captured common lease. Public
    // USB and Muse entrypoints remain disjoint; no owner enum is substituted.
    bool ScopedEnableInputChecked(uint64_t,bool);
    bool ScopedEnableOutputChecked(uint64_t,bool);
    bool ScopedInputDataChecked(uint64_t,std::vector<int16_t>&);
    bool ScopedOutputDataChecked(uint64_t,std::vector<int16_t>&);
    bool ScopedDrainBegin(uint64_t);
    DirectUsbDrain ScopedDrainState(uint64_t);
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
    bool muse_bound_=false,muse_retired_=false,muse_discarding_=false;
    uint64_t muse_last_discard_ticket_=0;
    uint64_t muse_drain_ticket_=0,muse_last_drain_ticket_=0;
    bool MuseMatch(const companion::AudioLease&)const;
#endif
    bool IsMuseBinding()const {
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
        return muse_bound_;
#else
        return false;
#endif
    }
    void RefreshHardwareBinding() {
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
        // Keep the legacy-IO guard through the gap between checked hardware
        // release and the original owner's logical release. Never release it
        // on an unavailable/unknown close receipt.
        if(muse_bound_&&muse_retired_&&!companion::CommonAudioAuthority().Holds(direct_common_lease_)){
            direct_generation_=0;direct_common_lease_={};muse_bound_=muse_retired_=false;
            muse_drain_ticket_=muse_last_drain_ticket_=muse_last_discard_ticket_=0;
        }
#endif
    }


    const audio_codec_data_if_t* data_if_ = nullptr;
    const audio_codec_ctrl_if_t* out_ctrl_if_ = nullptr;
    const audio_codec_if_t* out_codec_if_ = nullptr;
    const audio_codec_ctrl_if_t* in_ctrl_if_ = nullptr;
    const audio_codec_if_t* in_codec_if_ = nullptr;
    const audio_codec_gpio_if_t* gpio_if_ = nullptr;

    esp_codec_dev_handle_t output_dev_ = nullptr;
    esp_codec_dev_handle_t input_dev_ = nullptr;

    void CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din);

    virtual int Read(int16_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;

public:
    CoreS3AudioCodec(void* i2c_master_handle, int input_sample_rate, int output_sample_rate,
        gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout, gpio_num_t din,
        uint8_t aw88298_addr, uint8_t es7210_addr, bool input_reference);
    virtual ~CoreS3AudioCodec();

    bool DirectUsbCodecAcquire(uint64_t generation) override;
    bool DirectUsbEnableInputChecked(uint64_t generation, bool enable) override;
    bool DirectUsbEnableOutputChecked(uint64_t generation, bool enable) override;
    bool DirectUsbInputDataChecked(uint64_t generation, std::vector<int16_t>& data) override;
    bool DirectUsbOutputDataChecked(uint64_t generation, std::vector<int16_t>& data) override;
    bool DirectUsbDrainBegin(uint64_t generation) override;
    DirectUsbDrain DirectUsbDrainState(uint64_t generation) override;
    bool DirectUsbStopInputChecked(uint64_t generation) override;
    bool DirectUsbStopOutputChecked(uint64_t generation) override;
    bool DirectUsbCodecRelease(uint64_t generation) override;

#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
    enum class MuseDrain { Unavailable, Pending, Sent, Failed };
    enum class MuseDiscard { Unavailable, Failed, StoppedDmaCleared };
    static constexpr size_t MuseDiscardByteLimit=16384;
    static constexpr size_t MuseRawSampleLimit=2048;
    // Resource/hardware binding only: not microphone/output consent. The
    // caller retains the exact original Muse lease through checked cleanup.
    bool MuseCodecAcquire(const companion::AudioLease&);
    bool MuseEnableInputChecked(const companion::AudioLease&,bool);
    bool MuseEnableOutputChecked(const companion::AudioLease&,bool);
    bool MuseInputDataChecked(const companion::AudioLease&,std::vector<int16_t>&);
    bool MuseOutputDataChecked(const companion::AudioLease&,std::vector<int16_t>&);
    bool MuseDrainBegin(const companion::AudioLease&,uint64_t ticket);
    MuseDrain MuseDrainState(const companion::AudioLease&,uint64_t ticket);
    // Digital stopped-DMA overwrite only, not acoustic completion or consent.
    MuseDiscard MuseDiscardOutputChecked(const companion::AudioLease&,uint64_t ticket);
    bool MuseStopInputChecked(const companion::AudioLease&);
    bool MuseStopOutputChecked(const companion::AudioLease&);
    bool MuseCodecRelease(const companion::AudioLease&);
#endif

    virtual void SetOutputVolume(int volume) override;
    virtual void EnableInput(bool enable) override;
    bool EnableInputChecked(bool enable) override;
    virtual void EnableOutput(bool enable) override;
    bool EnableOutputChecked(bool enable) override;
};

#endif // _BOX_AUDIO_CODEC_H
