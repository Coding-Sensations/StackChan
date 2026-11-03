#include "station_event_barrier.hpp"
#include <esp_event.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <limits>
#include <mutex>
ESP_EVENT_DEFINE_BASE(STACKCHAN_STATION_DRAIN);
namespace companion::network {
namespace {
std::mutex drain_mutex;
esp_event_handler_instance_t drain_handler=nullptr;
std::atomic<uint32_t> completed{0};uint32_t requested=0;
void Acknowledge(void*,esp_event_base_t,int32_t,void* data){if(data)completed= *static_cast<const uint32_t*>(data);}
bool NoDefaultLoop(esp_err_t error){wifi_mode_t mode{};return error==ESP_ERR_INVALID_STATE&&esp_wifi_get_mode(&mode)==ESP_ERR_WIFI_NOT_INIT;}
}
bool DrainStationEvents(uint32_t timeout_ms){
    std::lock_guard<std::mutex> lock(drain_mutex);
    if(!drain_handler){
        const auto error=esp_event_handler_instance_register(STACKCHAN_STATION_DRAIN,0,Acknowledge,nullptr,&drain_handler);
        if(error!=ESP_OK)return NoDefaultLoop(error);
    }
    if(requested==std::numeric_limits<uint32_t>::max())return false;
    const uint32_t ticket=++requested;
    const auto error=esp_event_post(STACKCHAN_STATION_DRAIN,0,&ticket,sizeof(ticket),pdMS_TO_TICKS(10));
    if(error!=ESP_OK)return NoDefaultLoop(error);
    const auto deadline=esp_timer_get_time()+int64_t(timeout_ms)*1000;
    while(completed.load()<ticket){if(esp_timer_get_time()>=deadline)return false;vTaskDelay(pdMS_TO_TICKS(1));}
    return true;
}
}
