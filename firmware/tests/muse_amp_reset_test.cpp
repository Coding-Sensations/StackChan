#include "muse_codec_fixture.hpp"
#include <audio_codec_if.h>
#include <cstdlib>
#include "muse_amplifier_fixture.hpp"
using Reset_=Codec::MuseReset;using RPhase=Codec::MuseResetPhase;
// Fake expander: models a P0 latch with unrelated bits, so a blind write would be visible.
struct Port:Codec::MuseAmpResetPort{
 uint8_t latch=0b10100111;int assert_status=0,release_status=0,assert_calls=0,release_calls=0;std::vector<uint32_t> delays;
 std::function<void(bool)> hook;
 // Snapshots of progress at the instant the line moves: stop/discard/mute must already be complete.
 int in_off_at_assert=-1,out_off_at_assert=-1,preload_at_assert=-1,mute_at_assert=-1,writes_at_assert=-1,amp_calls_at_release=-1;
 int SetResetLine(bool released)override{
  if(hook)hook(released);
  if(!released){++assert_calls;in_off_at_assert=input_disables;out_off_at_assert=output_disables;preload_at_assert=preload_calls;mute_at_assert=mute_reg;writes_at_assert=amp_writes;if(assert_status)return assert_status;latch&=uint8_t(~0x04);return 0;}
  ++release_calls;amp_calls_at_release=amp_calls;if(release_status)return release_status;latch|=0x04;return 0;
 }
 std::function<void(uint32_t)> delay_hook;
 void DelayMs(uint32_t ms)override{delays.push_back(ms);if(delay_hook)delay_hook(ms);}
};
static void Dma(){dma_info={};std::fill(dma_bytes.begin(),dma_bytes.end(),0x6d);dma_cursor=largest_preload=0;dma_capacity=5760;info_calls=preload_calls=info_error=error_preload=partial_preload=0;info_hook={};preload_hook={};}
static int Io(){return opens+reads+writes+closes+disables+info_calls+preload_calls+amp_calls;}
// A failed reset retains custody: lease revoked but held, nothing frees the next owner, no retry.
static void Retained(Codec&core,Port&port,const AudioLease&lease,uint64_t ticket){
 assert(CommonAudioAuthority().Holds(lease)&&!CommonAudioAuthority().Current(lease)&&!core.MuseCodecRelease(lease));
 assert(!core.MuseEnableInputChecked(lease,true)&&!core.MuseEnableOutputChecked(lease,true));
 const int before=Io(),calls=port.assert_calls+port.release_calls;
 assert(core.MuseResetAmplifierChecked(lease,ticket+1).outcome==Reset_::Unavailable&&Io()==before&&port.assert_calls+port.release_calls==calls);
}
int main(int argc,char**argv){
 assert(argc==2);const int test=std::atoi(argv[1]);Reset();Dma();auto core=Make();auto lease=Take();Port port;
 core.out_codec_if_=&amplifier;
 if(test==0){ // reset without an injected port or with a missing amplifier interface is Unavailable and does no IO
  assert(core.MuseCodecAcquire(lease));
  assert(core.MuseResetAmplifierChecked(lease,1).outcome==Reset_::Unavailable&&!Io()&&CommonAudioAuthority().Current(lease));
  core.SetMuseAmpResetPort(&port);core.out_codec_if_=nullptr;
  assert(core.MuseResetAmplifierChecked(lease,1).outcome==Reset_::Unavailable&&!Io()&&!port.assert_calls&&CommonAudioAuthority().Current(lease));
  core.out_codec_if_=&amplifier;Cleanup(core,lease);
 }
 else if(test==1){ // success path: ordering, bit preservation, non-terminal, single-use ticket
  core.SetMuseAmpResetPort(&port);assert(core.MuseCodecAcquire(lease));
  assert(core.MuseEnableInputChecked(lease,true)&&core.MuseEnableOutputChecked(lease,true)); // interrupted live output
  auto r=core.MuseResetAmplifierChecked(lease,1);
  assert(r.outcome==Reset_::AmplifierResetMutedVerified&&r.phase==RPhase::Complete&&r.assert_attempted&&r.release_attempted);
  assert(r.amplifier.Complete()&&r.discard==Codec::MuseDiscard::StoppedDmaCleared);
  assert(port.assert_calls==1&&port.release_calls==1&&port.latch==0b10100111); // other bits untouched, line released
  assert(port.delays==(std::vector<uint32_t>{Codec::MuseResetHoldMs,Codec::MuseResetSettleMs}));
  // Everything quiet happened before the line moved: mute, both stops, zeroed DMA.
  assert(port.mute_at_assert&0x10&&port.in_off_at_assert==1&&port.out_off_at_assert==1&&port.preload_at_assert>0);
  assert(port.amp_calls_at_release==::amp_calls-7); // fresh mute and readback only after release
  assert((mute_reg&0x10)&&(system_reg&0x03)&&!(system_reg&0x40)); // registers read back muted
  assert(CommonAudioAuthority().Current(lease)); // non-terminal: same owner continues
  // Single-use tickets, including ones that succeeded.
  const int before=Io();assert(core.MuseResetAmplifierChecked(lease,1).outcome==Reset_::Unavailable&&Io()==before&&port.assert_calls==1);
  assert(core.MuseEnableOutputChecked(lease,true)&&core.MuseStopOutputChecked(lease)); // owner may resume
  auto again=core.MuseResetAmplifierChecked(lease,2);assert(again.outcome==Reset_::AmplifierResetMutedVerified&&port.assert_calls==2);
  Cleanup(core,lease);
 }
 else if(test==2){ // repeated cycles stay bounded
  core.SetMuseAmpResetPort(&port);assert(core.MuseCodecAcquire(lease));
  for(uint64_t n=1;n<=25;++n){assert(core.MuseEnableOutputChecked(lease,true));assert(core.MuseResetAmplifierChecked(lease,n).outcome==Reset_::AmplifierResetMutedVerified);}
  assert(port.assert_calls==25&&port.release_calls==25&&port.latch==0b10100111&&port.delays.size()==50&&CommonAudioAuthority().Current(lease));
  Cleanup(core,lease);
 }
 else if(test==3){ // admission rejects before IO without consuming the ticket or ownership
  core.SetMuseAmpResetPort(&port);assert(core.MuseCodecAcquire(lease));
  assert(core.MuseResetAmplifierChecked(lease,0).outcome==Reset_::Unavailable&&!Io());
  volume_hook=[&]{assert(core.MuseResetAmplifierChecked(lease,1).outcome==Reset_::Unavailable);};
  core.SetOutputVolume(20);assert(volume_sets==1&&!port.assert_calls&&!opens&&!disables&&!preload_calls&&CommonAudioAuthority().Current(lease));
  AudioOwnerAuthority foreign;AudioLease other;assert(foreign.Acquire(AudioOwner::Muse,AudioPurpose::MusePushToTalk,7,other));
  assert(core.MuseResetAmplifierChecked(other,1).outcome==Reset_::Unavailable&&!Io());
  assert(core.MuseStopInputChecked(lease)&&core.MuseStopOutputChecked(lease));
  assert(core.MuseDiscardOutputChecked(lease,5)==Codec::MuseDiscard::StoppedDmaCleared);
  // Not newer than the last discard ticket; not consumed by the rejection.
  const int before=Io();assert(core.MuseResetAmplifierChecked(lease,5).outcome==Reset_::Unavailable&&Io()==before&&!port.assert_calls);
  // A completed discard leaves the preload cursor full until a fresh stop, so the reset must follow a live enable.
  assert(core.MuseEnableOutputChecked(lease,true));assert(core.MuseResetAmplifierChecked(lease,6).outcome==Reset_::AmplifierResetMutedVerified);Cleanup(core,lease);
 }
 else if(test==4){ // callbacks and every owner entry are fenced while the line is moving
  core.SetMuseAmpResetPort(&port);assert(core.MuseCodecAcquire(lease));
  port.hook=[&](bool released){const int before=Io();
   assert(core.MuseResetAmplifierChecked(lease,9).outcome==Reset_::Unavailable);
   assert(core.MuseDiscardOutputChecked(lease,9)==Codec::MuseDiscard::Unavailable);
   assert(!core.MuseEnableInputChecked(lease,true)&&!core.MuseEnableOutputChecked(lease,true)&&!core.MuseCodecRelease(lease));
   assert(!core.MuseStopInputChecked(lease)&&!core.MuseStopOutputChecked(lease)&&!core.MuseDrainBegin(lease,9));
   core.SetOutputVolume(77);assert(!volume_sets);
   (void)released;assert(Io()==before);};
  assert(core.MuseResetAmplifierChecked(lease,1).outcome==Reset_::AmplifierResetMutedVerified);
  port.hook={};core.SetOutputVolume(77);assert(volume_sets==1); // fence lifts only on success
  Cleanup(core,lease);
 }
 else if(test==5){ // line assert failure: release never attempted, custody retained
  port.assert_status=-5;core.SetMuseAmpResetPort(&port);assert(core.MuseCodecAcquire(lease));
  auto r=core.MuseResetAmplifierChecked(lease,1);
  assert(r.outcome==Reset_::Failed&&r.phase==RPhase::Assert&&r.port_status==-5&&r.assert_attempted&&!r.release_attempted&&!port.release_calls&&port.latch==0b10100111);
  Retained(core,port,lease,1);
 }
 else if(test==6){ // release failure: amplifier may be held in reset; stays fenced and muted
  port.release_status=-6;core.SetMuseAmpResetPort(&port);assert(core.MuseCodecAcquire(lease));
  auto r=core.MuseResetAmplifierChecked(lease,1);
  assert(r.outcome==Reset_::Failed&&r.phase==RPhase::Release&&r.port_status==-6&&r.release_attempted&&port.latch==0b10100011);
  const int amp_after=amp_calls;Retained(core,port,lease,1);assert(amp_calls==amp_after);
 }
 else if(test>=10&&test<=23){ // driver failure at every amplifier call, before and after the line moves
  const int at=test-9;fail_at=at;fail_status=-73;core.SetMuseAmpResetPort(&port);assert(core.MuseCodecAcquire(lease));
  auto r=core.MuseResetAmplifierChecked(lease,1);
  assert(r.outcome==Reset_::Failed&&r.phase==(at<=7?RPhase::Mute:RPhase::Requiesce)&&r.amplifier.fault==codec_quiet::Fault::DriverError&&amp_calls==at);
  assert(port.assert_calls==(at<=7?0:1)&&port.release_calls==(at<=7?0:1));
  fail_at=0;Retained(core,port,lease,1);
 }
 else if(test==30){ // digital discard failure happens before the line moves
  error_preload=1;core.SetMuseAmpResetPort(&port);assert(core.MuseCodecAcquire(lease));
  auto r=core.MuseResetAmplifierChecked(lease,1);
  assert(r.outcome==Reset_::Failed&&r.phase==RPhase::DigitalDiscard&&r.discard==Codec::MuseDiscard::Failed&&!port.assert_calls);Retained(core,port,lease,1);
 }
 else if(test==31){ // stop failure happens before the line moves
  core.SetMuseAmpResetPort(&port);assert(core.MuseCodecAcquire(lease));assert(core.MuseEnableInputChecked(lease,true));
  disable_result=-1;auto r=core.MuseResetAmplifierChecked(lease,1);
  assert(r.outcome==Reset_::Failed&&r.phase==RPhase::InputStop&&!port.assert_calls&&!preload_calls);disable_result=0;Retained(core,port,lease,1);
 }
 else if(test==32){ // authority revoked while the line is low: no release, custody retained
  core.SetMuseAmpResetPort(&port);assert(core.MuseCodecAcquire(lease));
  port.hook=[&](bool released){if(!released)RevokeAudio(lease);};
  auto r=core.MuseResetAmplifierChecked(lease,1);
  assert(r.outcome==Reset_::Failed&&r.binding_or_authority_lost&&r.phase==RPhase::Assert&&!port.release_calls&&port.latch==0b10100011);
  port.hook={};Retained(core,port,lease,1);
 }
 else if(test==33){ // owner replaced mid-reset: the successor is never touched
  core.SetMuseAmpResetPort(&port);assert(core.MuseCodecAcquire(lease));AudioLease next;
  port.hook=[&](bool released){if(!released){assert(ReleaseAudio(lease,true));assert(AcquireAudio(AudioOwner::Stock,AudioPurpose::StockRuntime,99,next));}};
  auto r=core.MuseResetAmplifierChecked(lease,1);
  assert(r.outcome==Reset_::Failed&&r.binding_or_authority_lost&&!port.release_calls&&CommonAudioAuthority().Current(next));
 }
 else if(test==34){ // authority revoked during the hold delay: the line must stay asserted
  core.SetMuseAmpResetPort(&port);assert(core.MuseCodecAcquire(lease));
  port.delay_hook=[&](uint32_t){RevokeAudio(lease);};
  auto r=core.MuseResetAmplifierChecked(lease,1);
  assert(r.outcome==Reset_::Failed&&r.binding_or_authority_lost&&r.phase==RPhase::Assert&&!r.release_attempted&&!port.release_calls&&port.latch==0b10100011);
  port.delay_hook={};Retained(core,port,lease,1);
 }
 else assert(false);
 std::puts("PASS amplifier reset: admission, ordering, fencing, failure custody, repeat cycles; no physical flush or audible-silence claim");
}
