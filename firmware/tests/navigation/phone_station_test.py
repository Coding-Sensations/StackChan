"""Execute actual phone station Start/Stop and generated manager Initialize.
Verify saved -> phone -> vendor and vendor -> phone -> vendor using inert SDK.
"""
import pathlib,importlib.util,subprocess,tempfile,shutil
f=pathlib.Path(__file__).resolve().parents[2];phone=(f/'main/hal/utils/wifi_connect/wifi_station.cc').read_text()
def extract(source,signature):
 start=source.index(signature);p=source.index('{',start);d=1;e=p+1
 while d:d+=(source[e]=='{')-(source[e]=='}');e+=1
 return source[start:e]
spec=importlib.util.spec_from_file_location('backend',f/'main/apps/stock_agent/generate_backend.py');backend=importlib.util.module_from_spec(spec);spec.loader.exec_module(backend)
with tempfile.TemporaryDirectory(prefix='phone-station-test-') as tmp:
 tmp=pathlib.Path(tmp);backend.generate(f/'xiaozhi-esp32/main',f/'managed_components/78__esp-wifi-connect',tmp)
 manager=(tmp/'stock_wifi_manager.cc').read_text()
 code=r'''
#include "companion/station_authority.hpp"
#include <atomic>
#include <cassert>
#include <memory>
#include <mutex>
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
constexpr int ESP_OK=0,ESP_FAIL=-1,ESP_ERR_WIFI_NOT_INIT=-2,ESP_ERR_WIFI_NOT_STARTED=-3,ESP_ERR_INVALID_STATE=-4,ESP_ERR_WIFI_INIT_STATE=-5,ESP_ERR_NVS_NO_FREE_PAGES=-6,ESP_ERR_NVS_NEW_VERSION_FOUND=-7,WIFI_MODE_NULL=0,WIFI_MODE_STA=1,WIFI_EVENT=1,IP_EVENT=2,ESP_EVENT_ANY_ID=-1,IP_EVENT_STA_GOT_IP=3,WIFI_STORAGE_RAM=1;
using wifi_mode_t=int;using esp_err_t=int;
struct wifi_init_config_t{bool nvs_enable=true;};
#define WIFI_INIT_CONFIG_DEFAULT() wifi_init_config_t{}
bool driver_initialized=false,driver_running=false,permission=true;int mode=0,fail=0,netifs=0;
int nvs_flash_init(){return 0;}int esp_netif_init(){return 0;}int esp_event_loop_create_default(){return ESP_ERR_INVALID_STATE;}
int esp_wifi_init(wifi_init_config_t*){driver_initialized=true;return 0;} // pinned SDK is idempotent
int esp_wifi_get_mode(wifi_mode_t* value){*value=mode;return driver_initialized?ESP_OK:ESP_ERR_WIFI_NOT_INIT;}
int esp_wifi_set_mode(int value){if(!driver_initialized)return ESP_ERR_WIFI_NOT_INIT;mode=value;return 0;}
int esp_wifi_set_storage(int){return driver_initialized?ESP_OK:ESP_ERR_WIFI_NOT_INIT;}
int esp_wifi_start(){if(fail==3)return ESP_FAIL;driver_running=true;return driver_initialized?0:ESP_ERR_WIFI_NOT_INIT;}
int esp_wifi_stop(){if(fail==4)return ESP_FAIL;driver_running=false;return driver_initialized?ESP_OK:ESP_ERR_WIFI_NOT_INIT;}
void* esp_netif_create_default_wifi_sta(){++netifs;return (void*)3;}
void esp_netif_destroy_default_wifi(void*){assert(mode==WIFI_MODE_NULL&&!driver_running);--netifs;}
int esp_event_handler_instance_register(int base,int,void(*)(void*,int,int,void*),void*,void** handle){if(fail==base)return ESP_FAIL;*handle=(void*)1;return 0;}
int esp_event_handler_instance_unregister(int base,int,void*){if(fail==base)return ESP_FAIL;return 0;}
namespace network_setup{bool CredentialPersistenceAllowed(){return permission;}}
namespace companion::network{bool DrainStationEvents(){return fail!=5;}}
class StackChanWifiStation{public:
 std::mutex mutex_;void* event_group_=(void*)1;void* instance_any_id_=nullptr;void* instance_got_ip_=nullptr;void* station_netif_=nullptr;
 bool driver_touched_=false,is_started_=false;std::atomic<bool> callbacks_enabled_{false};uint64_t network_generation_=0;
 void WipePendingLocked(){};
 static void WifiEventHandler(void*,int,int,void*){};static void IpEventHandler(void*,int,int,void*){};
 bool Start();bool StopChecked();};
struct WifiManagerConfig{};struct WifiStation{};struct WifiConfigurationAp{};
struct WifiManager{std::mutex mutex_;bool initialized_=false;WifiManagerConfig config_;std::unique_ptr<WifiStation> station_;std::unique_ptr<WifiConfigurationAp> config_ap_;bool Initialize(const WifiManagerConfig&);};
'''
 code+=extract(phone,'bool StackChanWifiStation::Start()')+'\n'+extract(phone,'bool StackChanWifiStation::StopChecked()')+'\n'
 code+=extract(manager,'bool WifiManager::Initialize(const WifiManagerConfig& config)')+r'''
int main(){
 auto& authority=companion::network::Authority();using companion::network::Owner;
 WifiManager manager;WifiManagerConfig config;
 for(int repeat=0;repeat<50;++repeat){
  assert(authority.Acquire(Owner::Saved));driver_initialized=true;mode=WIFI_MODE_NULL;
  StackChanWifiStation denied;assert(!denied.Start());assert(netifs==0);
  authority.RequestStop(Owner::Saved);assert(authority.FinishStop(Owner::Saved,true));
  assert(authority.Acquire(Owner::Vendor));assert(manager.Initialize(config));
  StackChanWifiStation phone;assert(phone.Start());assert(mode==WIFI_MODE_STA&&netifs==1);
  authority.RequestStop(Owner::Vendor);fail=1;assert(!phone.StopChecked());assert(netifs==1);assert(!authority.Acquire(Owner::Saved));
  fail=4;assert(!phone.StopChecked());assert(netifs==1);fail=5;assert(!phone.StopChecked());assert(netifs==1);fail=0;assert(phone.StopChecked());assert(netifs==0&&driver_initialized&&mode==WIFI_MODE_NULL);
  assert(authority.FinishStop(Owner::Vendor,true));assert(authority.Acquire(Owner::Vendor));
  assert(manager.initialized_&&manager.Initialize(config));assert(esp_wifi_set_mode(WIFI_MODE_STA)==ESP_OK); // manager may early-return; driver is still valid
  assert(esp_wifi_stop()==ESP_OK);assert(esp_wifi_set_mode(WIFI_MODE_NULL)==ESP_OK);authority.RequestStop(Owner::Vendor);assert(authority.FinishStop(Owner::Vendor,true));
 }
 assert(authority.Acquire(Owner::Vendor));StackChanWifiStation partial;fail=2;assert(!partial.Start());fail=0;assert(partial.StopChecked());assert(netifs==0&&driver_initialized);authority.RequestStop(Owner::Vendor);assert(authority.FinishStop(Owner::Vendor,true));
}
'''
 cpp=tmp/'phone.cpp';cpp.write_text(code);exe=tmp/'phone'
 subprocess.run([shutil.which('clang++')or'c++','-std=c++17','-fsanitize=address,undefined','-I'+str(f/'main'),str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
 print('50 saved/phone/vendor and vendor/phone/vendor cycles preserve initialized driver; real Start/Stop partial/failure cases passed')
