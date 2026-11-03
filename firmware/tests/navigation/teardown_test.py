"""Exercise actual generated shutdown bodies with inert SDK stand-ins.
No radios, audio, motion, firmware upload or device access is involved.
"""
import importlib.util
import pathlib
import shutil
import subprocess
import tempfile

firmware = pathlib.Path(__file__).resolve().parents[2]
network = firmware / 'managed_components/78__esp-ml307'
espnow = firmware / 'components/esp-now'
if not (network/'src/esp/esp_tcp.cc').exists() or not (espnow/'src/espnow/src/espnow.c').exists():
    print('Pinned SDK cache unavailable; target adapter test needs project dependencies')
    raise SystemExit(77)
spec=importlib.util.spec_from_file_location('adapter',firmware/'main/apps/runtime_lifecycle/generate_teardown.py')
adapter=importlib.util.module_from_spec(spec);spec.loader.exec_module(adapter)
def function(source, signature):
    start=source.index(signature); pos=source.index('{',start); depth=1; end=pos+1
    while depth:
        depth += (source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]

with tempfile.TemporaryDirectory(prefix='stackchan-teardown-test-') as tmp:
    tmp=pathlib.Path(tmp);adapter.generate(network,espnow,tmp)
    source=r'''
#include <cassert>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
constexpr int ESP_OK=0,ESP_FAIL=-1,ESP_ERR_INVALID_STATE=-2,ESP_ERR_ESPNOW_NOT_INIT=-3;
constexpr int SHUT_RDWR=2,pdFALSE=0,BIT0=1,ESPNOW_EVENT_STOP=1,ESPNOW_DATA_TYPE_MAX=4,WIFI_EVENT=1,ESP_EVENT_ANY_ID=-1;
constexpr int MQTT_CONNECTED_EVENT=1,MQTT_DISCONNECTED_EVENT=2,MQTT_ERROR_EVENT=4;
constexpr int ESP_TCP_EVENT_RECEIVE_TASK_EXIT=1,ESP_UDP_EVENT_RECEIVE_TASK_EXIT=1,ESP_SSL_EVENT_RECEIVE_TASK_EXIT=1;
#define pdMS_TO_TICKS(x) (x)
using esp_err_t=int;
struct Group{std::atomic<int> bits{0};int polls=0;};
std::mutex home_tcp_receiver_exit_mutex,home_udp_receiver_exit_mutex,home_ssl_receiver_exit_mutex;
std::atomic<bool> kernel_window{false},setter_return{false},waiter_seen{false};
std::function<void()> during_wait;
void vTaskDelay(int){}
int xEventGroupWaitBits(Group* group,int,int,int,int){
 assert(group);++group->polls;
 if(kernel_window){while(!group->bits.load())std::this_thread::yield();waiter_seen=true;return group->bits.load();}
 if(during_wait)during_wait();
 if(group->polls==3)group->bits=1;
 return group->bits.load();
}
void xEventGroupClearBits(Group*,int){}
void xEventGroupSetBits(Group* group,int bits){group->bits=bits;while(!setter_return.load())std::this_thread::yield();}
int xEventGroupGetBits(Group* group){assert(group);return group->bits.load();}
int shutdown(int,int){return 0;}int close(int){return 0;}
struct EspTcp{bool connected_=false;int tcp_fd_=1;void* receive_task_handle_=(void*)1;Group* event_group_;void Disconnect();};
struct EspUdp{bool connected_=false;int udp_fd_=1;void* receive_task_handle_=(void*)1;Group* event_group_;void Disconnect();};
struct EspSsl{bool connected_=false;void* tls_client_=(void*)1;void* receive_task_handle_=(void*)1;Group* event_group_;void Disconnect();};
bool tls_destroyed=false;Group* tls_group;
int esp_tls_get_conn_sockfd(void*,int* fd){*fd=3;return 0;}
void esp_tls_conn_destroy(void*){assert(tls_group->bits);tls_destroyed=true;}
int mqtt_stop_calls=0,mqtt_destroy_calls=0;bool mqtt_pointer_valid=true;
int esp_mqtt_client_stop(void*){assert(mqtt_pointer_valid);++mqtt_stop_calls;return mqtt_stop_calls<3?ESP_FAIL:ESP_OK;}
int esp_mqtt_client_destroy(void*){assert(mqtt_pointer_valid);return ++mqtt_destroy_calls<2?ESP_FAIL:ESP_OK;}
struct EspMqtt{Group* event_group_handle_=nullptr;void* mqtt_client_handle_=(void*)1;bool connected_=true;void Disconnect();};
struct Config{bool sec_enable=false;};struct Recv{int enable=1;void* handle=(void*)1;};
Config* g_espnow_config=nullptr;Recv g_recv_handle[ESPNOW_DATA_TYPE_MAX];
int* g_espnow_sec=nullptr;int* g_espnow_dec=nullptr;
Group* home_join_group=nullptr;bool home_driver_started=false,home_events_registered=false,home_worker_started=false;
void* g_send_lock=nullptr;Group* g_event_group=nullptr;
bool driver_failure=false;int freed=0;
int esp_now_unregister_recv_cb(){return 0;}int esp_now_unregister_send_cb(){return 0;}
int esp_now_deinit(){return driver_failure?ESP_FAIL:ESP_OK;}
int queue_over_write(int,void*,int,void*,int){return 1;}
int esp_event_handler_unregister(int,int,void(*)()){return 0;}
void wifi_event_handler(){}
void espnow_sec_deinit(void*){}
void vSemaphoreDelete(void*){++freed;}
void vEventGroupDelete(Group* group){assert(group);++freed;}
#define ESP_FREE(x) do{++freed;delete x;x=nullptr;}while(0)
'''
    for name,signature in [('tcp','void EspTcp::Disconnect()'),('udp','void EspUdp::Disconnect()'),('ssl','void EspSsl::Disconnect()'),('mqtt','void EspMqtt::Disconnect()')]:
        source+=function((tmp/f'joined_esp_{name}.cc').read_text(),signature)+'\n'
    for kind in ['tcp','udp','ssl']:
        generated=(tmp/f'joined_esp_{kind}.cc').read_text()
        start=generated.index('{std::lock_guard<std::mutex> exit_lock(')
        end=generated.index('} // last owner access',start)+1
        block=generated[start:end]
        source+=f'void publish_{kind}(Esp{kind.title()}* {kind})'+block+'\n'
    source+=function((tmp/'joined_espnow.c').read_text(),'esp_err_t espnow_deinit(void)')
    source+=r'''
int main(){
 for(int repeat=0;repeat<50;++repeat){
  Group tcp,udp,tls;EspTcp t{false,1,(void*)1,&tcp};EspUdp u{false,1,(void*)1,&udp};EspSsl s{false,(void*)1,(void*)1,&tls};
  during_wait=[&]{assert(t.receive_task_handle_);};t.Disconnect();assert(tcp.polls==3&&!t.receive_task_handle_);t.Disconnect();assert(tcp.polls==3);
  during_wait=[&]{assert(u.receive_task_handle_);};u.Disconnect();assert(udp.polls==3&&!u.receive_task_handle_);
  tls_destroyed=false;tls_group=&tls;during_wait=[&]{assert(!tls_destroyed&&s.tls_client_);};s.Disconnect();assert(tls_destroyed&&!s.tls_client_);
  EspMqtt m;mqtt_stop_calls=mqtt_destroy_calls=0;mqtt_pointer_valid=true;m.Disconnect();assert(!m.mqtt_client_handle_&&mqtt_stop_calls==3&&mqtt_destroy_calls==2);
 }
 during_wait={};
 // Model the kernel's post-unblock read. A close must wait for the actual
 // generated setter block to release its boot-lifetime mutex.
 for(int repeat=0;repeat<10;++repeat){
  Group group;EspTcp transport{false,1,(void*)1,&group};
  kernel_window=true;setter_return=false;waiter_seen=false;std::atomic<bool> closed{false};
  std::thread producer([&]{publish_tcp(&transport);});
  std::thread closer([&]{transport.Disconnect();closed=true;});
  while(!waiter_seen.load())std::this_thread::yield();
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  assert(!closed.load());setter_return=true;
  producer.join();closer.join();assert(closed.load());kernel_window=false;
 }
 assert(espnow_deinit()==ESP_OK); // event-group allocation failed before any side effect
 Group join,events;
 // Partial initialization before worker creation never dereferences absent receipt.
 g_espnow_config=new Config;home_join_group=nullptr;assert(espnow_deinit()==ESP_OK);
 g_espnow_config=new Config;home_driver_started=true;driver_failure=true;const int before=freed;
 assert(espnow_deinit()==ESP_FAIL&&g_espnow_config&&freed==before); // failed stop retains owner
 driver_failure=false;home_join_group=&join;home_worker_started=true;g_event_group=&events;g_send_lock=(void*)1;
 during_wait=[&]{assert(g_espnow_config&&g_event_group&&g_send_lock);};
 assert(espnow_deinit()==ESP_OK);assert(join.polls==3&&!g_espnow_config&&!home_worker_started);
}
'''
    cpp=tmp/'shutdown.cpp';cpp.write_text(source);exe=tmp/'shutdown'
    subprocess.run([shutil.which('clang++') or 'c++','-std=c++17','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    print('Generated TCP/UDP/TLS/MQTT/ESP-NOW shutdown: repeated, passive-disconnect, delayed-receipt and partial/failure cases passed')
