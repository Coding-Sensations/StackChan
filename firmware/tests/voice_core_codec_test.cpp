#include "muse_codec_fixture.hpp"
#include <companion/muse_voice_core_codec.hpp>
#include <deque>
struct TestClock:muse_native_review::Clock {int64_t now=1000;std::function<void()> during;bool Sample(muse_native_review::Stamp& out)override{auto f=std::move(during);during={};if(f)f();out={now,1760000000000LL};return true;}};
#include "voice_pcm_fixture.hpp"
using Native=companion::muse::CoreVoiceCodec<CoreS3AudioCodec>;
using Op=voice_codec_prep::Operation;
static void Close(Native& native,AudioLease& l){assert(native.StopMicrophone(7));assert(native.StopOutput(7));assert(native.Join(7));assert(native.Release(7));assert(CommonAudioAuthority().Holds(l));assert(ReleaseAudio(l,true));}

struct TestPermit:voice_codec_prep::Permit {
 bool context=true,mic=true,output=true,held=true;mutable int64_t seen=0;int64_t hold_until=10000000;
 bool VoiceContext(uint64_t,int64_t n)const override{seen=n;return context;}
 bool Provider(uint64_t,int64_t)const override{return context;}
 bool Microphone(uint64_t,int64_t)const override{return mic;}
 bool Output(uint64_t,int64_t)const override{return output;}
 bool Hold(uint64_t,uint64_t,int64_t n)const override{return held&&n<hold_until;}
};
static void Live(voice_prep::Protocol& p){constexpr auto call="11111111-2222-4333-8444-555555555555";assert(p.Begin({7,call,"synthetic-device",7,9,10000000},0,1760000000000LL));assert(p.Sent(7,p.Pending().sequence,true,0));assert(p.Head(7,7,200,0));const char* event="{\"type\":\"hatch.voice.session\",\"call_id\":\"11111111-2222-4333-8444-555555555555\",\"session_id\":\"session-exact\",\"thread_id\":\"thread-exact\"}\n";assert(p.Media(7,7,event,std::strlen(event),0));assert(p.Status()==voice_prep::State::Live);}
// One unavailable physical boundary is synthetic here. All resource, driver,
// converter and Owner code remain real; this is NOT hardware flush evidence.
struct SyntheticFlush:voice_codec_prep::Codec {
 Native& codec;Gate captured;explicit SyntheticFlush(Native& c):codec(c){}
 bool Reserve(uint64_t)override{return false;}
 bool ReserveExact(uint64_t g,const AudioLease& l,voice_codec_prep::Directions d,int64_t expiry,muse_native_review::Clock* clock,Gate gate)override{captured=gate;return codec.ReserveExact(g,l,d,expiry,clock,std::move(gate));}
 voice_codec_prep::Observation Observe(uint64_t g)override{return codec.Observe(g);}
 bool EnableMicrophone(uint64_t g)override{return codec.EnableMicrophone(g);}bool EnableOutput(uint64_t g)override{return codec.EnableOutput(g);}
 size_t Read16k(uint64_t g,int16_t* out,size_t n)override{return codec.Read16k(g,out,n);}
 voice_codec_prep::ReadResult ReadFrame(uint64_t g,int16_t* out,size_t n,size_t& read)override{return codec.ReadFrame(g,out,n,read);}
 size_t Write16k(uint64_t g,const int16_t* in,size_t n)override{return codec.Write16k(g,in,n);}
 bool StopMicrophone(uint64_t g)override{return codec.StopMicrophone(g);}bool StopOutput(uint64_t g)override{return codec.StopOutput(g);}
 bool FlushOutput(uint64_t,uint64_t)override{return true;}
 bool Join(uint64_t g)override{return codec.Join(g);}bool Release(uint64_t g)override{return codec.Release(g);}
};
int main(){
 {
  Reset();auto core=Make();Native native(core);TestClock clock;auto l=Take();auto gate=[](uint64_t g,int64_t n,Op){return g==7&&n<10000000;};
  assert(!native.Reserve(7));assert(!native.ReserveExact(7,l,{true,true},10000000,nullptr,gate));assert(!opens&&!reads&&!writes&&!disables);
  assert(native.ReserveExact(7,l,{true,true},10000000,&clock,gate));assert(!opens&&!reads&&!writes&&!disables);assert(!native.Join(7)&&!native.Release(7));assert(native.EnableMicrophone(7));produced={100,300,240};batch=11;int16_t out[320];size_t n=0;
  assert(native.ReadFrame(7,out,320,n)==voice_codec_prep::ReadResult::Pending&&!n&&reads==1);assert(std::all_of(out,out+320,[](auto x){return x==0;}));
  assert(native.ReadFrame(7,out,320,n)==voice_codec_prep::ReadResult::Complete&&n==320&&reads==2);assert(out[0]==11&&out[99]==11&&out[100]==22&&out[319]==22);
  assert(native.ReadFrame(7,out,320,n)==voice_codec_prep::ReadResult::Complete&&n==320&&reads==3);assert(out[0]==22&&out[79]==22&&out[80]==33&&out[319]==33);
  auto observed=native.Observe(7);assert(observed.known&&observed.mic_open&&!observed.capturing&&!observed.playing);assert(native.StopMicrophone(7));assert(native.EnableOutput(7));int16_t audio[1282]={};const int before=writes;assert(native.Write16k(7,audio,1282)==1282&&writes==before+5);assert(!native.FlushOutput(7,1));Close(native,l);
 }
 {
  Reset();auto core=Make();Native native(core);TestClock clock;auto l=Take();bool allowed=true;auto gate=[&](uint64_t,int64_t,Op){return allowed;};assert(native.ReserveExact(7,l,{false,true},10000000,&clock,gate));assert(native.EnableOutput(7));int16_t audio[640]={};dsp_hook=[&]{allowed=false;};assert(!native.Write16k(7,audio,640)&&!writes);assert(CommonAudioAuthority().Holds(l)&&!CommonAudioAuthority().Current(l));Close(native,l);
 }
 {
  Reset();auto core=Make();Native native(core);TestClock clock;auto l=Take();auto gate=[](uint64_t,int64_t,Op){return true;};assert(native.ReserveExact(7,l,{true,false},10000000,&clock,gate));RevokeAudio(l);disable_result=-1;assert(!native.StopMicrophone(7));assert(!native.Release(7));assert(CommonAudioAuthority().Holds(l));disable_result=0;Close(native,l);
 }

 {
  Reset();auto core=Make();Native native(core);TestClock clock;TestPermit permit;voice_prep::Protocol protocol;Live(protocol);voice_codec_prep::Owner owner(protocol,native,permit,clock);
  assert(owner.Prepare(7,10000000,0));assert(owner.Start(7,0)&&owner.Status()==voice_codec_prep::State::Ready);assert(permit.seen==1000&&!opens&&!reads&&!writes);assert(!owner.Talking(7,1,true,0));assert(!opens&&!reads&&!writes&&CurrentAudioOwner()==AudioOwner::None); // unavailable flush fails closed before microphone
 }
 {
  Reset();auto core=Make();Native native(core);TestClock clock;TestPermit permit;voice_prep::Protocol protocol;Live(protocol);voice_codec_prep::Owner owner(protocol,native,permit,clock);assert(owner.Prepare(7,10000000,0));clock.now=10000001;assert(!owner.Start(7,0)&&!opens&&!reads&&!writes&&CurrentAudioOwner()==AudioOwner::None);
 }
 {
  Reset();auto core=Make();Native native(core);SyntheticFlush codec(native);TestClock clock;TestPermit permit;voice_prep::Protocol protocol;Live(protocol);voice_codec_prep::Owner owner(protocol,codec,permit,clock);assert(owner.Prepare(7,10000000,0));assert(owner.Start(7,0));assert(owner.Talking(7,1,true,0));clock.now=201001;produced={100,300};batch=11;
  assert(!owner.Capture(7,0)&&owner.Status()==voice_codec_prep::State::Ready&&reads==1);assert(owner.Capture(7,0)&&owner.Status()==voice_codec_prep::State::Ready&&reads==2);assert(owner.Talking(7,1,false,0));assert(owner.Stop(7,0));assert(CurrentAudioOwner()==AudioOwner::None);
 }
 {
  Reset();auto core=Make();Native native(core);TestClock clock;auto l=Take();bool allow=true;auto gate=[&](uint64_t,int64_t,Op){return allow;};assert(native.ReserveExact(7,l,{true,true},10000000,&clock,gate));assert(native.EnableOutput(7));int16_t audio[640]={};during=[&]{allow=false;};assert(!native.Write16k(7,audio,640)&&writes==1);Close(native,l);
 }
 {
  Reset();auto core=Make();Native native(core);TestClock clock;auto l=Take();auto gate=[](uint64_t,int64_t,Op){return true;};assert(native.ReserveExact(7,l,{true,false},10000000,&clock,gate));assert(native.EnableMicrophone(7));clock.now=10000000;int16_t out[320];size_t n;assert(native.ReadFrame(7,out,320,n)==voice_codec_prep::ReadResult::Failed&&!reads);Close(native,l);
 }

 {
  Reset();auto core=Make();Native native(core);TestClock clock;AudioOwnerAuthority foreign;AudioLease other;assert(foreign.Acquire(AudioOwner::Muse,AudioPurpose::MusePushToTalk,7,other));auto gate=[](uint64_t,int64_t,Op){return true;};assert(!native.ReserveExact(7,other,{true,true},10000000,&clock,gate)&&!opens&&!disables);auto l=Take();assert(native.ReserveExact(7,l,{true,true},10000000,&clock,gate));const int stops=disables;assert(!native.StopMicrophone(8)&&!native.Release(8)&&disables==stops&&CommonAudioAuthority().Current(l));dsp_error=-1;assert(!native.StopMicrophone(7)&&!native.Join(7)&&!native.Release(7));dsp_error=0;Close(native,l);auto successor=Take();assert(!native.ReserveExact(7,successor,{true,true},10000000,&clock,gate)&&CommonAudioAuthority().Current(successor));assert(ReleaseAudio(successor,true));
 }
 {
  Reset();auto core=Make();Native native(core);SyntheticFlush codec(native);TestClock clock;TestPermit permit;voice_prep::Protocol protocol;Live(protocol);voice_codec_prep::Owner owner(protocol,codec,permit,clock);assert(owner.Prepare(7,10000000,0)&&owner.Start(7,0)&&owner.Talking(7,1,true,0));clock.now=201001;produced={320};read_hook=[&]{permit.held=false;};assert(!owner.Capture(7,0)&&reads==1&&protocol.Pending().kind!=voice_prep::Kind::Append);assert(owner.Stop(7,0)&&CurrentAudioOwner()==AudioOwner::None);
 }

 {
  Reset();auto core=Make();Native native(core);TestPermit permit;voice_prep::Protocol protocol;Live(protocol);voice_codec_prep::Owner model_only(protocol,native,permit);assert(model_only.Prepare(7,10000000,0));assert(!model_only.Start(7,0)&&!opens&&!reads&&!writes&&!disables&&CurrentAudioOwner()==AudioOwner::None);
 }

 {
  Reset();auto core=Make();Native native(core);SyntheticFlush codec(native);TestClock clock;TestPermit permit;voice_prep::Protocol protocol;Live(protocol);voice_codec_prep::Owner owner(protocol,codec,permit,clock);assert(owner.Prepare(7,10000000,0)&&owner.Start(7,0)&&owner.Talking(7,1,true,0));permit.hold_until=2000;assert(!codec.captured(7,2001,Op::Input));assert(owner.Stop(7,0));
 }
 {
  Reset();auto core=Make();Native native(core);TestClock clock;TestPermit permit;voice_prep::Protocol protocol;Live(protocol);voice_codec_prep::Owner owner(protocol,native,permit,clock);
  clock.now=10000001;assert(!owner.Prepare(7,10000000,0)&&CurrentAudioOwner()==AudioOwner::None);clock.now=1000;
  bool nested=true;clock.during=[&]{nested=owner.Prepare(8,10000000,0);};
  assert(owner.Prepare(7,10000000,0)&&!nested&&owner.Status()==voice_codec_prep::State::Prepared);
  assert(owner.Stop(7,0)&&CurrentAudioOwner()==AudioOwner::None&&!opens&&!reads&&!writes);
 }
 std::puts("PASS actual Core/DSP resource composition, bounded carry/split IO, fresh gate, retained cleanup; no physical flush/authority claim");
}
