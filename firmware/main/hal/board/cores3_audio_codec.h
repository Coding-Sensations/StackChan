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
#include "muse_aw88298_quiet.hpp"

#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
namespace companion::muse {class QuiesceDiagnostic;}
#endif

class CoreS3AudioCodec : public AudioCodec {
public:
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
    struct MuseColdOutputConfig {void* bus=nullptr;};
    enum class MuseColdStage {Unused,Binding,Channel,Format,Info,Control,Gpio,Amplifier,Quiet,Complete};
    enum class MuseColdFault {None,Rejected,Driver,NoObject,BindingLost,InvalidState,Amplifier};
    struct MuseColdEvidence {
        MuseColdStage stage=MuseColdStage::Unused;MuseColdFault fault=MuseColdFault::Rejected;int status=0;size_t dma_bytes=0;codec_quiet::Evidence amplifier;
        bool Complete()const{return stage==MuseColdStage::Complete&&fault==MuseColdFault::None;}
    };
    // No IO, program-lifetime retirement-only instance. No PCM/enable path.
    static CoreS3AudioCodec& MuseColdOutputOnly();
    MuseColdEvidence MuseColdInitialize(const companion::AudioLease&,const MuseColdOutputConfig&);
    enum class MuseDrain { Unavailable, Pending, Sent, Failed };
    enum class MuseDiscard { Unavailable, Failed, StoppedDmaCleared };
    static constexpr size_t MuseDiscardByteLimit=16384;
    static constexpr size_t MuseRawSampleLimit=2048;
    enum class MuseQuiesce {Unavailable,Failed,RegistersAndStoppedDma};
    enum class MuseQuiescePhase {None,Amplifier,InputStop,OutputStop,DigitalDiscard,FinalReadback,Complete};
    // Non-terminal amplifier hardware reset. Success means registers read back
    // muted after reset; it is not physical FIFO discard or audible silence.
    enum class MuseReset {Unavailable,Failed,AmplifierResetMutedVerified};
    enum class MuseResetPhase {None,Mute,InputStop,OutputStop,DigitalDiscard,Assert,Release,Requiesce,Complete};
    // Reset-line access is injected, never owned. Implementations must read-modify-write
    // only the AW88298 reset bit under their own bus lock and return 0 only when readback
    // shows the requested level with every other output bit unchanged.
    class MuseAmpResetPort {
    public:
        virtual ~MuseAmpResetPort()=default;
        virtual int SetResetLine(bool released)=0;
        virtual void DelayMs(uint32_t ms)=0;
    };
    // Timings are the values in the unused Aw9523::ResetAw88298; datasheet minimums are unverified.
    static constexpr uint32_t MuseResetHoldMs=10,MuseResetSettleMs=50;
    enum class MuseDiscardPhase {None,BeforeInfo,Preload,AfterInfo};
    struct MuseDiscardEvidence {MuseDiscardPhase phase=MuseDiscardPhase::None;int status=0;size_t requested=0,loaded=0,total_loaded=0,total_dma_bytes=0;int direction=0,mode=0;bool enabled_before=false,enabled_after=false;};
    struct MuseQuiesceEvidence {
        MuseQuiesce outcome=MuseQuiesce::Unavailable;MuseQuiescePhase phase=MuseQuiescePhase::None;
        codec_quiet::Evidence amplifier;int stop_status=0;bool channel_disable_failure=false;
        MuseDiscard discard=MuseDiscard::Unavailable;MuseDiscardEvidence discard_detail;
        uint8_t input_stop_attempts=0,output_stop_attempts=0;bool binding_lost=false,input_never_created=false,output_never_started=false;
    };
    struct MuseResetEvidence {
        MuseReset outcome=MuseReset::Unavailable;MuseResetPhase phase=MuseResetPhase::None;
        codec_quiet::Evidence amplifier;int stop_status=0,port_status=0;bool channel_disable_failure=false;
        MuseDiscard discard=MuseDiscard::Unavailable;MuseDiscardEvidence discard_detail;
        uint8_t input_stop_attempts=0,output_stop_attempts=0;
        bool assert_attempted=false,release_attempted=false,binding_or_authority_lost=false;
    };
#endif
private:
    struct DirectCallScope{bool& flag;bool previous;explicit DirectCallScope(bool&f):flag(f),previous(f){flag=true;}~DirectCallScope(){flag=previous;}};
    companion::AudioLease direct_common_lease_;
    std::recursive_mutex direct_io_mutex_;
    portMUX_TYPE direct_mux_ = portMUX_INITIALIZER_UNLOCKED;
    codec_drain::DmaSentFence direct_fence_;
    uint64_t direct_generation_ = 0, direct_last_generation_ = 0, direct_fence_generation_ = 0;
    bool direct_callback_ready_ = false, direct_call_ = false, direct_draining_ = false;
    bool direct_input_stopped_ = false, direct_output_stopped_ = false;
    bool direct_input_closed_ = false, direct_output_closed_ = false;
    static bool IRAM_ATTR DirectOnSent(i2s_chan_handle_t, i2s_event_data_t*, void*);
    bool DirectStopChecked(uint64_t generation, bool output,int* error=nullptr,bool* channel_failure=nullptr,uint8_t* attempts=nullptr,const companion::AudioLease* expected=nullptr);
    // Shared checked IO, always fenced by the captured common lease. Public
    // USB and Muse entrypoints remain disjoint; no owner enum is substituted.
    bool ScopedEnableInputChecked(uint64_t,bool);
    bool ScopedEnableOutputChecked(uint64_t,bool);
    bool ScopedInputDataChecked(uint64_t,std::vector<int16_t>&);
    bool ScopedOutputDataChecked(uint64_t,std::vector<int16_t>&);
    bool ScopedDrainBegin(uint64_t);
    DirectUsbDrain ScopedDrainState(uint64_t);
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
    struct MuseColdTag{};
    explicit CoreS3AudioCodec(MuseColdTag);
    bool muse_cold_=false,muse_cold_attempted_=false,muse_cold_ready_=false,muse_cold_output_confirmed_=false;
    MuseColdEvidence muse_cold_evidence_;
    bool MuseColdInputAbsent()const;
    bool MuseInputQuiescentProof()const;
    bool MuseOutputQuiescentProof()const;
    bool MuseColdStop(bool,int*,uint8_t*);
    bool muse_bound_=false,muse_retired_=false,muse_discarding_=false;
    bool muse_stop_active_=false,muse_volume_active_=false;
    bool muse_quiescent_=false,muse_quiescing_=false,muse_quiesce_retained_=false;uint64_t muse_last_quiesce_ticket_=0;
    uint64_t muse_last_discard_ticket_=0,muse_last_reset_ticket_=0;
    MuseAmpResetPort* muse_reset_port_=nullptr;
    uint64_t muse_drain_ticket_=0,muse_last_drain_ticket_=0;
    bool MuseMatch(const companion::AudioLease&)const;
    MuseDiscard MuseDiscardStopped(const companion::AudioLease&,uint64_t,MuseDiscardEvidence* detail=nullptr);
#endif
    bool IsMuseBinding()const {
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
        return muse_bound_;
#else
        return false;
#endif
    }
    bool ColdOutputOnlyInstance()const {
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
        return muse_cold_;
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
            muse_drain_ticket_=muse_last_drain_ticket_=muse_last_discard_ticket_=muse_last_reset_ticket_=0;
            muse_quiescent_=muse_quiescing_=muse_quiesce_retained_=false;muse_last_quiesce_ticket_=0;
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
    void Start() override;

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
    // Resource/hardware binding only: not microphone/output consent. The
    // caller retains the exact original Muse lease through checked cleanup.
    bool MuseCodecAcquire(const companion::AudioLease&);
    // Terminal cleanup only: parks this binding. Register/DMA evidence is not physical flush.
    MuseQuiesceEvidence MuseQuiesceForRetirementChecked(const companion::AudioLease&,uint64_t,companion::muse::QuiesceDiagnostic* diagnostic=nullptr);
    // Non-owning; set before acquire. Without a port the reset is Unavailable.
    void SetMuseAmpResetPort(MuseAmpResetPort* port){std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);muse_reset_port_=port;}
    // Ticket must exceed every prior reset and discard ticket. Failure retains custody and revokes the lease.
    MuseResetEvidence MuseResetAmplifierChecked(const companion::AudioLease&,uint64_t ticket);
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
