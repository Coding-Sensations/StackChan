"""Production device handshake/lease/foreground peer; no SDK or network."""
from pathlib import Path
import json,subprocess,sys,tempfile
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'companion/host/streaming'))
from provider_room_bridge import encode_wire,decode_wire,ROOM
from muse_room_transport import OPEN
E=b'event-fixture-key-not-a-credential!';D=b'decision-fixture-key-not-secret!!'
hello=dict(version=1,provider='muse',binding='binding',generation='peer-generation',kind='challenge',nonce='a'*32)
challenge=encode_wire(E,hello,OPEN);bad=encode_wire(D,hello,OPEN)
state=dict(version=1,provider='muse',binding='binding',generation='peer-generation',sequence=1,scene='waiting_receipt',summary='',request_id='',operation_sha256='',presentation_id='',ttl_ms=0,caption='A real peer fixture caption.')
frame=encode_wire(E,state,ROOM)
code=r'''#include "companion/muse_room_peer.hpp"
#include "companion/provider_room_runtime.cc"
#include <cassert>
#include <fstream>
int main(int argc,char**argv){using namespace companion::room;using P=companion_modes::Provider;
 const unsigned char e[]="event-fixture-key-not-a-credential!",d[]="decision-fixture-key-not-secret!!";
 MuseRoomGrant grant;grant.binding="binding";grant.generation="peer-generation";grant.event_size=sizeof(e)-1;grant.control_size=sizeof(d)-1;
 std::memcpy(grant.event.data(),e,grant.event_size);std::memcpy(grant.control.data(),d,grant.control_size);grant.expires_ms=10000;
 assert(MuseRoomUri("wss://192.168.20.40:18444/companion/muse-room","192.168.20.40"));
 for(const std::string uri:{"ws://192.168.20.40/companion/muse-room","wss://192.168.20.40/companion/audio","wss://192.168.20.41/companion/muse-room","wss://user@192.168.20.40/companion/muse-room","wss://192.168.20.40:0/companion/muse-room","wss://192.168.20.40:65536/companion/muse-room"})assert(!MuseRoomUri(uri,"192.168.20.40"));
 SelectProvider(P::Mira);MuseRoomPeer peer;assert(!peer.Open(grant,1000));SelectProvider(P::Muse);assert(peer.Open(grant,1000));assert(!peer.Authenticated());
 std::string reply;assert(peer.Receive(CHALLENGE,false,1001,reply));assert(!peer.Authenticated());std::ofstream(argv[1])<<reply;
 assert(peer.Receive(FRAME,false,1002,reply));assert(peer.Authenticated());assert(reply=="room-ack:1");assert(ViewProvider(P::Muse,1002).caption=="A real peer fixture caption.");
 assert(!peer.Receive(FRAME,false,1003,reply));assert(!peer.Authenticated()&&!ViewProvider(P::Muse,1003).verified);assert(!peer.Open(grant,1004));
 grant.generation="second-generation";assert(peer.Open(grant,2000));assert(!peer.Receive(BAD,false,2001,reply));
 grant.generation="third-generation";assert(peer.Open(grant,3000));assert(!peer.Receive("SCAV audio fixture",true,3001,reply));
 grant.generation="fourth-generation";assert(peer.Open(grant,4000));assert(!peer.Tick(6000));assert(!ViewProvider(P::Muse,6000).verified);
 grant.generation="fifth-generation";assert(peer.Open(grant,7000));SelectProvider(P::Mira);assert(!peer.Tick(7001));assert(ProviderSelected(P::Mira));
 SelectProvider(P::Muse);grant.generation="sixth-generation";assert(peer.Open(grant,8000));assert(!peer.Tick(7999));
 grant.generation="seventh-generation";grant.expires_ms=9000;assert(peer.Open(grant,8500));assert(!peer.Tick(9000));
}'''
for name,value in [('CHALLENGE',challenge),('FRAME',frame),('BAD',bad)]:code=code.replace(name,json.dumps(value.decode()))
sdk=Path('/Users/mikedemott/Developer/AgentWork/StackChan/.tools/sdk/esp-idf-v5.5.4');mb=sdk/'components/mbedtls/mbedtls';cj=sdk/'components/json/cJSON'
with tempfile.TemporaryDirectory(prefix='muse-peer-') as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(code);(p/'room_config.h').write_text('#define MBEDTLS_MD_C\n#define MBEDTLS_SHA256_C\n#define MBEDTLS_PLATFORM_C\n')
 inc=['-I'+str(p),'-I'+str(mb/'include'),'-I'+str(mb/'library'),'-I'+str(cj),'-I'+str(root/'firmware/main')];defines=['-DMBEDTLS_CONFIG_FILE="room_config.h"'];objects=[]
 for n,source in enumerate([cj/'cJSON.c']+[mb/'library'/name for name in ['md.c','sha256.c','platform.c','platform_util.c','constant_time.c']]):
  obj=p/(str(n)+'.o');subprocess.run(['clang','-std=c99','-fsanitize=address,undefined',*inc,*defines,'-c',str(source),'-o',str(obj)],check=True,capture_output=True);objects.append(str(obj))
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',*inc,*defines,str(p/'test.cpp'),*objects,'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test'),str(p/'proof')],check=True)
 assert decode_wire(D,(p/'proof').read_bytes(),OPEN)==dict(hello,kind='ready')
print('Actual device peer challenge proof cross-checked with host: TLS URI pins, fresh generation, mutual proof, replay, binary rejection, heartbeat, lease, clock reversal and Muse/Mira foreground isolation pass')
