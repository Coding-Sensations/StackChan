#include "muse_codec_fixture.hpp"
#include "voice_device_ui_fixture.hpp"
#include <companion/muse_voice_device_runtime.hpp>
#include <thread>
struct HoldLock:VoiceUiAccess{std::function<void()>on_leave;int depth=0;bool available=true;bool Enter()override{if(!available)return false;assert(!depth);++depth;return true;}void Leave()override{assert(depth==1);--depth;if(on_leave){auto f=on_leave;f();}}};
struct HoldDevice:Device{HoldLock*lock=nullptr;lv_indev_t*Input()const{assert(lock&&lock->depth==1);return input;}};
// Capability/flush are INERT test ports, never native physical evidence.
struct HoldCodec:voice_codec_prep::Codec{
 CoreS3AudioCodec core=Make();CoreVoiceCodec<CoreS3AudioCodec> actual{core};bool supported=true,flush_ok=true,in_read=false;int flushes=0;mutable std::function<void()>support_hook;
 bool SupportsNativePtt()const override{if(support_hook){auto f=std::move(support_hook);support_hook={};f();}return supported;}
 bool Reserve(uint64_t g)override{return actual.Reserve(g);}bool ReserveExact(uint64_t g,const AudioLease&l,Directions d,int64_t e,muse_native_review::Clock*k,Gate f)override{return actual.ReserveExact(g,l,d,e,k,std::move(f));}
 voice_codec_prep::Observation Observe(uint64_t g)override{return actual.Observe(g);}bool EnableMicrophone(uint64_t g)override{return actual.EnableMicrophone(g);}bool EnableOutput(uint64_t g)override{return actual.EnableOutput(g);}
 size_t Read16k(uint64_t g,int16_t*p,size_t n)override{return actual.Read16k(g,p,n);}voice_codec_prep::ReadResult ReadFrame(uint64_t g,int16_t*p,size_t n,size_t&w)override{in_read=true;auto result=actual.ReadFrame(g,p,n,w);in_read=false;return result;}size_t Write16k(uint64_t g,const int16_t*p,size_t n)override{return actual.Write16k(g,p,n);}
 bool StopMicrophone(uint64_t g)override{return actual.StopMicrophone(g);}bool StopOutput(uint64_t g)override{return actual.StopOutput(g);}bool FlushOutput(uint64_t,uint64_t)override{++flushes;return flush_ok;}bool Join(uint64_t g)override{return actual.Join(g);}bool Release(uint64_t g)override{return actual.Release(g);}
};
using HoldRuntime=DeviceVoiceRuntime<Admission,Reader,HoldDevice>;
struct HoldRig{
 AdmissionOwner original;HoldCodec codec;HoldLock lock;HoldDevice device;lv_indev_t pointer;std::unique_ptr<HoldRuntime> runtime;
 HoldRig(bool supported=true){Reset();screen.w=320;screen.h=240;device.input=&pointer;device.lock=&lock;codec.supported=supported;runtime=HoldRuntime::Create(original.protocol,original.admission,original.reader,codec,original.clock,device,&lock);assert(runtime);}
 ~HoldRig(){lock.available=true;assert(runtime->Close()&&!LiveObjects());objects.clear();}
 void Pump(){assert(runtime->Poll());}
 void Live(){assert(runtime->Open(&screen,original.Scope()));original.port->incoming.push_back(original.server.Response(7,200));Pump();original.port->incoming.push_back(original.server.Body(7,session_event));Pump();}
 void Touch(const char*t){assert(lock.Enter());auto*b=Button(t);assert(b);Tap(b,pointer);lock.Leave();Pump();}
 void Approve(unsigned mask=1){Live();unsigned pages=0;while(Button("Next")){Touch("Next");assert(++pages<40);}if(mask&1)Touch("Mic: no");if(mask&2)Touch("Out: no");Touch("Review");if(mask)Touch("Allow");assert(runtime->DirectionsBound()==bool(mask));}
 void DownHold(lv_indev_t*input=nullptr){assert(lock.Enter());auto*b=Button("Hold to talk");assert(b);Down(b,input?*input:pointer);lock.Leave();}
 void UpHold(){assert(lock.Enter());Up(Button("Hold to talk"),pointer);lock.Leave();}
};
int main(){
 {auto core=Make();CoreVoiceCodec<CoreS3AudioCodec> native(core);assert(!native.SupportsNativePtt());}
 {HoldRig r(false);r.Approve();assert(!Button("Hold to talk")&&Text("PTT unavailable: physical flush")&&!reads&&!writes&&!r.codec.flushes);r.Pump();assert(!reads&&!writes);}
 {HoldRig r;r.Approve(2);assert(!Button("Hold to talk")&&Text("Microphone not selected")&&!reads&&!r.codec.flushes);}
 {HoldRig r;r.Approve();assert(Button("Hold to talk"));assert(r.lock.Enter());Click(Button("Hold to talk"));r.lock.Leave();r.Pump();assert(!reads&&!r.codec.flushes);lv_indev_t foreign;r.DownHold(&foreign);r.Pump();assert(!reads&&!r.codec.flushes);}
 {HoldRig r;r.Approve();r.DownHold();r.Pump();assert(r.codec.flushes==1&&reads>0&&!writes);auto before=reads;r.UpHold();r.Pump();assert(reads==before&&!r.runtime->CleanupComplete());r.DownHold();r.Pump();assert(r.codec.flushes==2&&reads>before);r.UpHold();r.Pump();}

 for(unsigned fault=0;fault<5;++fault){HoldRig r;r.Approve();r.DownHold();r.Pump();const auto before=reads;assert(before>0);if(fault==0)r.pointer.state=LV_INDEV_STATE_RELEASED;if(fault==1)r.pointer.point={-1,-1};if(fault==2){assert(r.lock.Enter());auto*b=Button("Hold to talk");lv_event_t lost{b,b->callback_user,LV_EVENT_PRESS_LOST};b->callback(&lost);r.lock.Leave();}lv_indev_t foreign;if(fault==3)r.device.input=&foreign;if(fault==4)assert(r.runtime->ShowReview());r.Pump();assert(reads==before);r.pointer.state=LV_INDEV_STATE_PRESSED;r.pointer.point={250,150};r.device.input=&r.pointer;r.Pump();assert(reads==before);}
 {HoldRig r;r.Approve();r.DownHold();r.lock.available=false;const auto before=reads;assert(!r.runtime->Poll()&&!r.runtime->CleanupComplete()&&LiveObjects()&&reads==before&&!r.original.h.socket_closes);r.lock.available=true;assert(r.runtime->Close()&&r.runtime->CleanupComplete());}
 {HoldRig r;r.Approve();r.DownHold();r.original.h.credential=false;const auto before=reads;r.Pump();assert(r.runtime->CleanupComplete()&&reads==before&&!r.codec.flushes);}
 {HoldRig r;r.Approve();r.DownHold();r.device.foreground=false;const auto before=reads;r.Pump();assert(r.runtime->CleanupComplete()&&reads==before&&!r.codec.flushes);}
 {HoldRig r;r.Approve();r.DownHold();r.original.clock.mono=-1;const auto before=reads;r.Pump();assert(r.runtime->CleanupComplete()&&reads==before);}
 {HoldRig r;r.Approve();r.DownHold();r.codec.flush_ok=false;r.Pump();assert(r.runtime->CleanupComplete()&&r.codec.flushes==1&&!reads&&!writes);}
 {HoldRig r;r.Approve();r.DownHold();read_hook=[&]{std::thread gui([&]{r.UpHold();});gui.join();};r.Pump();assert(r.runtime->CleanupComplete()&&reads==1&&!writes&&r.original.protocol.Pending().kind!=voice_prep::Kind::Append);}
 {HoldRig r;r.Approve();r.DownHold();r.Pump();disable_result=-1;r.UpHold();assert(!r.runtime->Poll()&&!r.runtime->CleanupComplete()&&CurrentAudioOwner()==AudioOwner::Muse&&!r.original.h.socket_closes);disable_result=0;assert(r.runtime->Close()&&r.runtime->CleanupComplete());}

 unsigned clock_fences=0,leave_fences=0;
 {HoldRig r;r.Approve();r.DownHold();r.original.clock.sample_hook=[&](int){if(r.codec.in_read&&!reads&&!r.lock.depth)++clock_fences;};r.lock.on_leave=[&]{if(r.codec.in_read&&!reads)++leave_fences;};r.Pump();r.original.clock.sample_hook={};r.lock.on_leave={};assert(reads>0);}
 assert(clock_fences&&leave_fences);
 for(unsigned fence=clock_fences>48?clock_fences-48:1;fence<=clock_fences;++fence){HoldRig r;r.Approve();r.DownHold();unsigned seen=0;bool released=false;r.original.clock.sample_hook=[&](int){if(r.codec.in_read&&!reads&&!r.lock.depth&&++seen==fence){r.UpHold();released=true;}};r.Pump();r.original.clock.sample_hook={};assert(released&&reads==0&&r.original.protocol.Pending().kind!=voice_prep::Kind::Append);}
 for(unsigned fence=1;fence<=leave_fences;++fence){HoldRig r;r.Approve();r.DownHold();unsigned seen=0;bool released=false;r.lock.on_leave=[&]{if(r.codec.in_read&&!reads&&++seen==fence){r.lock.on_leave={};r.UpHold();released=true;}};r.Pump();r.lock.on_leave={};assert(released&&reads==0&&r.original.protocol.Pending().kind!=voice_prep::Kind::Append);}
 {HoldRig r;r.Approve();r.DownHold();r.codec.support_hook=[&]{r.original.clock.hook=[&]{r.UpHold();r.DownHold();};};r.Pump();assert(!reads&&!r.codec.flushes);r.Pump();assert(r.codec.flushes==1&&reads>0);r.UpHold();r.Pump();}

 std::puts("PASS native held-input UI/owner path with synthetic SDK and inert physical-capability codec; no hardware proof");}
