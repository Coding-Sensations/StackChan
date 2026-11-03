"""Run production ESP-NOW stop with queued callbacks and a failed drain; no hardware."""
from pathlib import Path
import shutil
import subprocess
import tempfile

firmware = Path(__file__).resolve().parents[2]
source = (firmware / 'main/hal/hal_espnow.cpp').read_text()
body = source[source.index('bool Hal::stopEspNow()'):]
fixture = r'''
#include <cassert>
#include <functional>
#include <vector>
constexpr int ESP_OK=0, ESP_ERR_WIFI_NOT_STARTED=1, ESP_FAIL=-1;
constexpr int WIFI_EVENT=1, IP_EVENT=2, ESP_EVENT_ANY_ID=-1, IP_EVENT_STA_GOT_IP=3;
struct Group { bool alive=true; };
Group group;
void* home_sta_netif=(void*)1;
void* home_wifi_handler=(void*)2;
void* home_ip_handler=(void*)3;
bool home_espnow_started=true;
Group* s_wifi_event_group=&group;
bool drain_ok=false, wifi_ok=true;
int callbacks=0, drains=0, deinitializations=0, destroys=0, deletes=0;
std::vector<std::function<void()>> queued;
int espnow_deinit(){++deinitializations;return ESP_OK;}
int esp_event_handler_instance_unregister(int,int,void*){return ESP_OK;}
int esp_wifi_stop(){return wifi_ok?ESP_OK:ESP_FAIL;}
void esp_netif_destroy_default_wifi(void*){assert(queued.empty());++destroys;}
void vEventGroupDelete(Group* value){assert(queued.empty());assert(value->alive);value->alive=false;++deletes;}
namespace companion::network {
bool DrainStationEvents(){
 ++drains;
 assert(home_wifi_handler==nullptr&&home_ip_handler==nullptr);
 assert(home_sta_netif!=nullptr&&s_wifi_event_group!=nullptr&&s_wifi_event_group->alive);
 if(!drain_ok)return false;
 for(auto& callback:queued)callback();
 queued.clear();return true;
}
}
struct Hal { bool stopEspNow(); };
'''
checks = r'''
int main(){
 Hal hal;
 queued.push_back([]{assert(s_wifi_event_group&&s_wifi_event_group->alive);++callbacks;});
 assert(!hal.stopEspNow()); // timeout must retain the netif/group and callback ownership
 assert(drains==1&&callbacks==0&&destroys==0&&deletes==0&&group.alive);
 assert(!home_espnow_started&&home_wifi_handler==nullptr&&home_ip_handler==nullptr);
 drain_ok=true;assert(hal.stopEspNow());
 assert(callbacks==1&&drains==2&&destroys==1&&deletes==1&&!group.alive);
 assert(home_sta_netif==nullptr&&s_wifi_event_group==nullptr&&deinitializations==1);
 assert(hal.stopEspNow());assert(drains==2&&destroys==1&&deletes==1);
 // A failed wifi stop retains resources without falsely reporting closure.
 group.alive=true;home_sta_netif=(void*)1;s_wifi_event_group=&group;wifi_ok=false;
 assert(!hal.stopEspNow());assert(group.alive&&home_sta_netif&&s_wifi_event_group);
 wifi_ok=true;assert(hal.stopEspNow());assert(destroys==2&&deletes==2);
}
'''
with tempfile.TemporaryDirectory(prefix='espnow-stop-test-') as directory:
    directory = Path(directory)
    cpp = directory / 'stop.cpp'
    cpp.write_text(fixture + body + checks)
    binary = directory / 'stop'
    subprocess.run([shutil.which('clang++') or 'c++', '-std=c++17', '-fsanitize=address,undefined',
                    str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('ESP-NOW queued callback drain, timeout retention, retry and repeated close passed')
