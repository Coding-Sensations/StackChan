#pragma once
struct audio_codec_data_if_t;struct audio_codec_ctrl_if_t;struct audio_codec_if_t;struct audio_codec_gpio_if_t;
using esp_codec_dev_handle_t=void*;
struct esp_codec_dev_sample_info_t{int bits_per_sample,channel,channel_mask;uint32_t sample_rate;int mclk_multiple;};
#define ESP_CODEC_DEV_MAKE_CHANNEL_MASK(x) (1<<(x))
constexpr int ESP_OK=0,ESP_CODEC_DEV_OK=0;
