#pragma once
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <vector>
#include <algorithm>
#ifdef CODEC_FIXTURE_REAL_TASKS
#include <chrono>
#include <thread>
#include <atomic>
#endif
#include <companion/diagnostics.h>
namespace codec_fixture {
inline uint64_t clock_ms=100;
#ifdef CODEC_FIXTURE_REAL_TASKS
inline std::thread worker;
inline std::atomic<bool> release_worker{false};
inline unsigned decode_delay_ms=0,receive_delay_ms=0;
inline thread_local bool is_worker=false;
inline std::vector<unsigned> decoded_sequence;
inline const auto started=std::chrono::steady_clock::now();
#endif
inline void (*entry)(void*)=nullptr;
inline void* argument=nullptr;
inline std::function<void()> delay;
inline int decode_result=0;
inline std::size_t decoded_bytes=2880;
inline bool consume_all=true;
inline unsigned decode_calls=0;
inline int task_create_result=1;
inline int encoder_open_result=0,decoder_open_result=0,resampler_open_result=0;
inline int encoder_process_result=0;inline uint32_t resample_samples=0;
inline bool fault_recorded=false;
inline companion::diagnostic::Task fault_task=companion::diagnostic::Task::Main;
inline companion::diagnostic::FaultCode fault_code=companion::diagnostic::FaultCode::None;
inline int32_t fault_detail=0;
inline unsigned fault_calls=0;
#ifdef CODEC_FIXTURE_REAL_TASKS
inline std::atomic<unsigned> samples{0};
#else
inline unsigned samples=0;
#endif

}
using TaskHandle_t=void*;
constexpr int pdPASS=1;
#define configASSERT(x) assert(x)
#define pdMS_TO_TICKS(x) (x)
#ifndef CODEC_FIXTURE_REAL_TASKS
inline int xTaskCreate(void (*entry)(void*),const char*,int,void* arg,int,TaskHandle_t* handle){if(codec_fixture::task_create_result!=pdPASS)return codec_fixture::task_create_result;codec_fixture::entry=entry;codec_fixture::argument=arg;*handle=arg;return pdPASS;}
inline void vTaskDelay(unsigned ms){codec_fixture::clock_ms+=ms;if(codec_fixture::delay)codec_fixture::delay();}
inline void vTaskDelete(void*){}
inline int64_t esp_timer_get_time(){return codec_fixture::clock_ms*1000;}
#else
inline int xTaskCreate(void (*entry)(void*),const char*,int,void* arg,int,TaskHandle_t* handle){
 *handle=arg;codec_fixture::worker=std::thread([=]{codec_fixture::is_worker=true;while(!codec_fixture::release_worker.load())std::this_thread::sleep_for(std::chrono::milliseconds(1));entry(arg);});return pdPASS;
}
inline void vTaskDelay(unsigned ms){
 if(!codec_fixture::is_worker && codec_fixture::receive_delay_ms){ms+=codec_fixture::receive_delay_ms;codec_fixture::receive_delay_ms=0;}
 std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
inline void vTaskDelete(void*){}
inline int64_t esp_timer_get_time(){return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-codec_fixture::started).count();}
#endif
class AudioCodec {
protected: bool input_=false,output_=false;
public:
 virtual ~AudioCodec()=default;
 bool input_enabled()const{return input_;} bool output_enabled()const{return output_;}
 int input_sample_rate()const{return 24000;} int output_sample_rate()const{return 24000;} int input_channels()const{return 1;}
 virtual void EnableInput(bool b){input_=b;} virtual void EnableOutput(bool b){output_=b;}
 virtual bool InputData(std::vector<int16_t>&)=0;
 virtual void OutputData(std::vector<int16_t>&)=0;
};
constexpr int ESP_AUDIO_MONO=1,ESP_AUDIO_BIT16=16,ESP_OPUS_ENC_FRAME_DURATION_60_MS=60,ESP_OPUS_ENC_APPLICATION_VOIP=1,ESP_OPUS_DEC_FRAME_DURATION_60_MS=60,ESP_AE_RATE_CVT_PERF_TYPE_SPEED=1,ESP_AUDIO_DEC_RECOVERY_NONE=0,ESP_AUDIO_ERR_OK=0,ESP_AE_ERR_OK=0;
struct esp_opus_enc_config_t{int sample_rate,channel,bits_per_sample,bitrate,frame_duration,application_mode,complexity;bool enable_dtx,enable_vbr;};
struct esp_opus_dec_cfg_t{int sample_rate,channel,frame_duration;};
struct esp_ae_rate_cvt_cfg_t{int src_rate,dest_rate,channel,bits_per_sample,complexity,perf_type;};
using esp_ae_rate_cvt_handle_t=void*;
struct esp_audio_dec_in_raw_t{uint8_t* buffer;std::size_t len;int frame_recover;std::size_t consumed;};
struct esp_audio_dec_out_frame_t{uint8_t* buffer;std::size_t len,decoded_size;};
struct esp_audio_dec_info_t{};
struct esp_audio_enc_in_frame_t{uint8_t* buffer;std::size_t len;};
struct esp_audio_enc_out_frame_t{uint8_t* buffer;std::size_t len,encoded_bytes;};
inline int esp_opus_enc_open(const esp_opus_enc_config_t*,std::size_t,void** p){*p=reinterpret_cast<void*>(1);return codec_fixture::encoder_open_result;}
inline int esp_opus_dec_open(const esp_opus_dec_cfg_t*,std::size_t,void** p){*p=reinterpret_cast<void*>(1);return codec_fixture::decoder_open_result;}
inline int esp_ae_rate_cvt_open(const esp_ae_rate_cvt_cfg_t*,void** p){*p=reinterpret_cast<void*>(1);return codec_fixture::resampler_open_result;}
inline void esp_opus_enc_close(void*){} inline void esp_opus_dec_close(void*){} inline void esp_ae_rate_cvt_close(void*){} inline void esp_ae_rate_cvt_reset(void*){}
inline int esp_opus_dec_decode(void*,esp_audio_dec_in_raw_t* in,esp_audio_dec_out_frame_t* out,esp_audio_dec_info_t*){++codec_fixture::decode_calls;
#ifdef CODEC_FIXTURE_REAL_TASKS
 std::this_thread::sleep_for(std::chrono::milliseconds(codec_fixture::decode_delay_ms));codec_fixture::decoded_sequence.push_back(in->buffer[0]);
#endif
in->consumed=codec_fixture::consume_all?in->len:0;out->decoded_size=codec_fixture::decoded_bytes;return codec_fixture::decode_result;}
inline int esp_ae_rate_cvt_get_max_out_sample_num(void*,int,uint32_t* n){*n=codec_fixture::resample_samples;return 0;}
inline int esp_ae_rate_cvt_process(void*,const int16_t*,int,int16_t* out,uint32_t* n){*n=codec_fixture::resample_samples;std::fill_n(out,*n,int16_t{0});return 0;}
inline int esp_opus_enc_process(void*,esp_audio_enc_in_frame_t*,esp_audio_enc_out_frame_t* out){out->encoded_bytes=1;return codec_fixture::encoder_process_result;}

// Link-boundary recorder: real diagnostic enums/API, no hardware or SDK stack scan.
// The module's atomic publication implementation has its own separate tests.
namespace companion::diagnostic {
inline void Stage(Task,StageCode){}
inline void Sample(Task){++codec_fixture::samples;}
inline void Fault(Task task,FaultCode code,int32_t detail){
 ++codec_fixture::fault_calls;
 if(!codec_fixture::fault_recorded){codec_fixture::fault_recorded=true;codec_fixture::fault_task=task;codec_fixture::fault_code=code;codec_fixture::fault_detail=detail;}
}
}
