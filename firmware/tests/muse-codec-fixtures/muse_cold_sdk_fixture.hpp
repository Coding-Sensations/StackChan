// Inert SDK port only. Production cold methods are extracted unchanged.
#pragma once
constexpr int AW88298_CODEC_DEFAULT_ADDR=(0x36<<1);
constexpr int GPIO_NUM_NC=-1,I2S_NUM_0=0,I2S_ROLE_MASTER=1,I2S_DATA_BIT_WIDTH_16BIT=16,I2S_SLOT_MODE_STEREO=2,I2S_GPIO_UNUSED=-1;
constexpr int AUDIO_CODEC_DMA_DESC_NUM=6,AUDIO_CODEC_DMA_FRAME_NUM=240;
struct i2s_chan_config_t{int id=0,role=1,dma_desc_num=6,dma_frame_num=240;};
#define I2S_CHANNEL_DEFAULT_CONFIG(id,role) i2s_chan_config_t{}
struct ColdClk{int sample_rate_hz;};struct ColdSlot{int data_bit_width,slot_mode;};
#define I2S_STD_CLK_DEFAULT_CONFIG(rate) ColdClk{rate}
#define I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(bits,mode) ColdSlot{bits,mode}
struct i2s_std_config_t{ColdClk clk_cfg;ColdSlot slot_cfg;struct Gpio{int mclk,bclk,ws,dout,din;struct Invert{bool mclk_inv,bclk_inv,ws_inv;}invert_flags;}gpio_cfg;};
using i2c_port_t=int;
struct audio_codec_i2c_cfg_t{int port=0;uint8_t addr=0;void*bus_handle=nullptr;};
struct audio_codec_ctrl_if_t{};struct audio_codec_gpio_if_t{};
struct aw88298_codec_cfg_t{const audio_codec_ctrl_if_t*ctrl_if=nullptr;const audio_codec_gpio_if_t*gpio_if=nullptr;int reset_pin=-1;struct Gain{float pa_voltage=0,codec_dac_voltage=0,pa_gain=0;}hw_gain;};
static int cold_calls=0,cold_fail=0,cold_code=-73;static bool cold_internal=true,cold_null=false;
static std::function<void()> cold_hook;static const audio_codec_if_t*cold_amplifier=nullptr;
inline int ColdCall(){++cold_calls;auto f=std::move(cold_hook);cold_hook={};if(f)f();return cold_calls==cold_fail?cold_code:0;}
inline bool esp_ptr_internal(const void*){return cold_internal;}
inline int i2s_new_channel(i2s_chan_config_t*,void**tx,void**rx){assert(tx&&rx==nullptr);auto s=ColdCall();*tx=cold_null?nullptr:reinterpret_cast<void*>(2);return s;}
inline int i2s_channel_init_std_mode(void*h,i2s_std_config_t*c){assert(h==reinterpret_cast<void*>(2)&&c->gpio_cfg.mclk==I2S_GPIO_UNUSED&&c->gpio_cfg.bclk==I2S_GPIO_UNUSED&&c->gpio_cfg.ws==I2S_GPIO_UNUSED&&c->gpio_cfg.dout==I2S_GPIO_UNUSED&&c->gpio_cfg.din==I2S_GPIO_UNUSED&&c->clk_cfg.sample_rate_hz==24000);return ColdCall();}
inline const audio_codec_ctrl_if_t*audio_codec_new_i2c_ctrl(audio_codec_i2c_cfg_t*c){assert(c->bus_handle==reinterpret_cast<void*>(9)&&c->addr==AW88298_CODEC_DEFAULT_ADDR);static audio_codec_ctrl_if_t object;return ColdCall()?nullptr:&object;}
inline const audio_codec_gpio_if_t*audio_codec_new_gpio(){static audio_codec_gpio_if_t object;return ColdCall()?nullptr:&object;}
inline const audio_codec_if_t*aw88298_codec_new(aw88298_codec_cfg_t*c){assert(c->ctrl_if&&c->gpio_if&&c->reset_pin==-1);return ColdCall()?nullptr:cold_amplifier;}
