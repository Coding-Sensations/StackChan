// SPDX-License-Identifier: Apache-2.0
#include <hal/board/cores3_audio_codec.h>
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include <esp_memory_utils.h>

CoreS3AudioCodec::CoreS3AudioCodec(MuseColdTag) {
    muse_cold_=true;duplex_=false;input_reference_=false;
    input_sample_rate_=output_sample_rate_=24000;input_channels_=0;output_channels_=1;output_volume_=0;
    tx_handle_=rx_handle_=nullptr;
}
CoreS3AudioCodec& CoreS3AudioCodec::MuseColdOutputOnly(){
    // No driver call until explicit Initialize; retain all resources for program
    // lifetime, including partial failures. No attempt to delete/recreate DMA.
    static CoreS3AudioCodec core(MuseColdTag{});return core;
}
CoreS3AudioCodec::MuseColdEvidence CoreS3AudioCodec::MuseColdInitialize(const companion::AudioLease& lease,const MuseColdOutputConfig&config){
    std::lock_guard<std::recursive_mutex> lock(direct_io_mutex_);
    MuseColdEvidence rejected;
    const auto original=lease;const auto supplied=config;
    if(!muse_cold_||muse_cold_attempted_||!supplied.bus||!companion::CommonAudioAuthority().Current(original)||
       original.owner()!=companion::AudioOwner::Muse||original.purpose()!=companion::AudioPurpose::MusePushToTalk)return rejected;
    muse_cold_attempted_=true;muse_cold_evidence_={};auto&e=muse_cold_evidence_;e.stage=MuseColdStage::Binding;e.fault=MuseColdFault::None;
    direct_common_lease_=original;direct_generation_=original.generation();muse_bound_=true;
    muse_quiescent_=muse_quiesce_retained_=true;
    const auto retain=[&]{if(!muse_cold_evidence_.Complete())companion::RevokeAudio(original);};
    struct Retain{const decltype(retain)&call;~Retain(){call();}} retained{retain};
    struct Busy{bool&flag;explicit Busy(bool&f):flag(f){flag=true;}~Busy(){flag=false;}} busy(muse_quiescing_);
    const auto current=[&]{return MuseMatch(original)&&companion::CommonAudioAuthority().Current(original);};
    const auto okay=[&](MuseColdStage stage,int status,bool object=true){
        e.stage=stage;e.status=status;
        if(!current()){e.fault=MuseColdFault::BindingLost;return false;}
        if(status!=ESP_OK){e.fault=MuseColdFault::Driver;return false;}
        if(!object){e.fault=MuseColdFault::NoObject;return false;}return true;
    };
    if(!esp_ptr_internal(this)){e.fault=MuseColdFault::InvalidState;return e;}
    i2s_chan_config_t channel=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_MASTER);
    channel.dma_desc_num=AUDIO_CODEC_DMA_DESC_NUM;channel.dma_frame_num=AUDIO_CODEC_DMA_FRAME_NUM;
    if(!okay(MuseColdStage::Channel,i2s_new_channel(&channel,&tx_handle_,nullptr)))return e;
    // Handle is checked separately: function-argument evaluation order must not
    // read the out parameter before the SDK has populated it.
    if(!tx_handle_){e.fault=MuseColdFault::NoObject;return e;}
    i2s_std_config_t format={
        .clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(24000),
        .slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO),
        .gpio_cfg={.mclk=I2S_GPIO_UNUSED,.bclk=I2S_GPIO_UNUSED,.ws=I2S_GPIO_UNUSED,.dout=I2S_GPIO_UNUSED,.din=I2S_GPIO_UNUSED,.invert_flags={.mclk_inv=false,.bclk_inv=false,.ws_inv=false}}
    };
    if(!okay(MuseColdStage::Format,i2s_channel_init_std_mode(tx_handle_,&format)))return e;
    i2s_chan_info_t info{};
    if(!okay(MuseColdStage::Info,i2s_channel_get_info(tx_handle_,&info)))return e;
    if(info.dir!=I2S_DIR_TX||info.mode!=I2S_COMM_MODE_STD||info.is_enabled||!info.total_dma_buf_size||info.total_dma_buf_size>MuseDiscardByteLimit||info.total_dma_buf_size%sizeof(uint32_t)){
        e.fault=MuseColdFault::InvalidState;return e;
    }
    e.dma_bytes=info.total_dma_buf_size;
    audio_codec_i2c_cfg_t control={};control.port=1;control.addr=AW88298_CODEC_DEFAULT_ADDR;control.bus_handle=supplied.bus;
    out_ctrl_if_=audio_codec_new_i2c_ctrl(&control);if(!okay(MuseColdStage::Control,ESP_OK,out_ctrl_if_!=nullptr))return e;
    gpio_if_=audio_codec_new_gpio();if(!okay(MuseColdStage::Gpio,ESP_OK,gpio_if_!=nullptr))return e;
    aw88298_codec_cfg_t amplifier={};amplifier.ctrl_if=out_ctrl_if_;amplifier.gpio_if=gpio_if_;amplifier.reset_pin=GPIO_NUM_NC;
    amplifier.hw_gain.pa_voltage=5.0;amplifier.hw_gain.codec_dac_voltage=3.3;amplifier.hw_gain.pa_gain=1;
    out_codec_if_=aw88298_codec_new(&amplifier);if(!okay(MuseColdStage::Amplifier,ESP_OK,out_codec_if_!=nullptr))return e;
    e.stage=MuseColdStage::Quiet;e.amplifier=codec_quiet::Quiesce(out_codec_if_,current);
    if(!e.amplifier.Complete()){e.fault=MuseColdFault::Amplifier;return e;}
    if(!current()){e.fault=MuseColdFault::BindingLost;return e;}
    muse_cold_ready_=true;muse_quiescent_=muse_quiesce_retained_=false;e.stage=MuseColdStage::Complete;return e;
}
bool CoreS3AudioCodec::MuseColdInputAbsent()const{
    return muse_cold_&&muse_cold_ready_&&MuseMatch(direct_common_lease_)&&!rx_handle_&&!input_dev_&&!in_codec_if_&&!in_ctrl_if_&&!data_if_&&!input_enabled_;
}
bool CoreS3AudioCodec::MuseInputQuiescentProof()const{
    return muse_cold_?MuseColdInputAbsent():direct_input_stopped_&&direct_input_closed_;
}
bool CoreS3AudioCodec::MuseOutputQuiescentProof()const{
    return muse_cold_?MuseColdInputAbsent()&&muse_cold_output_confirmed_&&!output_dev_&&!output_enabled_:direct_output_stopped_&&direct_output_closed_;
}
bool CoreS3AudioCodec::MuseColdStop(bool output,int*error,uint8_t*attempts){
    if(!MuseColdInputAbsent()||muse_retired_||(muse_quiesce_retained_&&!muse_quiescing_))return false;
    if(!output)return true; // typed source absence; no stopped/closed flag is forged
    if(muse_cold_output_confirmed_)return MuseOutputQuiescentProof();
    const auto original=direct_common_lease_;const auto channel=tx_handle_;
    if(!channel)return false;
    if(attempts)*attempts|=4; // info query, not disable/close
    i2s_chan_info_t info{};const int status=i2s_channel_get_info(channel,&info);
    const bool bound=MuseMatch(original)&&MuseColdInputAbsent()&&tx_handle_==channel;
    const bool okay=status==ESP_OK&&bound&&info.dir==I2S_DIR_TX&&info.mode==I2S_COMM_MODE_STD&&!info.is_enabled&&info.total_dma_buf_size==muse_cold_evidence_.dma_bytes;
    if(!okay){if(error)*error=status;muse_quiescent_=muse_quiesce_retained_=true;companion::RevokeAudio(original);return false;}
    muse_cold_output_confirmed_=true;return true; // never enabled/opened, actual READY info
}
#endif
