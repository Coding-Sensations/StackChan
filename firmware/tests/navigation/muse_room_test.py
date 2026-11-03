"""Actual Muse host projection -> production C++ room codec with sanitizers."""
from pathlib import Path
import asyncio,json,os,subprocess,sys,tempfile
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'companion/host/streaming'))
from muse_text_adapter import MuseTextAdapter,MuseTextPort,deny_device_command
from provider_room_bridge import decode_wire,encode_wire,ROOM
from types import SimpleNamespace
E=b'event-fixture-key-not-a-credential!';D=b'decision-fixture-key-not-secret!!'
class Link:
 def __init__(self):
  self._device=SimpleNamespace(commands={});self._run_command=deny_device_command
  self.registered=asyncio.Event();self.registered.set();self.chat_subscribed=asyncio.Event();self.chat_subscribed.set()
 async def open_side_chat(self,side):return SimpleNamespace(exists=True)
 async def send_chat(self,*args,**kw):return {'ok':True,'status':200,'response':{'message_id':'user-1'}}
async def frames():
 a=MuseTextAdapter(MuseTextPort(Link(),'existing-side'),'binding','generation',E,D,authorize=lambda:True,publish_captions=True,clock=lambda:10.)
 await a.submit_text('local-request','Fixture text submission');work=a.snapshot()
 assert a.observe({'type':'event','seq':1,'event':'delta.message_done','payload':{'message_id':'assistant-1','reply_to_message_id':'user-1','display_text':'A bounded fixture reply from Muse.'}})
 caption=a.snapshot();await a.close();closed=a.snapshot();return work,caption,closed
work,caption,closed=asyncio.run(frames());frame=decode_wire(E,caption,ROOM)
variants={
 'GOOD':caption,'WORK':work,'CLOSED':closed,
 'FOREIGN':encode_wire(E,dict(frame,provider='mira'),ROOM),
 'LONG':encode_wire(E,dict(frame,caption='x'*161),ROOM),
 'CONTROL':encode_wire(E,dict(frame,caption='unsafe\nline'),ROOM),
 'WRONG_TYPE':encode_wire(E,dict(frame,caption=True),ROOM),
 'UNKNOWN':encode_wire(E,dict(frame,scene='unknown'),ROOM),
 'MISSING':encode_wire(E,{k:v for k,v in frame.items() if k!='scene'},ROOM),
 'DECISION':encode_wire(E,dict(frame,scene='decision_sign',summary='Fixture',request_id='r',presentation_id='p',operation_sha256='a'*64,ttl_ms=10),ROOM),
}
fixture=r'''#include "companion/provider_room_wire.hpp"
#include "companion/provider_room_runtime.cc"
#include <cassert>
int main(){using namespace companion::room;
 const unsigned char e[]="event-fixture-key-not-a-credential!",d[]="decision-fixture-key-not-secret!!";
 Wire muse("muse");assert(muse.Bind("binding","generation",e,sizeof(e)-1,d,sizeof(d)-1,false));
 assert(muse.Accept(WORK,1000));assert(muse.Current(1000).caption.empty());
 for(const auto& bad:{FOREIGN,LONG,CONTROL,WRONG_TYPE,UNKNOWN,MISSING,DECISION})assert(!muse.Accept(bad,1001));
 assert(muse.Accept(GOOD,1002));auto state=muse.Current(1002);assert(state.verified&&state.caption=="A bounded fixture reply from Muse."&&state.scene=="waiting_receipt");
 assert(muse.Decision("approve","p",1003).empty());assert(!muse.Accept(GOOD,1003));
 assert(muse.Accept(CLOSED,1004));assert(muse.Current(1004).caption.empty()&&muse.Current(1004).scene=="unknown");
 Wire mira("mira");assert(mira.Bind("binding","generation",e,sizeof(e)-1,d,sizeof(d)-1,false));assert(!mira.Accept(FOREIGN,1002));
 muse.Disconnect();assert(muse.Current(1005).caption.empty()&&!muse.Current(1005).verified);
 using P=companion_modes::Provider;
 SelectProvider(P::Muse);assert(BindProvider(P::Muse,"binding","generation",e,sizeof(e)-1,d,sizeof(d)-1,false));
 assert(AcceptProvider(P::Muse,GOOD,2000));assert(ViewProvider(P::Muse,2000).caption=="A bounded fixture reply from Muse.");
 assert(!QueueDecision(P::Muse,"approve","p",2000));
 SelectProvider(P::Mira);assert(!ViewProvider(P::Muse,2001).verified);assert(!AcceptProvider(P::Muse,GOOD,2001));
 assert(BindProvider(P::Mira,"binding","mira-generation",e,sizeof(e)-1,d,sizeof(d)-1,false));
 assert(!ViewProvider(P::Mira,2002).verified);assert(!ViewProvider(P::Muse,2002).verified);
 SelectProvider(P::Muse);assert(!BindProvider(P::Muse,"binding","generation",e,sizeof(e)-1,d,sizeof(d)-1,false));
 DisconnectProvider(P::Muse);

}'''
for name,value in sorted(variants.items(),key=lambda item:-len(item[0])):fixture=fixture.replace(name,json.dumps(value.decode()))
sdk=Path('/Users/mikedemott/Developer/AgentWork/StackChan/.tools/sdk/esp-idf-v5.5.4');mb=sdk/'components/mbedtls/mbedtls';cj=sdk/'components/json/cJSON'
with tempfile.TemporaryDirectory(prefix='muse-room-wire-') as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(fixture);(p/'room_config.h').write_text('#define MBEDTLS_MD_C\n#define MBEDTLS_SHA256_C\n#define MBEDTLS_PLATFORM_C\n')
 inc=['-I'+str(p),'-I'+str(mb/'include'),'-I'+str(mb/'library'),'-I'+str(cj),'-I'+str(root/'firmware/main')];defines=['-DMBEDTLS_CONFIG_FILE="room_config.h"'];objects=[]
 for n,source in enumerate([cj/'cJSON.c']+[mb/'library'/name for name in ['md.c','sha256.c','platform.c','platform_util.c','constant_time.c']]):
  obj=p/(str(n)+'.o');subprocess.run(['clang','-std=c99','-fsanitize=address,undefined',*inc,*defines,'-c',str(source),'-o',str(obj)],check=True,capture_output=True);objects.append(str(obj))
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',*inc,*defines,str(p/'test.cpp'),*objects,'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Muse adapter -> real HMAC/cJSON codec: caption, no approval, scope, bounds, replay, revocation and disconnect pass')
