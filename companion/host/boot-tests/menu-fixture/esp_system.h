#pragma once
enum esp_reset_reason_t {ESP_RST_POWERON,ESP_RST_SW};
extern esp_reset_reason_t reset_reason;
inline esp_reset_reason_t esp_reset_reason(){return reset_reason;}
