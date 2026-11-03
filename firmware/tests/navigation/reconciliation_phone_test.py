"""Compile the full production phone station with inert SDK and persistence boundaries.

Exercise main's diagnostics together with lifecycle credential, epoch and stop gates.
No network, storage, SDK installation or device access occurs.
"""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

firmware = Path(__file__).resolve().parents[2]
station = firmware / 'main/hal/utils/wifi_connect'
stub = r'''
#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <functional>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <thread>
#define configASSERT(value) assert(value)
#define BIT0 1
#define pdFALSE 0
#define pdMS_TO_TICKS(value) (value)
constexpr int ESP_OK=0,ESP_FAIL=-1,ESP_ERR_WIFI_NOT_INIT=-2,ESP_ERR_WIFI_NOT_STARTED=-3,
 ESP_ERR_INVALID_STATE=-4,WIFI_MODE_NULL=0,WIFI_MODE_STA=1,WIFI_EVENT=1,IP_EVENT=2,
 ESP_EVENT_ANY_ID=-1,IP_EVENT_STA_GOT_IP=3,WIFI_EVENT_STA_CONNECTED=4,WIFI_EVENT_STA_DISCONNECTED=5,
 WIFI_STORAGE_RAM=1,WIFI_IF_STA=0,WIFI_PS_MIN_MODEM=1,WIFI_PS_NONE=0;
using wifi_mode_t=int;using esp_err_t=int;using esp_event_base_t=int;
using esp_event_handler_instance_t=void*;
using EventGroupHandle_t=int*;
inline EventGroupHandle_t xEventGroupCreate(){return new int(0);}
inline void vEventGroupDelete(int* p){delete p;}
inline int xEventGroupClearBits(int* p,int bits){return *p&=~bits;}
inline int xEventGroupSetBits(int* p,int bits){return *p|=bits;}
inline int xEventGroupGetBits(int* p){return *p;}
inline int xEventGroupWaitBits(int* p,int,int,int,int){return *p;}
enum wifi_scan_method_t{WIFI_FAST_SCAN=0,WIFI_ALL_CHANNEL_SCAN=1};
enum wifi_sort_method_t{WIFI_CONNECT_AP_BY_SIGNAL=0,WIFI_CONNECT_AP_BY_SECURITY=1};
struct wifi_config_t {struct {unsigned char ssid[32],password[64];wifi_scan_method_t scan_method;wifi_sort_method_t sort_method;}sta;};
struct wifi_ap_record_t{unsigned char ssid[33]{};int8_t rssi=0;uint8_t primary=1;};
struct wifi_event_sta_connected_t{unsigned char ssid[32];uint8_t ssid_len;};
struct wifi_event_sta_disconnected_t{uint16_t reason;};
struct esp_netif_t{};
struct ip_event_got_ip_t{esp_netif_t* esp_netif;struct{int ip;}ip_info;};
inline void esp_ip4addr_ntoa(int*,char* output,size_t size){strncpy(output,"192.0.2.1",size);}
struct wifi_init_config_t{bool nvs_enable=true;};
#define WIFI_INIT_CONFIG_DEFAULT() wifi_init_config_t{}
inline int mode=0,fail_stop=0,config_error=0,connect_error=0,netifs=0,config_calls=0,connect_calls=0;
inline bool initialized=false,ap_live=false,permission=true,save_ok=true,saved=false;
inline int64_t now=0;inline int saves=0;
inline wifi_ap_record_t ap{};inline wifi_config_t configured{};inline wifi_config_t* observed_config=nullptr;inline int policy_checks=0;
inline std::mutex gate_mutex;inline std::condition_variable gate_cv;
inline bool racing=false,timeout_waiting=false,commit_waiting=false,allow_timeout=false,allow_commit=false;
inline thread_local bool timeout_thread=false;
inline void before_failure(){
 if(!racing||!timeout_thread)return;
 std::unique_lock<std::mutex> lock(gate_mutex);timeout_waiting=true;gate_cv.notify_all();
 gate_cv.wait(lock,[]{return allow_timeout;});
}
inline int64_t esp_timer_get_time(){return now;}
inline int esp_netif_init(){return 0;}inline int esp_event_loop_create_default(){return 0;}
inline int esp_wifi_init(wifi_init_config_t*){initialized=true;return 0;}
inline int esp_wifi_get_mode(int* value){*value=mode;return initialized?0:ESP_ERR_WIFI_NOT_INIT;}
inline int esp_wifi_set_mode(int value){mode=value;return 0;}
inline int esp_wifi_set_storage(int){return 0;}inline int esp_wifi_start(){return 0;}
inline int esp_wifi_stop(){return fail_stop?ESP_FAIL:0;}
inline esp_netif_t* esp_netif_create_default_wifi_sta(){++netifs;return new esp_netif_t;}
inline void esp_netif_destroy_default_wifi(esp_netif_t* p){--netifs;delete p;}
inline int esp_event_handler_instance_register(int,int,void(*)(void*,int,int,void*),void*,void** handle){*handle=(void*)1;return 0;}
inline int esp_event_handler_instance_unregister(int,int,void*){return 0;}
inline int esp_wifi_sta_get_ap_info(wifi_ap_record_t* value){*value=ap;return ap_live?0:ESP_FAIL;}
inline int esp_wifi_disconnect(){ap_live=false;return 0;}
inline int esp_wifi_connect(){
 if(observed_config){auto* bytes=reinterpret_cast<unsigned char*>(observed_config);for(size_t i=0;i<sizeof(*observed_config);++i)assert(bytes[i]==0);observed_config=nullptr;}
 ++connect_calls;return connect_error;}
inline int esp_wifi_set_config(int,wifi_config_t* config){
 if(config->sta.ssid[0]){assert(config->sta.scan_method==WIFI_ALL_CHANNEL_SCAN);assert(config->sta.sort_method==WIFI_CONNECT_AP_BY_SIGNAL);++policy_checks;observed_config=config;}else observed_config=nullptr;
 ++config_calls;configured=*config;return config_error;}
inline int esp_wifi_set_ps(int){return 0;}
namespace network_setup {
inline bool CredentialPersistenceAllowed(){return permission;}
inline void NotifyCredentialsSaved(bool value){saved=value;}
inline bool CredentialSaveSucceeded(){return saved;}
}
namespace companion::network {inline bool DrainStationEvents(){return true;}}
struct SsidManager{static SsidManager& GetInstance(){static SsidManager s;return s;}
 void AddSsid(const std::string& ssid,const std::string& password){assert(ssid=="chosen"&&password=="password");
  if(racing){std::unique_lock<std::mutex> lock(gate_mutex);commit_waiting=true;gate_cv.notify_all();gate_cv.wait(lock,[]{return allow_commit;});}
  ++saves;saved=save_ok;}};
'''
# All reason identifiers used by the real diagnostic mapping remain distinct.
reasons = sorted(set(re.findall(r'WIFI_REASON_\w+', (station / 'wifi_failure.h').read_text())))
stub += '\n' + '\n'.join(f'constexpr int {reason}={100+i};' for i, reason in enumerate(reasons))
test = r'''
#include "sdk.h"
#include "companion/station_authority.hpp"
#define private public
#include "wifi_station.h"
#undef private
#include "wifi_station.cc"
using companion::network::Authority;using companion::network::Owner;
void associate(StackChanWifiStation& station){
 wifi_event_sta_connected_t event{};memcpy(event.ssid,"chosen",6);event.ssid_len=6;
 StackChanWifiStation::WifiEventHandler(&station,WIFI_EVENT,WIFI_EVENT_STA_CONNECTED,&event);
 memcpy(ap.ssid,"chosen",7);ap_live=true;
}
void address(StackChanWifiStation& station,esp_netif_t* netif){
 ip_event_got_ip_t event{netif,{0}};
 StackChanWifiStation::IpEventHandler(&station,IP_EVENT,IP_EVENT_STA_GOT_IP,&event);
}
int main(){
 assert(Authority().Acquire(Owner::Vendor));
 {StackChanWifiStation station;assert(station.Start());
 int failures=0,connected=0;std::string code;
 station.OnConnectFailed([&](const std::string&,const WifiConnectionFailure& failure){++failures;code=failure.code;});
 station.OnConnected([&](const std::string&){++connected;});
 assert(!station.AddAuth("", "password")&&!station.AddAuth("chosen","short"));
 permission=false;assert(!station.AddAuth("chosen","password"));permission=true;
 assert(config_calls==0&&saves==0);
 WifiBandInfo band;band.reported_5ghz=true;assert(station.AddAuth("chosen","password",band));
 now=30000000;station.Poll();assert(code=="not_2g4"&&failures==1&&station.pending_password_.empty()&&saves==0);
 now=0;assert(station.AddAuth("chosen","password"));associate(station);
 now=30000000;station.Poll();assert(code=="dhcp_timeout"&&failures==2&&saves==0);
 now=0;config_error=ESP_FAIL;assert(!station.AddAuth("chosen","password"));
 assert(code=="invalid_configuration"&&failures==3);config_error=0;
 connect_error=ESP_FAIL;assert(!station.AddAuth("chosen","password"));
 assert(code=="connect_failed"&&failures==4);connect_error=0;
 assert(station.AddAuth("chosen","password"));
 wifi_event_sta_disconnected_t disconnected{WIFI_REASON_AUTH_FAIL};
 StackChanWifiStation::WifiEventHandler(&station,WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,&disconnected);
 StackChanWifiStation::WifiEventHandler(&station,WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,&disconnected);
 assert(code=="authentication_failed"&&failures==5&&saves==0);
 assert(station.AddAuth("chosen","password"));associate(station);
 esp_netif_t other;address(station,&other);assert(saves==0);
 memcpy(ap.ssid,"wrong",6);address(station,station.station_netif_);assert(saves==0);
 memcpy(ap.ssid,"chosen",7);save_ok=false;address(station,station.station_netif_);
 assert(saves==1&&!station.IsConnected()&&!connected&&station.pending_password_.empty());
 save_ok=true;assert(station.AddAuth("chosen","password"));associate(station);address(station,station.station_netif_);
 assert(saves==2&&station.IsConnected()&&connected==1&&station.pending_password_.empty());
 // Starting a same-SSID replacement cannot accept stale association/IP.
 assert(station.AddAuth("chosen","password"));address(station,station.station_netif_);assert(saves==2);
 assert(station.awaiting_disconnect_);
 StackChanWifiStation::WifiEventHandler(&station,WIFI_EVENT,WIFI_EVENT_STA_DISCONNECTED,&disconnected);
 address(station,station.station_netif_);assert(saves==2);associate(station);
 permission=false;address(station,station.station_netif_);assert(saves==2);permission=true;
 Authority().RequestStop(Owner::Vendor);address(station,station.station_netif_);assert(saves==2);
 assert(!station.AddAuth("chosen","password"));
 fail_stop=1;assert(!station.StopChecked()&&netifs==1);fail_stop=0;
 assert(station.StopChecked()&&netifs==0&&station.pending_password_.empty());
 assert(Authority().FinishStop(Owner::Vendor,true));
 assert(Authority().Acquire(Owner::Saved));
 assert(!station.Start());Authority().RequestStop(Owner::Saved);assert(Authority().FinishStop(Owner::Saved,true));
 }
 // Deterministically schedule the real IP commit between Poll's timeout
 // snapshot and FailPending's mutex claim. The copied production source has
 // one inert scheduling hook at that boundary; cleanup code is unmodified.
 assert(Authority().Acquire(Owner::Vendor));ap_live=false;now=0;
 {StackChanWifiStation station;assert(station.Start());int failed=0,connected=0;
 station.OnConnectFailed([&](const std::string&){++failed;});
 station.OnConnected([&](const std::string&){++connected;});
 assert(station.AddAuth("chosen","password"));associate(station);now=30000000;racing=true;
 std::thread timeout([&]{timeout_thread=true;station.Poll();});
 {std::unique_lock<std::mutex> lock(gate_mutex);gate_cv.wait(lock,[]{return timeout_waiting;});}
 std::thread commit([&]{address(station,station.station_netif_);});
 {std::unique_lock<std::mutex> lock(gate_mutex);gate_cv.wait(lock,[]{return commit_waiting;});allow_timeout=true;gate_cv.notify_all();}
 timeout.join();assert(failed==0&&station.pending_&&station.committing_);
 {std::lock_guard<std::mutex> lock(gate_mutex);allow_commit=true;gate_cv.notify_all();}
 commit.join();assert(connected==1&&station.IsConnected()&&saves==3);racing=false;
 Authority().RequestStop(Owner::Vendor);assert(station.StopChecked());assert(Authority().FinishStop(Owner::Vendor,true));
 }
}
'''
with tempfile.TemporaryDirectory(prefix='stackchan-phone-reconciliation-') as temporary:
    directory = Path(temporary)
    (directory / 'sdk.h').write_text(stub)
    for name in ['freertos/FreeRTOS.h', 'freertos/event_groups.h', 'esp_event.h', 'esp_netif.h',
                 'esp_wifi_types_generic.h', 'esp_wifi.h', 'esp_wifi_default.h', 'esp_system.h',
                 'esp_timer.h', 'ssid_manager.h', 'companion/station_event_barrier.hpp']:
        path = directory / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#include "sdk.h"\n')
    for name in ['wifi_station.h', 'wifi_station.cc', 'wifi_failure.h']:
        (directory / name).write_bytes((station / name).read_bytes())
    path = directory / 'wifi_station.cc'
    instrumented, count = re.subn(r'(void StackChanWifiStation::FailPending[^\{]+\{)',
                                 r'\1\n    before_failure();', path.read_text(), count=1)
    assert count == 1
    path.write_text(instrumented)
    cpp, exe = directory / 'phone.cpp', directory / 'phone'
    cpp.write_text(test)
    subprocess.run([shutil.which('clang++') or 'c++', '-std=c++17', '-fsanitize=address,undefined',
                    '-I' + str(directory), '-I' + str(firmware / 'main'), str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('All-channel/signal policy and complete config wipe before connect; full production phone station: diagnostics, credentials, exact link, persistence, epoch and checked stop passed')
