"""Compile the actual saved-station SDK stop adapter and owner polling body.
SDK/task stand-ins inject partial init, borrow, task-create and stop failures.
"""
import pathlib,subprocess,tempfile,shutil
firmware=pathlib.Path(__file__).resolve().parents[2];original=(firmware/'main/companion/boot.cc').read_text()
def extract(signature):
 start=original.index(signature);pos=original.index('{',start);depth=1;end=pos+1
 while depth:depth+=(original[end]=='{')-(original[end]=='}');end+=1
 return original[start:end]
code=r'''
#include "companion/station_authority.hpp"
#include "companion/checked_station_stop.hpp"
#include "companion/saved_wifi_owner.hpp"
#include <atomic>
#include <cassert>
#include <memory>
#include <functional>
constexpr int ESP_OK=0,ESP_FAIL=-1,ESP_ERR_WIFI_NOT_INIT=-2,ESP_ERR_WIFI_NOT_STARTED=-3,WIFI_MODE_NULL=0,WIFI_MODE_STA=1,WIFI_EVENT=1,IP_EVENT=2,ESP_EVENT_ANY_ID=-1,IP_EVENT_STA_GOT_IP=3,pdPASS=1;
using wifi_mode_t=int;void* saved_wifi_handler=nullptr;void* saved_ip_handler=nullptr;void* saved_station_netif=nullptr;
namespace companion::network {bool DrainStationEvents(){return true;}}
namespace network=companion::network;
companion::SavedWifiOwner saved_wifi_owner;
std::mutex wifi_start_mutex;std::atomic<int> saved_stop_job{0};
std::atomic<bool> connected{false},wifi_survey_busy{false};
bool consumers_idle=true,create_ok=true,radio_initialized=true;int failing_step=0,mode=WIFI_MODE_STA,stops=0,destroys=0,jobs=0;
struct Callback{~Callback(){assert(!saved_wifi_handler&&!saved_ip_handler);}};std::unique_ptr<Callback> saved_callback;
int esp_event_handler_instance_unregister(int base,int,void* handle){assert(handle);if(failing_step==(base==WIFI_EVENT?1:2))return ESP_FAIL;return ESP_OK;}
int esp_wifi_stop(){++stops;if(failing_step==3)return ESP_FAIL;return radio_initialized?ESP_OK:ESP_ERR_WIFI_NOT_INIT;}
int esp_wifi_set_mode(int value){mode=value;return ESP_OK;}
int esp_wifi_get_mode(wifi_mode_t* value){if(failing_step==5)return ESP_FAIL;*value=mode;return radio_initialized?ESP_OK:ESP_ERR_WIFI_NOT_INIT;}
void esp_netif_destroy_default_wifi(void* netif){assert(netif&&!saved_wifi_handler&&!saved_ip_handler);assert(!radio_initialized||mode==WIFI_MODE_NULL);++destroys;}
std::function<void()> task;
int xTaskCreate(void(*work)(void*),const char*,int,void*,int,void*){++jobs;if(!create_ok)return 0;assert(!task);task=[work]{work(nullptr);};return pdPASS;}
void vTaskDelete(void*){}
void run_task(){auto current=std::move(task);task={};if(current)current();}
bool FeatureAudioIdle(){return consumers_idle;}
struct Runtime{std::atomic<bool> wifi_started{true};};Runtime runtime_value;Runtime& runtime(){return runtime_value;}
struct Survey{bool owns=false;void Poll(int){}bool OwnsScan(){return owns;}void Cancel(){owns=false;}} wifi_survey;
int esp_timer_get_time(){return 0;}bool esp_sntp_enabled(){return false;}void esp_sntp_stop(){}
'''+extract('struct SavedStationStopDriver')+';\n'+extract('bool PollSavedNetworkStopped()')+r'''
int main(){
 auto& a=network::Authority();
 for(int repeat=0;repeat<50;++repeat){
  assert(a.Acquire(network::Owner::Saved));saved_wifi_owner.Begin(true);saved_wifi_owner.Touched();
  saved_wifi_handler=(void*)1;saved_ip_handler=(void*)2;saved_station_netif=(void*)3;saved_callback=std::make_unique<Callback>();
  connected=true;runtime().wifi_started=true;radio_initialized=true;mode=WIFI_MODE_STA;create_ok=true;failing_step=0;consumers_idle=false;
  auto borrow=a.BorrowSaved();int before=jobs;assert(!PollSavedNetworkStopped());assert(jobs==before);assert(!a.Acquire(network::Owner::Vendor));
  consumers_idle=true;assert(!PollSavedNetworkStopped());assert(jobs==before);borrow.Reset();
  wifi_survey.owns=true;assert(!PollSavedNetworkStopped()); // scan cancellation has cleared this fake scan
  if(task)run_task(); // real scan waits its completion event
  if(a.Current()==network::Owner::None){ // start another partial-init session for retry injections
   assert(a.Acquire(network::Owner::Saved));saved_station_netif=(void*)3;saved_wifi_handler=(void*)1;saved_ip_handler=(void*)2;saved_callback=std::make_unique<Callback>();mode=WIFI_MODE_STA;
  }
  create_ok=false;assert(!PollSavedNetworkStopped());assert(!task&&a.Current()==network::Owner::Saved);create_ok=true;
  for(int step:{1,2,3}){failing_step=step;assert(!PollSavedNetworkStopped());assert(task);run_task();assert(a.Current()==network::Owner::Saved);assert(saved_station_netif&&saved_callback);assert(!a.Acquire(network::Owner::Vendor));}
  failing_step=0;assert(!PollSavedNetworkStopped());int count=jobs;assert(!PollSavedNetworkStopped());assert(jobs==count);run_task();
  assert(PollSavedNetworkStopped());assert(!saved_station_netif&&!saved_callback&&!connected&&!runtime().wifi_started);
  assert(a.Acquire(network::Owner::Vendor));assert(!a.Acquire(network::Owner::Saved));a.RequestStop(network::Owner::Vendor);assert(a.FinishStop(network::Owner::Vendor,true));
 }
 // A partially initialized adapter with no driver or handlers also closes safely.
 assert(a.Acquire(network::Owner::Saved));radio_initialized=false;saved_station_netif=(void*)3;assert(!PollSavedNetworkStopped());run_task();assert(PollSavedNetworkStopped());
}
'''
with tempfile.TemporaryDirectory(prefix='station-sdk-test-') as tmp:
 tmp=pathlib.Path(tmp);cpp=tmp/'station.cpp';cpp.write_text(code);exe=tmp/'station'
 subprocess.run([shutil.which('clang++')or'c++','-std=c++17','-fsanitize=address,undefined','-I'+str(firmware/'main'),str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
 print('Actual saved SDK stop/poll bodies: 50 owner/borrow/failure/retry cycles and partial-init cleanup passed')
