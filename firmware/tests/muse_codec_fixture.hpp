// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <companion/audio_owner.hpp>
#include <mutex>
#include <vector>
#include <cassert>
#include <functional>
#include <algorithm>
#include <cstdio>
#define private public
#include "hal/board/cores3_audio_codec.h"
#undef private
namespace companion::diagnostic {
enum class StageCode {DriverInputOpen,DriverInputGain,DriverInputClose,DriverRead,DriverOutputOpen,DriverOutputVolume,DriverOutputClose,DriverWrite};
using FaultCode=StageCode;
}
template<class T>void companion_codec_stage(T){}
template<class T>void companion_codec_fault(T,int){}
#define ESP_ERROR_CHECK_WITHOUT_ABORT(x) ((void)(x))
#define ESP_ERROR_CHECK(x) assert((x)==0)
static int open_result=0,close_result=0,disable_result=0,read_result=0,write_result=0;
static int opens=0,closes=0,disables=0,reads=0,writes=0,input_disables=0,output_disables=0;
static std::function<void()> during,read_hook;
static void Hook(){auto f=std::move(during);during={};if(f)f();}
static int esp_codec_dev_open(void*,esp_codec_dev_sample_info_t*){++opens;Hook();return open_result;}
static int esp_codec_dev_close(void*){++closes;Hook();return close_result;}
static int esp_codec_dev_set_in_channel_gain(void*,int,int){return 0;}
static int esp_codec_dev_set_out_vol(void*,int){return 0;}
static int esp_codec_dev_read(void*,void* out,size_t size){++reads;std::fill_n(static_cast<int16_t*>(out),size/2,42);auto f=std::move(read_hook);read_hook={};if(f)f();Hook();return read_result;}
static int esp_codec_dev_write(void*,void*,size_t){++writes;Hook();return write_result;}
static int i2s_channel_disable(void* h){++disables;if(h==reinterpret_cast<void*>(1))++input_disables;else if(h==reinterpret_cast<void*>(2))++output_disables;else assert(false);Hook();return disable_result;}
CoreS3AudioCodec::CoreS3AudioCodec(void*,int input,int output,gpio_num_t,gpio_num_t,gpio_num_t,gpio_num_t,gpio_num_t,uint8_t,uint8_t,bool reference){input_sample_rate_=input;output_sample_rate_=output;input_reference_=reference;input_channels_=reference?2:1;direct_callback_ready_=true;input_dev_=reinterpret_cast<void*>(1);output_dev_=reinterpret_cast<void*>(2);}
CoreS3AudioCodec::~CoreS3AudioCodec()=default;
ACTUAL_METHODS
using namespace companion;
static AudioLease Take(AudioOwner owner=AudioOwner::Muse,AudioPurpose purpose=AudioPurpose::MusePushToTalk,uint64_t epoch=7){AudioLease lease;assert(AcquireAudio(owner,purpose,epoch,lease));return lease;}
static void Reset(){open_result=close_result=disable_result=read_result=write_result=0;opens=closes=disables=reads=writes=input_disables=output_disables=0;during={};read_hook={};}
inline void Cleanup(CoreS3AudioCodec& codec,AudioLease& lease){assert(codec.MuseStopInputChecked(lease));assert(codec.MuseStopOutputChecked(lease));assert(codec.MuseCodecRelease(lease));assert(CommonAudioAuthority().Holds(lease));assert(ReleaseAudio(lease,true));}
static CoreS3AudioCodec Make(){return {nullptr,24000,24000,0,0,0,0,0,0,0,true};}
