#include "muse_codec_fixture.hpp"
#include "voice_device_ui_fixture.hpp"
#include <companion/muse_voice_device_runtime.hpp>
#include <companion/muse_voice_task_owner.hpp>
struct Parked{};
struct Kernel{using Handle=int;void(*entry)(void*)=nullptr;void*context=nullptr;int state=0,deleted=0,delays=0;bool create=true,partial=false;std::function<void()> before_park;
 bool Create(void(*fn)(void*),void*ctx,unsigned stack,Handle&h){assert(stack>=8192);if(!create&&!partial)return false;entry=fn;context=ctx;h=1;state=1;return create;}
 void Delay(unsigned ms){assert(ms==5||ms==20);++delays;}void SuspendSelf(){if(before_park)before_park();state=2;throw Parked{};}
 bool Suspended(Handle h){assert(h==1);return state==2;}void Delete(Handle h){assert(h==1&&state==2);++deleted;state=3;}
 void Run(){try{entry(context);}catch(const Parked&){} }
};
struct UiAccess:VoiceUiAccess{bool Enter()override{return true;}void Leave()override{}};
struct Work{AdmissionOwner&original;CoreS3AudioCodec core=Make();CoreVoiceCodec<CoreS3AudioCodec> codec{core};Device device;lv_indev_t pointer;UiAccess access;std::unique_ptr<DeviceVoiceRuntime<Admission,Reader,Device>> runtime;std::function<bool()> current;int phase=0,closes=0;bool&destroyed;
 Work(AdmissionOwner&o,std::function<bool()> c,bool&d):original(o),current(std::move(c)),destroyed(d){device.input=&pointer;runtime=DeviceVoiceRuntime<Admission,Reader,Device>::Create(original.protocol,original.admission,original.reader,codec,original.clock,device,&access);assert(runtime);}
 ~Work(){assert(runtime->CleanupComplete()&&!LiveObjects()&&CurrentAudioOwner()==AudioOwner::None);destroyed=true;}
 bool Begin(){auto scope=original.Scope();auto grant=scope.voice_current;scope.voice_current=[this,grant]{return current()&&grant();};return runtime->Open(&screen,scope);}
 bool Poll(){if(phase==0)original.port->incoming.push_back(original.server.Response(7,200));else if(phase==1)original.port->incoming.push_back(original.server.Body(7,session_event));else if(auto*b=Button("Next"))Tap(b,pointer);else if(auto*b=Button("Mic: no"))Tap(b,pointer);else if(auto*b=Button("Review"))Tap(b,pointer);else if(auto*b=Button("Allow"))Tap(b,pointer);++phase;bool valid=runtime->Poll();return valid&&!runtime->DirectionsBound();}
 void SignalClose(){runtime->SignalClose();}
 bool Close(){++closes;if(closes<3)disable_result=-1;else disable_result=0;return runtime->Close();}
};
int main(){Reset();screen.w=320;screen.h=240;Kernel kernel;AdmissionOwner original;bool destroyed=false;RetainedVoiceTask<Work,Kernel> task(kernel,[&](auto current){return std::make_unique<Work>(original,current,destroyed);});assert(task.Start(12288)&&!task.Done()&&!task.TryRetire()&&!original.h.loads);kernel.before_park=[&]{assert(destroyed&&task.Done()&&!task.TryRetire()&&!LiveObjects());};kernel.Run();assert(task.Done()&&destroyed&&!kernel.deleted);assert(task.TryRetire()&&task.CleanupComplete()&&kernel.deleted==1&&!task.Start(12288));
 {Reset();Kernel failed;failed.create=false;AdmissionOwner unused;bool gone=false;unsigned factories=0;RetainedVoiceTask<Work,Kernel> rejected(failed,[&](auto current){++factories;return std::make_unique<Work>(unused,current,gone);});assert(!rejected.Start(12288)&&rejected.Done()&&rejected.TryRetire()&&rejected.CleanupComplete()&&!factories&&!unused.h.loads&&!failed.deleted&&!rejected.Start(12288));}
 {Reset();Kernel stopped;AdmissionOwner unused;bool gone=false;unsigned factories=0;RetainedVoiceTask<Work,Kernel> cancelled(stopped,[&](auto current){++factories;return std::make_unique<Work>(unused,current,gone);});assert(cancelled.Start(12288));cancelled.SignalStop();stopped.Run();assert(cancelled.Done()&&!factories&&!unused.h.loads&&cancelled.TryRetire());}
 {Reset();Kernel partial;partial.create=false;partial.partial=true;AdmissionOwner retained;bool gone=false;RetainedVoiceTask<Work,Kernel> ambiguous(partial,[&](auto current){return std::make_unique<Work>(retained,current,gone);});assert(!ambiguous.Start(12288)&&!ambiguous.Done()&&!ambiguous.TryRetire()&&!gone&&!partial.deleted);partial.Run();assert(gone&&ambiguous.TryRetire()&&partial.deleted==1);}
 {Reset();Kernel closing;AdmissionOwner retained;bool gone=false;RetainedVoiceTask<Work,Kernel> cancelled(closing,[&](auto current){return std::make_unique<Work>(retained,current,gone);});retained.h.load_hook=[&]{cancelled.SignalStop();};assert(cancelled.Start(12288));closing.Run();assert(gone&&cancelled.Done()&&!retained.h.finds&&!retained.h.connects&&cancelled.TryRetire());}
 std::puts("PASS actual SDK/runtime retained task with synthetic kernel; no native task/IO acceptance");}
