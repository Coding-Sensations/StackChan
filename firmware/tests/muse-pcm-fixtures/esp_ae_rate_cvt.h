#pragma once
// Handwritten inert public-API declarations; no DSP/vendor implementation.
// Values/signatures verified against the pinned ESP header; see PROVENANCE.json.
#include <cstdint>
using esp_ae_rate_cvt_handle_t=void*;
enum esp_ae_rate_cvt_perf_type_t{ESP_AE_RATE_CVT_PERF_TYPE_MEMORY=0,ESP_AE_RATE_CVT_PERF_TYPE_SPEED=1};
enum esp_ae_err_t{ESP_AE_ERR_OK=0,ESP_AE_ERR_FAIL=-1,ESP_AE_ERR_MEM_LACK=-2,ESP_AE_ERR_INVALID_PARAMETER=-3,ESP_AE_ERR_NOT_SUPPORT=-4};
struct esp_ae_rate_cvt_cfg_t{uint32_t src_rate=0,dest_rate=0;uint8_t channel=0,bits_per_sample=0,complexity=0;esp_ae_rate_cvt_perf_type_t perf_type=ESP_AE_RATE_CVT_PERF_TYPE_MEMORY;};
esp_ae_err_t esp_ae_rate_cvt_open(esp_ae_rate_cvt_cfg_t*,void**);
void esp_ae_rate_cvt_close(void*);
esp_ae_err_t esp_ae_rate_cvt_get_max_out_sample_num(void*,uint32_t,uint32_t*);
esp_ae_err_t esp_ae_rate_cvt_process(void*,void*,uint32_t,void*,uint32_t*);
