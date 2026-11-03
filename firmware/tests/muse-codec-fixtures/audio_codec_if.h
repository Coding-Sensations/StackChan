#pragma once
// Inert call-shape boundary; actual pinned header is used by ESP32 checks.
struct audio_codec_if_t{int(*get_reg)(const audio_codec_if_t*,int,int*)=nullptr;int(*set_reg)(const audio_codec_if_t*,int,int)=nullptr;int(*enable)(const audio_codec_if_t*,bool)=nullptr;};
