"""Compile the exact production transport with inert network/task boundaries."""
from pathlib import Path
import json,subprocess,sys,tempfile,shutil
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'companion/host/streaming'))
from muse_room_transport import OPEN
from provider_room_bridge import ROOM,encode_wire
E=b'event-fixture-key-not-a-credential!';D=b'decision-fixture-key-not-secret!!'
hello=dict(version=1,provider='muse',binding='binding',generation='transport-generation',kind='challenge',nonce='a'*32)
challenge=encode_wire(E,hello,OPEN)
frame=encode_wire(E,dict(version=1,provider='muse',binding='binding',generation='transport-generation',sequence=1,scene='waiting_receipt',summary='',request_id='',operation_sha256='',presentation_id='',ttl_ms=0,caption='Fixture transport caption.'),ROOM)
code=r'''#include "muse_room_transport.cc"
#include "companion/provider_room_runtime.cc"
#include <cassert>
uint64_t fixture_ms=1000;TaskHandle_t fixture_task=reinterpret_cast<void*>(1);
extern "C" const char* companion_local_ca_pem(){return "public-fixture-ca";}
extern "C" const char* companion_local_tls_host(){return "192.168.20.40";}
int main(){using namespace companion::room;using P=companion_modes::Provider;
 assert(BoundedWebSocket::created==0&&MuseRoomCleanupComplete());PollMuseRoom();StopMuseRoom();assert(BoundedWebSocket::created==0);
 NetworkInterface network;MuseRoomGrant grant;grant.uri="wss://192.168.20.41:18444/companion/muse-room";grant.binding="binding";grant.generation="transport-generation";grant.expires_ms=10000;bool network_allowed=true;grant.network_current=[&]{return network_allowed;};
 const unsigned char e[]="event-fixture-key-not-a-credential!",d[]="decision-fixture-key-not-secret!!";
 grant.event_size=sizeof(e)-1;grant.control_size=sizeof(d)-1;std::memcpy(grant.event.data(),e,grant.event_size);std::memcpy(grant.control.data(),d,grant.control_size);
 SelectProvider(P::Muse);assert(!StartMuseRoom(network,grant)&&BoundedWebSocket::created==0);
 grant.uri="wss://192.168.20.40:18444/companion/muse-room";assert(PrepareMuseRoom(network,grant));assert(MuseRoomPrepared()&&BoundedWebSocket::created==0);assert(ConnectPreparedMuseRoom());assert(!MuseRoomPrepared());assert(BoundedWebSocket::created==1&&!MuseRoomAuthenticated()&&!MuseRoomCleanupComplete());
 auto* ws=BoundedWebSocket::last;assert(ws->id==3&&ws->bound==2048);fixture_task=reinterpret_cast<void*>(2);ws->emit(CHALLENGE,false);assert(ws->sent.size()==1&&!MuseRoomAuthenticated());ws->emit(FRAME,false);assert(MuseRoomAuthenticated()&&ws->sent.back()=="room-ack:1");
 StopMuseRoom();assert(!MuseRoomAuthenticated()&&!MuseRoomCleanupComplete()&&BoundedWebSocket::closed==0);
 fixture_task=reinterpret_cast<void*>(1);PollMuseRoom();assert(MuseRoomCleanupComplete()&&BoundedWebSocket::closed==1);
 assert(!StartMuseRoom(network,grant));assert(MuseRoomCleanupComplete());
 grant.generation="next-generation";assert(StartMuseRoom(network,grant));SelectProvider(P::Mira);PollMuseRoom();assert(MuseRoomCleanupComplete()&&ProviderSelected(P::Mira));
 assert(BoundedWebSocket::closed==2);SelectProvider(P::Muse);grant.generation="network-revoke-generation";assert(PrepareMuseRoom(network,grant));assert(ConnectPreparedMuseRoom());network_allowed=false;PollMuseRoom();assert(MuseRoomCleanupComplete()&&BoundedWebSocket::closed==3);
}'''
for name,data in [('CHALLENGE',challenge),('FRAME',frame)]:code=code.replace(name,json.dumps(data.decode()))
sdk=Path('/Users/mikedemott/Developer/AgentWork/StackChan/.tools/sdk/esp-idf-v5.5.4');mb=sdk/'components/mbedtls/mbedtls';cj=sdk/'components/json/cJSON'
with tempfile.TemporaryDirectory(prefix='muse-transport-owner-') as tmp:
 p=Path(tmp);shutil.copy2(root/'firmware/main/companion/muse_room_transport.cc',p/'muse_room_transport.cc');(p/'test.cpp').write_text(code)
 (p/'local_tls.h').write_text('extern "C" const char* companion_local_ca_pem();\nextern "C" const char* companion_local_tls_host();\n')
 (p/'esp_timer.h').write_text('#pragma once\n#include <cstdint>\nextern uint64_t fixture_ms;inline int64_t esp_timer_get_time(){return fixture_ms*1000;}\n')
 (p/'bounded_web_socket.h').write_text('''#pragma once
#include <functional>
#include <string>
#include <vector>
#include <cstdint>
using TaskHandle_t=void*;extern TaskHandle_t fixture_task;inline TaskHandle_t xTaskGetCurrentTaskHandle(){return fixture_task;}
class NetworkInterface{};
class BoundedWebSocket{public:static inline int created=0,closed=0;static inline BoundedWebSocket* last=nullptr;int id;size_t bound=0;std::vector<std::string> sent;std::function<void(const char*,size_t,bool)> data;
BoundedWebSocket(NetworkInterface*,int value):id(value){++created;last=this;}~BoundedWebSocket(){if(last==this)last=nullptr;}
void SetReceiveBufferSize(size_t n){bound=n;}void OnData(decltype(data) value){data=value;}void OnDisconnected(std::function<void()> value){}void OnError(std::function<void(int)> value){}
bool Connect(const char*){return true;}bool Send(const std::string& value){sent.push_back(value);return true;}void Close(){++closed;}void emit(const std::string& value,bool binary){data(value.data(),value.size(),binary);}};
''')
 (p/'room_config.h').write_text('#define MBEDTLS_MD_C\n#define MBEDTLS_SHA256_C\n#define MBEDTLS_PLATFORM_C\n')
 inc=['-I'+str(p),'-I'+str(root/'firmware/main/companion'),'-I'+str(mb/'include'),'-I'+str(mb/'library'),'-I'+str(cj),'-I'+str(root/'firmware/main')];defines=['-DMBEDTLS_CONFIG_FILE="room_config.h"'];objects=[]
 for n,source in enumerate([cj/'cJSON.c']+[mb/'library'/name for name in ['md.c','sha256.c','platform.c','platform_util.c','constant_time.c']]):
  obj=p/(str(n)+'.o');subprocess.run(['clang','-std=c99','-fsanitize=address,undefined',*inc,*defines,'-c',str(source),'-o',str(obj)],check=True,capture_output=True);objects.append(str(obj))
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',*inc,*defines,str(p/'test.cpp'),*objects,'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Production transport: dormant by default, URI pin, no audio socket, mutually authenticated ready state, callback revocation, retained owner join, no same-generation retry and Mira foreground isolation pass')
