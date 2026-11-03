/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include <apps/runtime_lifecycle/session.h>
#include <companion/station_authority.hpp>
#include <companion/station_event_barrier.hpp>
#include <algorithm>
#include <mooncake_log.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_err.h>
#include <esp_system.h>
#include <esp_event.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include <time.h>
#include <sys/time.h>
#include <esp_sntp.h>
#include <esp_log.h>
#include <esp_mac.h>
#include <espnow.h>
#include <espnow_storage.h>
#include <espnow_utils.h>
#include <esp_check.h>

static const std::string_view _tag = "HAL-EspNow";

static esp_netif_t* home_sta_netif=nullptr;
static esp_event_handler_instance_t home_wifi_handler=nullptr,home_ip_handler=nullptr;
static bool home_espnow_started=false;
static EventGroupHandle_t s_wifi_event_group = NULL;
static const int WIFI_CONNECTED_BIT          = BIT0;
static const int WIFI_DISCONNECTED_BIT       = BIT1;
static const int WIFI_FAIL_BIT               = BIT2;
static const int WIFI_STARTED_BIT            = BIT3;

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    const char* TAG = "WiFi";

    // Wifi started
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_STARTED_BIT);
    }

    // Disconnected
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }

    // Connected
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static bool _wifi_init(int channel = 1)
{
    if(companion::network::Authority().Current()!=companion::network::Owner::Vendor||!companion::network::Authority().CanAcquire(companion::network::Owner::Vendor))return false;
    mclog::tagInfo(_tag, "wifi init");

    // if((nvs_flash_init())!=ESP_OK)return false;
    // if((esp_netif_init())!=ESP_OK)return false;
    auto err=esp_netif_init();if(err!=ESP_OK&&err!=ESP_ERR_INVALID_STATE)return false;
    err=esp_event_loop_create_default();if(err!=ESP_OK&&err!=ESP_ERR_INVALID_STATE)return false;
    if(!home_sta_netif)home_sta_netif=esp_netif_create_default_wifi_sta();
    if(!home_sta_netif)return false;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_mode_t existing;
    if(esp_wifi_get_mode(&existing)!=ESP_OK){err=esp_wifi_init(&cfg);if(err!=ESP_OK)return false;}
    if((esp_wifi_set_mode(WIFI_MODE_STA))!=ESP_OK)return false;

    if (!s_wifi_event_group) {
        s_wifi_event_group = xEventGroupCreate();
    }

    if(!s_wifi_event_group)return false;
    if((
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr, &home_wifi_handler))!=ESP_OK)return false;
    if((
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr, &home_ip_handler))!=ESP_OK)return false;

    if((esp_wifi_start())!=ESP_OK)return false;

    channel = std::clamp(channel, 1, 13);

    mclog::tagInfo(_tag, "wifi channel set to {}", channel);

    // 建议先开启混杂模式再设信道，确保射频频率被强制锁定
    if((esp_wifi_set_promiscuous(true))!=ESP_OK)return false;
    if((esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE))!=ESP_OK)return false;
    if((esp_wifi_set_promiscuous(false))!=ESP_OK)return false;
    return true;
}

static esp_err_t _handle_espnow_received(uint8_t* src_addr, void* data, size_t size, wifi_pkt_rx_ctrl_t* rx_ctrl)
{
    const char* TAG = "EspNow";

    ESP_PARAM_CHECK(src_addr);
    ESP_PARAM_CHECK(data);
    ESP_PARAM_CHECK(size);
    ESP_PARAM_CHECK(rx_ctrl);

    static uint32_t count = 0;

    ESP_LOGI(TAG, "espnow_recv, <%" PRIu32 "> [" MACSTR "][%d][%d][%u]: %.*s", count++, MAC2STR(src_addr),
             rx_ctrl->channel, rx_ctrl->rssi, size, size, "~");

    std::vector<uint8_t> received_data((uint8_t*)data, (uint8_t*)data + size);
    GetHAL().onEspNowData.emit(received_data);

    return ESP_OK;
}

