"""Model queued events being resolved against handlers at dequeue time.
Run production default-loop barrier so old events cannot reach a new owner.
"""
import pathlib,subprocess,tempfile,shutil
f=pathlib.Path(__file__).resolve().parents[2];s=(f/'main/companion/station_event_barrier.cc').read_text();s=s[s.index('ESP_EVENT_DEFINE_BASE'):]
code=r'''
#include <atomic>
#include <limits>
#include <mutex>
#include <cassert>
#include <functional>
#include <deque>
#include <cstdint>
constexpr int ESP_OK=0,ESP_FAIL=-1,ESP_ERR_INVALID_STATE=-2,ESP_ERR_WIFI_NOT_INIT=-3;
using esp_err_t=int;using wifi_mode_t=int;using esp_event_base_t=int;using esp_event_handler_instance_t=void*;
#define ESP_EVENT_DEFINE_BASE(x) constexpr int x=2
#define pdMS_TO_TICKS(x) (x)
std::deque<std::function<void()>> events;bool configured=false,connected=false,post_ok=true,dispatch=true,initialized=true;
int64_t now=0;std::function<void(void*)> acknowledge;
int esp_wifi_get_mode(int*){return initialized?ESP_OK:ESP_ERR_WIFI_NOT_INIT;}
int esp_event_handler_instance_register(int,int,void(*callback)(void*,int,int32_t,void*),void*,void** handle){*handle=(void*)1;acknowledge=[callback](void* data){callback(nullptr,2,0,data);};return 0;}
int esp_event_post(int,int,const void* data,size_t,int){if(!post_ok)return ESP_FAIL;uint32_t ticket=*static_cast<const uint32_t*>(data);events.push_back([ticket]()mutable{acknowledge(&ticket);});return 0;}
int64_t esp_timer_get_time(){return now;}
void vTaskDelay(int){now+=1000;if(dispatch&&!events.empty()){auto event=events.front();events.pop_front();event();}}
'''+s+r'''
int main(){
 for(int repeat=0;repeat<100;++repeat){
  configured=false;connected=false;
  events.push_back([]{if(configured)connected=true;}); // old queued GOT_IP resolves newly registered handlers at dispatch time
  assert(companion::network::DrainStationEvents(20));assert(events.empty());
  configured=true;assert(!connected); // the replacement handler cannot see that old event
 }
 configured=false;events.push_back([]{if(configured)connected=true;});dispatch=false;
 assert(!companion::network::DrainStationEvents(2));assert(!events.empty()); // failure cannot grant ownership
 dispatch=true;assert(companion::network::DrainStationEvents(20));assert(events.empty());
 post_ok=false;assert(!companion::network::DrainStationEvents(20));
}
'''
with tempfile.TemporaryDirectory(prefix='station-event-test-') as tmp:
 tmp=pathlib.Path(tmp);cpp=tmp/'events.cpp';cpp.write_text(code);exe=tmp/'events'
 subprocess.run([shutil.which('clang++')or'c++','-std=c++17','-fsanitize=address,undefined',str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
 print('100 queued old-IP handoffs, delayed acknowledgement and failed-post cases passed')
