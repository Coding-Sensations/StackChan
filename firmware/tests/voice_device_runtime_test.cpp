#include "muse_codec_fixture.hpp"
#include "voice_device_ui_fixture.hpp"
#include <companion/muse_voice_device_runtime.hpp>
using Runtime=DeviceVoiceRuntime<Admission,Reader,Device>;
struct RuntimeRig {
 AdmissionOwner original;CoreS3AudioCodec core=Make();CoreVoiceCodec<CoreS3AudioCodec> codec{core};lv_indev_t pointer;Device device;std::unique_ptr<Runtime> runtime;
 RuntimeRig(){Reset();screen.w=320;screen.h=240;device.input=&pointer;runtime=Runtime::Create(original.protocol,original.admission,original.reader,codec,original.clock,device);assert(runtime&&!original.admission);}
 ~RuntimeRig(){assert(runtime->Close());}
 void Live(){assert(runtime->Open(&screen,original.Scope()));original.port->incoming.push_back(original.server.Response(7,200));assert(runtime->Poll());original.port->incoming.push_back(original.server.Body(7,session_event));assert(runtime->Poll());assert(Button("Next"));}
 void Review(unsigned mask){while(auto* b=Button("Next"))Tap(b,pointer);if(mask&1)Tap(Button("Mic: no"),pointer);if(mask&2)Tap(Button("Out: no"),pointer);Tap(Button("Review"),pointer);}
};
int main(){
#if !STACKCHAN_MUSE_RAM_ONLY
 {RuntimeRig r;assert(!r.runtime->Open(&screen,r.original.Scope())&&!r.original.h.loads&&!r.original.h.connects);assert(r.runtime->Close());}
 std::puts("PASS nonRAM runtime refuses admission before credential/endpoint access");return 0;
#endif
 {RuntimeRig r;assert(!r.original.h.loads&&!r.original.h.connects&&!r.runtime->Poll()&&!r.runtime->Play());assert(r.runtime->Close()&&r.runtime->CleanupComplete());}
 {RuntimeRig r;r.Live();r.Review(3);Tap(Button("Allow"),r.pointer);assert(r.runtime->Poll()&&r.runtime->DirectionsBound()&&!opens&&!reads&&!writes);Tap(Button("Return"),r.pointer);assert(r.runtime->ShowReview());r.original.h.socket_join=false;assert(!r.runtime->Close()&&!r.runtime->CleanupComplete()&&!LiveObjects()&&CurrentAudioOwner()==AudioOwner::None);r.original.h.socket_join=true;assert(r.runtime->Close()&&r.runtime->CleanupComplete());}
 for(bool ending:{false,true})for(unsigned phase=0;phase<4;++phase){
  RuntimeRig r;auto close=[&]{assert(!(ending?r.runtime->End():r.runtime->Close())&&!r.runtime->Poll()&&!r.runtime->Play());};
  if(phase==0)r.original.h.load_hook=close;if(phase==1)r.original.h.find_hook=close;if(phase==2)r.original.h.connect_hook=close;if(phase==3)r.original.h.send_hook=close;
  assert(!r.runtime->Open(&screen,r.original.Scope())&&r.runtime->CloseRequested());
  if(phase==0)assert(!r.original.h.finds);if(phase<2)assert(!r.original.h.connects);if(phase<3)assert(!r.original.h.sends);else assert(r.original.h.sends==1);
  assert(r.runtime->Close()&&r.runtime->CleanupComplete());
 }
 {
  Reset();AdmissionOwner original;auto core=Make();CoreVoiceCodec<CoreS3AudioCodec> codec(core);Device device;Clock wrong_clock;Harness wrong_h;Reader wrong_reader(wrong_h);Protocol wrong_protocol;auto* same=original.admission.get();
  assert(!Runtime::Create(original.protocol,original.admission,wrong_reader,codec,original.clock,device)&&original.admission.get()==same);
  assert(!Runtime::Create(original.protocol,original.admission,original.reader,codec,wrong_clock,device)&&original.admission.get()==same);
  assert(!Runtime::Create(wrong_protocol,original.admission,original.reader,codec,original.clock,device)&&original.admission.get()==same&&!original.h.loads);
 }
 {
  RuntimeRig r;assert(!r.runtime->Open(nullptr,r.original.Scope())&&!r.original.h.loads);auto scope=r.original.Scope();scope.call="invalid";assert(!r.runtime->Open(&screen,scope)&&!r.original.h.loads&&!r.runtime->Open(&screen,r.original.Scope()));assert(r.runtime->Close());
 }
 for(unsigned mask=0;mask<4;++mask){
  RuntimeRig r;r.Live();assert(!r.runtime->Play());r.Review(mask);if(mask)Tap(Button("Allow"),r.pointer);assert(r.runtime->Poll()&&r.runtime->DirectionsBound()==bool(mask));
  const int sent=r.original.h.sends;assert(!r.runtime->Play()&&!r.runtime->Play()&&r.original.h.sends==sent&&!opens&&!reads&&!writes);assert(r.runtime->DirectionsBound()==bool(mask));
 }
 {
  RuntimeRig r;r.Live();r.Review(3);Tap(Button("Allow"),r.pointer);assert(r.runtime->Poll());disable_result=-1;const int sends=r.original.h.sends;
  assert(!r.runtime->Close()&&!r.runtime->CleanupComplete()&&!r.original.h.socket_closes&&CurrentAudioOwner()==AudioOwner::Muse&&!r.runtime->Play());assert(!r.runtime->Poll()&&r.original.h.sends==sends);disable_result=0;assert(r.runtime->Close()&&CurrentAudioOwner()==AudioOwner::None);
 }
 {
  RuntimeRig r;r.Live();r.Review(3);Tap(Button("Allow"),r.pointer);assert(r.runtime->Poll());const int sends=r.original.h.sends;Tap(Button("End session"),r.pointer);assert(r.original.h.sends==sends&&!r.runtime->EndConfirmed());assert(r.runtime->Poll()&&r.runtime->EndRequested()&&!r.runtime->DirectionsBound()&&!r.runtime->EndConfirmed());
  r.original.port->incoming.push_back(r.original.server.Body(7,"{\"type\":\"response.done\"}\n",true));assert(r.runtime->Poll()&&!r.runtime->EndConfirmed());r.original.port->incoming.push_back(r.original.server.Response(9,200,"{\"ok\":true}",true));assert(r.runtime->Poll()&&r.runtime->CleanupComplete()&&r.runtime->EndConfirmed()&&!LiveObjects());
 }
 {
  RuntimeRig r;r.Live();r.original.port->receive_hook=[&]{assert(!r.runtime->Close());};assert(r.runtime->Poll()&&r.runtime->CleanupComplete()&&!LiveObjects());
 }
 {
  RuntimeRig r;r.Live();r.original.clock.hook=[&]{assert(!r.runtime->Close()&&!r.original.h.socket_closes&&LiveObjects());};Tap(Button("Next"),r.pointer);assert(r.runtime->CloseRequested()&&LiveObjects()&&!r.original.h.socket_closes);assert(r.runtime->Poll()&&r.runtime->CleanupComplete()&&!LiveObjects());
 }
 for(bool ending:{false,true})for(int step=1;step<=16;++step){
  RuntimeRig r;r.Live();r.Review(3);Tap(Button("Allow"),r.pointer);const int start=r.original.clock.samples;bool called=false;
  r.original.clock.sample_hook=[&](int sample){if(sample==start+step){called=true;assert(!(ending?r.runtime->End():r.runtime->Close()));assert(!r.runtime->Poll()&&!r.runtime->Play());}};
  (void)r.runtime->Poll();assert(called&&!r.runtime->DirectionsBound()&&!opens&&!reads&&!writes);r.original.clock.sample_hook={};assert(r.runtime->Close()&&r.runtime->CleanupComplete());
 }
 for(unsigned failure=0;failure<4;++failure){
  RuntimeRig r;r.Live();r.Review(3);Tap(Button("Allow"),r.pointer);assert(r.runtime->Poll());if(failure==0)r.original.h.credential=false;if(failure==1)r.original.h.network=false;if(failure==2)r.device.foreground=false;if(failure==3)r.original.clock.mono=10000000;
  (void)r.runtime->Poll();assert(r.runtime->CloseRequested()&&r.runtime->Close()&&!r.runtime->EndConfirmed()&&!LiveObjects()&&CurrentAudioOwner()==AudioOwner::None);
 }
 for(unsigned failure=0;failure<2;++failure){RuntimeRig r;if(failure==0)r.device.foreground=false;else r.device.node="another-board";assert(!r.runtime->Open(&screen,r.original.Scope())&&!r.original.h.loads&&!r.original.h.connects);assert(r.runtime->Close());}
 std::puts("PASS actual SDK/consent/UI/runtime with synthetic ports only");
}
