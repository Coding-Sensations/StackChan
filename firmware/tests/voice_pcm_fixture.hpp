// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <deque>
static int resets=0,dsp_error=0;static std::deque<unsigned> produced;static int16_t batch=11;static std::function<void()> dsp_hook;
struct Dsp {int to;};
esp_ae_err_t esp_ae_rate_cvt_open(esp_ae_rate_cvt_cfg_t* c,void** h){*h=new Dsp{int(c->dest_rate)};return ESP_AE_ERR_OK;}
void esp_ae_rate_cvt_close(void* h){delete static_cast<Dsp*>(h);}
esp_ae_err_t esp_ae_rate_cvt_reset(void*){++resets;return dsp_error?ESP_AE_ERR_FAIL:ESP_AE_ERR_OK;}
esp_ae_err_t esp_ae_rate_cvt_get_max_out_sample_num(void* h,uint32_t,uint32_t* out){*out=static_cast<Dsp*>(h)->to==16000?512:768;return ESP_AE_ERR_OK;}
esp_ae_err_t esp_ae_rate_cvt_process(void* h,void*,uint32_t n,void* out,uint32_t* size){unsigned count=static_cast<Dsp*>(h)->to==16000?(produced.empty()?320:produced.front()):(n*3/2);if(!produced.empty()&&static_cast<Dsp*>(h)->to==16000)produced.pop_front();assert(count<=*size);std::fill_n(static_cast<int16_t*>(out),count,batch);batch+=11;*size=count;auto f=std::move(dsp_hook);dsp_hook={};if(f)f();return dsp_error?ESP_AE_ERR_FAIL:ESP_AE_ERR_OK;}
