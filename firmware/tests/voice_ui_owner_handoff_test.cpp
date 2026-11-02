#include "muse_codec_fixture.hpp"
#include "voice_device_ui_fixture.hpp"
#include <companion/muse_voice_device_runtime.hpp>
#include <thread>
static bool native_lock_ok=false,native_lock_held=false;static unsigned native_lock_calls=0,native_unlock_calls=0,native_unchecked_calls=0,native_timeout=0;
bool lvgl_port_lock(uint32_t timeout){++native_lock_calls;native_timeout=timeout;native_lock_held=native_lock_ok;return native_lock_ok;}
void lvgl_port_unlock(){assert(native_lock_held);++native_unlock_calls;native_lock_held=false;}
struct UncheckedHal {lv_indev_t touch;lv_indev_t* lvTouchpad=&touch;void lvglLock(){++native_unchecked_calls;}void lvglUnlock(){++native_unchecked_calls;}};
UncheckedHal& GetHAL(){static UncheckedHal hal;return hal;}
NATIVE_UI_ACCESS_BODY
struct UiAccess:VoiceUiAccess {int depth=0,entries=0;bool available=true;bool Enter()override{if(!available)return false;assert(!depth);++depth;++entries;return true;}void Leave()override{assert(depth==1);--depth;}};
struct HandoffDevice:Device{UiAccess* access=nullptr;lv_indev_t* Input()const{assert(access&&access->depth==1);return input;}};
using Runtime=DeviceVoiceRuntime<Admission,Reader,HandoffDevice>;
struct HandoffRig{
 AdmissionOwner original;CoreS3AudioCodec core=Make();CoreVoiceCodec<CoreS3AudioCodec> codec{core};UiAccess access;HandoffDevice device;lv_indev_t pointer;std::unique_ptr<Runtime> runtime;
 HandoffRig(){Reset();native_tick=0;screen.w=320;screen.h=240;device.input=&pointer;device.access=&access;runtime=Runtime::Create(original.protocol,original.admission,original.reader,codec,original.clock,device,&access);assert(runtime&&!original.h.loads&&!access.entries);auto unlocked=[&]{assert(!access.depth);};original.h.load_hook=original.h.find_hook=original.h.connect_hook=original.h.send_hook=unlocked;}
 ~HandoffRig(){access.available=true;assert(runtime->Close()&&!access.depth&&!LiveObjects());}
 void Live(){assert(runtime->Open(&screen,original.Scope()));original.port->receive_hook=[&]{assert(!access.depth);};original.port->incoming.push_back(original.server.Response(7,200));assert(runtime->Poll());original.port->incoming.push_back(original.server.Body(7,session_event));assert(runtime->Poll());}
 void Touch(const char* text,lv_indev_t* input=nullptr){assert(access.Enter());auto*button=Button(text);assert(button);const int samples=original.clock.samples,sends=original.h.sends,receives=original.h.reads;Tap(button,input?*input:pointer);assert(original.clock.samples==samples&&original.h.sends==sends&&original.h.reads==receives);access.Leave();}
 void TouchAndPump(const char*text){Touch(text);assert(runtime->Poll());}
 void Review(unsigned mask){unsigned pages=0;while(Button("Next")){TouchAndPump("Next");assert(++pages<40);}if(mask&1)TouchAndPump("Mic: no");if(mask&2)TouchAndPump("Out: no");TouchAndPump("Review");}
 void Approve(){Touch("Allow");assert(!runtime->DirectionsBound());assert(runtime->Poll()&&runtime->DirectionsBound()&&!opens&&!reads&&!writes);}
};
int main(){
 {M5VoiceUiAccess access;auto* original_input=GetHAL().lvTouchpad;GetHAL().lvTouchpad=nullptr;assert(!access.Enter()&&!native_lock_calls);GetHAL().lvTouchpad=original_input;assert(!access.Enter()&&native_lock_calls==1&&!native_unlock_calls&&!native_unchecked_calls);native_lock_ok=true;assert(access.Enter()&&native_lock_held&&native_timeout==20);access.Leave();assert(!native_lock_held&&native_unlock_calls==1&&!native_unchecked_calls);}
 {
  Reset();screen.w=320;screen.h=240;AdmissionOwner original;auto core=Make();CoreVoiceCodec<CoreS3AudioCodec> codec(core);Device device;lv_indev_t input;device.input=&input;M5VoiceUiAccess access;
  auto runtime=DeviceVoiceRuntime<Admission,Reader,Device>::Create(original.protocol,original.admission,original.reader,codec,original.clock,device,&access);
  native_lock_ok=true;assert(runtime->Open(&screen,original.Scope()));original.port->incoming.push_back(original.server.Response(7,200));assert(runtime->Poll());original.port->incoming.push_back(original.server.Body(7,session_event));assert(runtime->Poll()&&LiveObjects());
  native_lock_ok=false;const auto unlocks=native_unlock_calls;assert(!runtime->Poll()&&!runtime->CleanupComplete()&&LiveObjects()&&!original.h.socket_closes&&native_unlock_calls==unlocks&&!native_lock_held);
  native_lock_ok=true;assert(runtime->Close()&&runtime->CleanupComplete()&&!LiveObjects()&&!native_lock_held&&!native_unchecked_calls);
 }
 for(unsigned mask=0;mask<4;++mask){HandoffRig r;r.Live();r.Review(mask);if(mask)r.Approve();assert(r.runtime->DirectionsBound()==bool(mask));assert(!r.runtime->Play()&&!r.access.depth&&!opens&&!reads&&!writes);}
 for(unsigned fault=0;fault<5;++fault){
  HandoffRig r;r.Live();r.Review(3);lv_indev_t foreign;
  if(fault==0)r.Touch("Allow",&foreign);
  if(fault==1){assert(r.access.Enter());Click(Button("Allow"));r.access.Leave();}
  if(fault==2){r.Touch("Allow");assert(r.runtime->ShowReview());}
  if(fault==3){r.Touch("Allow");native_tick+=10001;}
  if(fault==4){r.Touch("Allow");r.device.input=&foreign;}
  assert(r.runtime->Poll()&&!r.runtime->DirectionsBound()&&!opens&&!reads&&!writes);
 }
 {
  HandoffRig r;r.Live();for(unsigned i=0;i<5;++i)r.Touch("Next");const int before=r.original.h.reads;assert(r.runtime->Poll()&&r.runtime->CleanupComplete()&&r.original.h.reads==before&&!opens&&!reads&&!writes);
 }
 {
  HandoffRig r;r.Live();r.Review(3);r.Touch("Allow");const int before=r.original.h.reads;r.original.h.credential=false;assert(r.runtime->Poll()&&r.runtime->CleanupComplete()&&r.original.h.reads==before&&!opens&&!reads&&!writes);
 }
 {
  HandoffRig r;r.Live();r.Review(3);r.Approve();r.Touch("Return");assert(r.runtime->Poll()&&r.runtime->ShowReview());const int sends=r.original.h.sends;r.Touch("End session");assert(r.original.h.sends==sends&&!r.runtime->EndConfirmed());assert(r.runtime->Poll()&&r.runtime->EndRequested()&&!r.runtime->DirectionsBound());r.original.port->incoming.push_back(r.original.server.Body(7,"{\"type\":\"response.done\"}\n",true));assert(r.runtime->Poll());r.original.port->incoming.push_back(r.original.server.Response(9,200,"{\"ok\":true}",true));assert(r.runtime->Poll()&&r.runtime->CleanupComplete()&&r.runtime->EndConfirmed());
 }
 {
  HandoffRig r;r.Live();r.Review(3);r.Approve();r.Touch("Close");assert(!r.runtime->Play()&&!opens&&!reads&&!writes);assert(r.runtime->Close()&&r.runtime->CleanupComplete());
 }
 {
  HandoffRig r;r.Live();r.access.available=false;const int sent=r.original.h.sends;assert(!r.runtime->Poll()&&!r.runtime->CleanupComplete()&&!r.original.h.socket_closes&&LiveObjects()&&r.original.h.sends==sent);r.access.available=true;assert(r.runtime->Close()&&r.runtime->CleanupComplete());
 }
 {
  HandoffRig r;r.Live();r.original.port->receive_hook=[&]{assert(!r.access.depth);std::thread gui([&]{r.Touch("Close");});gui.join();};assert(r.runtime->Poll()&&r.runtime->CleanupComplete()&&!opens&&!reads&&!writes&&!r.runtime->EndConfirmed());
 }
 {
  HandoffRig r;r.Live();r.Review(3);r.Approve();disable_result=-1;r.Touch("Close");assert(!r.runtime->Poll()&&!r.runtime->CleanupComplete()&&CurrentAudioOwner()==AudioOwner::Muse&&!r.original.h.socket_closes);disable_result=0;assert(r.runtime->Close()&&r.runtime->CleanupComplete());
 }
 {
  HandoffRig r;r.Live();r.Review(3);assert(r.access.Enter());Down(Button("Allow"),r.pointer);native_tick+=5000;Up(Button("Allow"),r.pointer);r.access.Leave();native_tick+=6000;assert(r.runtime->Poll()&&!r.runtime->DirectionsBound());
 }
 {
  HandoffRig r;r.Live();r.Review(3);assert(r.access.Enter());native_tick=UINT32_MAX-5;Down(Button("Allow"),r.pointer);native_tick=4;Up(Button("Allow"),r.pointer);r.access.Leave();assert(r.runtime->Poll()&&r.runtime->DirectionsBound());
 }
 {
  HandoffRig r;r.Live();r.original.port->receive_hook=[&]{std::thread stop([&]{r.runtime->SignalClose();});stop.join();};assert(r.runtime->Poll()&&r.runtime->CleanupComplete()&&!r.runtime->EndConfirmed()&&!opens&&!reads&&!writes);
 }
 std::puts("PASS actual SDK/runtime/queued UI with synthetic ports; GUI callbacks contain no owner/SDK calls");
}
