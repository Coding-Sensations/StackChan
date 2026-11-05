#!/usr/bin/env python3
"""Pinned SDK transport variants: retain state until receiver/worker exit.
Generated files replace component units in CMake; fetched sources stay untouched.
Void legacy disconnects run only in the retained shutdown worker. Each wait is
bounded; a missing receipt retains that worker/object and keeps Home pending.
"""
import argparse, hashlib, json
from pathlib import Path

def replace(s,a,b):
 if s.count(a)!=1:raise SystemExit('teardown anchor drift: '+a[:80])
 return s.replace(a,b)
def region(s,a,b,new):
 start=s.index(a);end=s.index(b,start);return s[:start]+new+'\n\n'+s[end:]
def receiver_exit_barrier(source,kind,bit):
 # SetBits may wake a higher-priority waiter before its final event-group read.
 # This boot-lifetime mutex is never held over IO or a receive-exit wait.
 mutex='home_'+kind+'_receiver_exit_mutex'
 source='#include <mutex>\nstatic std::mutex '+mutex+';\n'+source
 source=replace(source,'        xEventGroupSetBits('+kind+'->event_group_, '+bit+');',
     '        {std::lock_guard<std::mutex> exit_lock('+mutex+');\n         xEventGroupSetBits('+kind+'->event_group_, '+bit+');} // last owner access before task deletion')
 source=replace(source,'        if (!(bits & '+bit+')) {\n            ESP_LOGE(TAG, "Failed to wait for receive task exit");\n        }\n        receive_task_handle_ = nullptr;',
     '        if (!(bits & '+bit+')) {\n            ESP_LOGE(TAG, "Failed to wait for receive task exit");\n        }\n        std::lock_guard<std::mutex> exit_lock('+mutex+'); // setter has returned\n        receive_task_handle_ = nullptr;')
 return source