void Hal::startEspNow(int channel)
{
    mclog::tagInfo(_tag, "start EspNow on channel {}", channel);

    if(!_wifi_init(channel)){runtime_lifecycle::RequestHome();return;}

    espnow_config_t espnow_config = ESPNOW_INIT_CONFIG_DEFAULT();

    // 2. 修改关键参数以兼容 Arduino
    espnow_config.forward_enable         = false;  // 关闭转发（多跳），Arduino 无法解析带转发头的包
    espnow_config.forward_switch_channel = false;  // 关闭自动切信道
    espnow_config.send_retry_num         = 5;      // 失败重试次数（可按需调，建议5-10）

    // 3. 修改接收使能开关
    espnow_config.receive_enable.forward = false;  // 关闭转发包接收
    espnow_config.receive_enable.data    = true;   // 必须开启这个，才能接收 Arduino 发来的普通数据包

    home_espnow_started=true;
    if(espnow_init(&espnow_config)!=ESP_OK){runtime_lifecycle::RequestHome();return;}
    espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, _handle_espnow_received);

    mclog::tagInfo(_tag, "factory mac: {}", getFactoryMacString());
}

bool Hal::espNowSend(const std::vector<uint8_t>& data, const uint8_t* destAddr)
{
    mclog::tagInfo(_tag, "send data with size: {}", data.size());

    espnow_frame_head_t frame_head = ESPNOW_FRAME_CONFIG_DEFAULT();
    esp_err_t ret                  = ESP_FAIL;

    if (destAddr == nullptr) {
        ret = espnow_send(ESPNOW_DATA_TYPE_DATA, ESPNOW_ADDR_BROADCAST, data.data(), data.size(), &frame_head,
                          pdMS_TO_TICKS(100));
    } else {
        ret = espnow_send(ESPNOW_DATA_TYPE_DATA, destAddr, data.data(), data.size(), &frame_head, pdMS_TO_TICKS(100));
    }

    if (ret != ESP_OK) {
        mclog::tagError(_tag, "send failed: {}", esp_err_to_name(ret));
        return false;
    }
    return true;
}

#include <driver/gpio.h>

void Hal::setLaserEnabled(bool enabled)
{
    static bool laser_enabled = false;
    static bool is_inited     = false;

    if (is_inited && laser_enabled == enabled) {
        return;
    }

    const gpio_num_t laser_pin = GPIO_NUM_2;

    if (!is_inited) {
        gpio_reset_pin(laser_pin);
        gpio_set_direction(laser_pin, GPIO_MODE_OUTPUT);
        gpio_set_pull_mode(laser_pin, enabled ? GPIO_PULLUP_ONLY : GPIO_PULLDOWN_ONLY);
        is_inited = true;
    }

    mclog::tagInfo(_tag, "set laser {}", enabled ? "enabled" : "disabled");

    if (enabled) {
        gpio_set_level(laser_pin, 1);
    } else {
        gpio_set_level(laser_pin, 0);
    }
    laser_enabled = enabled;
}

bool Hal::stopEspNow(){
    if(home_espnow_started){if(espnow_deinit()!=ESP_OK)return false;home_espnow_started=false;}
    if(home_wifi_handler){if(esp_event_handler_instance_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,home_wifi_handler)!=ESP_OK)return false;home_wifi_handler=nullptr;}
    if(home_ip_handler){if(esp_event_handler_instance_unregister(IP_EVENT,IP_EVENT_STA_GOT_IP,home_ip_handler)!=ESP_OK)return false;home_ip_handler=nullptr;}
    if(home_sta_netif){
        auto result=esp_wifi_stop();if(result!=ESP_OK&&result!=ESP_ERR_WIFI_NOT_STARTED)return false;
    }
    // Unregistering prevents new dispatch but a queued/running callback can still
    // borrow the netif and event group. Keep both alive until the default loop drains.
    if((home_sta_netif||s_wifi_event_group)&&!companion::network::DrainStationEvents())return false;
    if(home_sta_netif){esp_netif_destroy_default_wifi(home_sta_netif);home_sta_netif=nullptr;}
    // Keep the boot-lifetime driver initialized for the vendor Wi-Fi manager.
    // All callbacks have returned before their borrowed event group is freed.
    if(s_wifi_event_group){vEventGroupDelete(s_wifi_event_group);s_wifi_event_group=nullptr;}
    return true;
}
