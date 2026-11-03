"""Run generated protocol/native destruction bodies with callbacks during stop."""
import importlib.util,pathlib,subprocess,tempfile,shutil
firmware=pathlib.Path(__file__).resolve().parents[2]
xiaozhi=firmware/'xiaozhi-esp32/main';wifi=firmware/'managed_components/78__esp-wifi-connect'
if not (xiaozhi/'application.cc').exists() or not (wifi/'wifi_manager.cc').exists():raise SystemExit(77)
spec=importlib.util.spec_from_file_location('backend',firmware/'main/apps/stock_agent/generate_backend.py');backend=importlib.util.module_from_spec(spec);spec.loader.exec_module(backend)
def body(source,signature):
 start=source.index(signature);p=source.index('{',start);d=1;e=p+1
 while d:d+=(source[e]=='{')-(source[e]=='}');e+=1
 return source[start:e]
with tempfile.TemporaryDirectory(prefix='stackchan-owner-test-') as tmp:
 tmp=pathlib.Path(tmp);backend.generate(xiaozhi,wifi,tmp)
 source=r'''
#include <cassert>
#include <memory>
#include <functional>
#define ESP_LOGI(...) ((void)0)
struct Group{bool alive=true;};
void vEventGroupDelete(Group* g){assert(g&&g->alive);g->alive=false;}
bool mqtt_stopped=false;
void esp_timer_stop(Group* timer){assert(timer->alive&&mqtt_stopped);}
void esp_timer_delete(Group* timer){assert(timer->alive&&mqtt_stopped);timer->alive=false;}
struct Socket{Group* group;~Socket(){assert(group->alive);}};
struct Mqtt{Group* timer;Group* group;bool stopped=false;
 void Disconnect(){assert(timer->alive&&group->alive);stopped=mqtt_stopped=true;}
 ~Mqtt(){assert(stopped);}
};
struct WebsocketProtocol{Group* event_group_handle_;std::unique_ptr<Socket> websocket_;~WebsocketProtocol();};
struct MqttProtocol{Group* event_group_handle_;Group* reconnect_timer_;std::shared_ptr<bool> alive_=std::make_shared<bool>(true);std::unique_ptr<Mqtt> mqtt_;std::unique_ptr<Socket> udp_;~MqttProtocol();};
'''
 for name,cls in [('websocket','WebsocketProtocol'),('mqtt','MqttProtocol')]:source+=body((tmp/f'stock_{name}_protocol.cc').read_text(),f'{cls}::~{cls}()')+'\n'
 dns=(tmp/'stock_dns_server.cc').read_text()
 source+=dns.split('#include "dns_server.h"')[0]
 source+=r'''
#define pdMS_TO_TICKS(x) (x)
constexpr int SHUT_RDWR=2;
struct DnsServer{bool running_=false;int fd_=1;void* task_handle_=(void*)1;void Stop();};
int dns_polls=0;DnsServer* dns_owner=nullptr;
void vTaskDelay(int){assert(dns_owner&&dns_owner->task_handle_);if(++dns_polls==3)home_dns_publish(dns_owner);}
void shutdown(int,int){}void close(int){}
'''
 source+=body(dns,'void DnsServer::Stop()')+'\n'
 ezdata=(firmware/'main/hal/hal_ezdata.cpp').read_text();shutdown=body(ezdata,'void Shutdown()');destroy=body(ezdata,'void onDestroy() override')
 source+=r'''
std::function<void()> during_stop;
struct NativeMqtt{void Disconnect(){during_stop();}};
struct Service{std::unique_ptr<NativeMqtt> _mqtt=std::make_unique<NativeMqtt>();
'''+shutdown+r'''};
struct Ability{virtual void onDestroy(){};virtual ~Ability()=default;};
struct Worker:Ability{std::unique_ptr<Service> _service=std::make_unique<Service>();
'''+destroy+r'''};
int main(){
 for(int repeat=0;repeat<50;++repeat){
  Group ws;{WebsocketProtocol p{&ws,std::make_unique<Socket>(Socket{&ws})};}assert(!ws.alive);
  Group event,timer; mqtt_stopped=false;
  {MqttProtocol p{&event,&timer};p.mqtt_=std::make_unique<Mqtt>(Mqtt{&timer,&event});p.udp_=std::make_unique<Socket>(Socket{&event});}assert(!event.alive&&!timer.alive);
  DnsServer dns;dns_owner=&dns;dns_polls=0;home_dns_arm(&dns);dns.Stop();assert(dns_polls==3&&!dns.task_handle_);dns.Stop();assert(dns_polls==3);
  Worker worker;during_stop=[&]{assert(worker._service&&worker._service->_mqtt);};worker.onDestroy();assert(!worker._service);
 }
}
'''
 # Avoid temporary fake Mqtt destruction before Disconnect.
 source=source.replace('std::make_unique<Mqtt>(Mqtt{&timer,&event})','std::unique_ptr<Mqtt>(new Mqtt{&timer,&event})')
 cpp=tmp/'owner.cpp';cpp.write_text(source);exe=tmp/'owner'
 subprocess.run([shutil.which('clang++') or 'c++','-std=c++17','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
 print('50 repeated protocol/native stops preserve callback state and pointer validity until join')