def generate(network,espnow,out):
 out.mkdir(parents=True,exist_ok=True)
 pins=json.loads((Path(__file__).with_name('teardown_pins.json')).read_text())
 def read(root,name,key):
  data=(root/name).read_bytes()
  if hashlib.sha256(data).hexdigest()!=pins[key]:raise SystemExit('Unreviewed teardown dependency: '+key)
  return data.decode()
 # A receive task may already have set connected_=false while its callback is
 # still running. Join the task handle, not the connection-state boolean.
 for kind in ['tcp','udp']:
  s=read(network,'src/esp/esp_'+kind+'.cc','esp_'+kind+'.cc');bit='ESP_'+kind.upper()+'_EVENT_RECEIVE_TASK_EXIT'
  s=receiver_exit_barrier(s,kind,bit)
  (out/('joined_esp_'+kind+'.cc')).write_text(s)
 s=read(network,'src/esp/esp_ssl.cc','esp_ssl.cc')
 s=receiver_exit_barrier(s,'ssl','ESP_SSL_EVENT_RECEIVE_TASK_EXIT')
 (out/'joined_esp_ssl.cc').write_text(s)
 s=read(network,'src/web_socket.cc','web_socket.cc')
 (out/'joined_web_socket.cc').write_text(s)
 s=read(network,'src/esp/esp_mqtt.cc','esp_mqtt.cc')
 s=replace(s,'''        esp_mqtt_client_stop(mqtt_client_handle_);
        esp_mqtt_client_destroy(mqtt_client_handle_);''','''        esp_err_t stopped;
        do{stopped=esp_mqtt_client_stop(mqtt_client_handle_);if(stopped!=ESP_OK&&stopped!=ESP_ERR_INVALID_STATE)vTaskDelay(pdMS_TO_TICKS(100));}while(stopped!=ESP_OK&&stopped!=ESP_ERR_INVALID_STATE);
        while(esp_mqtt_client_destroy(mqtt_client_handle_)!=ESP_OK)vTaskDelay(pdMS_TO_TICKS(100));''')
 (out/'joined_esp_mqtt.cc').write_text(s)
 s=read(espnow,'src/espnow/src/espnow.c','espnow.c')
 s=s.replace('static void espnow_main_task(void *arg)','static EventGroupHandle_t home_join_group;\nstatic bool home_driver_started,home_events_registered,home_worker_started;\nstatic void espnow_main_task(void *arg)',1)
 s=replace(s,'    ESP_LOGI(TAG, "main task exit");\n    vTaskDelete(NULL);','    ESP_LOGI(TAG, "main task exit");\n    xEventGroupSetBits(home_join_group,BIT0);\n    vTaskDelete(NULL);')
 # Allocate the receipt before any startup side effect. Partial initialization
 # keeps each handle until its matching checked stop succeeds.
 s=replace(s,'    g_espnow_config = ESP_MALLOC(sizeof(espnow_config_t));','''    if(!home_join_group)home_join_group=xEventGroupCreate();
    if(!home_join_group)return ESP_ERR_NO_MEM;
    g_espnow_config = calloc(1,sizeof(espnow_config_t));
    if(!g_espnow_config)return ESP_ERR_NO_MEM;''')
 s=replace(s,'    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));','''    esp_err_t home_error=esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event_handler,NULL);
    if(home_error!=ESP_OK)return home_error;
    home_events_registered=true;''')
 s=replace(s,'    ESP_ERROR_CHECK(esp_now_init());','''    home_error=esp_now_init();if(home_error!=ESP_OK)return home_error;
    home_driver_started=true;''')
 for api in ['esp_now_register_send_cb(espnow_send_cb)','esp_now_register_recv_cb(espnow_recv_cb)','esp_now_set_pmk(config->pmk)']:
  s=replace(s,'    ESP_ERROR_CHECK('+api+');','    home_error='+api+';if(home_error!=ESP_OK)return home_error;')
 s=replace(s,'        g_espnow_sec = ESP_MALLOC(sizeof(espnow_sec_t));\n        g_espnow_dec = ESP_MALLOC(sizeof(espnow_sec_t));','''        g_espnow_sec=calloc(1,sizeof(espnow_sec_t));
        g_espnow_dec=calloc(1,sizeof(espnow_sec_t));
        if(!g_espnow_sec||!g_espnow_dec)return ESP_ERR_NO_MEM;''')
 s=replace(s,'    xTaskCreate(espnow_main_task, "espnow_main", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);','''    xEventGroupClearBits(home_join_group,BIT0);
    if(xTaskCreate(espnow_main_task,"espnow_main",4096,NULL,tskIDLE_PRIORITY+1,NULL)!=pdPASS)return ESP_ERR_NO_MEM;
    home_worker_started=true;''')
 s=replace(s,'esp_err_t espnow_deinit(void)\n{\n    ESP_ERROR_RETURN(!g_espnow_config, ESP_ERR_ESPNOW_NOT_INIT, "ESPNOW is not initialized");',
            'esp_err_t espnow_deinit(void)\n{\n    if(!g_espnow_config)return ESP_OK;')
 s=replace(s,'''    ESP_ERROR_CHECK(esp_now_unregister_recv_cb());
    ESP_ERROR_CHECK(esp_now_unregister_send_cb());
    ESP_ERROR_CHECK(esp_now_deinit());''','''    if(home_driver_started){
        esp_now_unregister_recv_cb();esp_now_unregister_send_cb();
        esp_err_t stopped=esp_now_deinit();
        if(stopped!=ESP_OK && stopped!=ESP_ERR_ESPNOW_NOT_INIT)return stopped;
        home_driver_started=false;
    }''')
 s=replace(s,'''    if (queue_over_write(ESPNOW_EVENT_STOP, NULL, 0, NULL, 0) != pdPASS) {
        ESP_LOGW(TAG, "[%s, %d] Send queue failed", __func__, __LINE__);
    }''','''    if(home_worker_started){
        while(!(xEventGroupGetBits(home_join_group)&BIT0)){
            queue_over_write(ESPNOW_EVENT_STOP,NULL,0,NULL,0);
            xEventGroupWaitBits(home_join_group,BIT0,pdFALSE,pdFALSE,pdMS_TO_TICKS(100));
        }
        home_worker_started=false;
    }''')
 s=replace(s,'    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler));','''    if(home_events_registered){
        esp_err_t stopped=esp_event_handler_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event_handler);
        if(stopped!=ESP_OK)return stopped;
        home_events_registered=false;
    }''')
 s=replace(s,'    vSemaphoreDelete(g_send_lock);','    if(g_send_lock)vSemaphoreDelete(g_send_lock);')
 s=replace(s,'    vEventGroupDelete(g_event_group);','    if(g_event_group)vEventGroupDelete(g_event_group);')
 (out/'joined_espnow.c').write_text(s)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--network',type=Path,required=True);p.add_argument('--espnow',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();generate(a.network,a.espnow,a.output)
