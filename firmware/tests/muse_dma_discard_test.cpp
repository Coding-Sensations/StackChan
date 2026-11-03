#include "muse_codec_fixture.hpp"
using Discard=CoreS3AudioCodec::MuseDiscard;
static void ResetDma(){Reset();dma_info={};std::fill(dma_bytes.begin(),dma_bytes.end(),0x6d);dma_cursor=largest_preload=0;dma_capacity=5760;info_calls=preload_calls=info_error=error_preload=partial_preload=0;info_hook={};preload_hook={};}
inline void Stopped(CoreS3AudioCodec& core,AudioLease& lease){assert(core.MuseCodecAcquire(lease));assert(core.MuseStopInputChecked(lease)&&core.MuseStopOutputChecked(lease));}
int main(){
 {
  ResetDma();auto core=Make();auto lease=Take();assert(core.MuseCodecAcquire(lease));
  assert(core.MuseDiscardOutputChecked(lease,1)==Discard::Unavailable&&!info_calls&&!preload_calls);
  assert(core.MuseStopInputChecked(lease)&&core.MuseStopOutputChecked(lease));
  assert(core.MuseDiscardOutputChecked(lease,1)==Discard::StoppedDmaCleared&&info_calls==2&&preload_calls==23&&largest_preload<=256&&dma_cursor==5760);
  assert(std::all_of(dma_bytes.begin(),dma_bytes.begin()+5760,[](auto x){return x==0;}));assert(dma_bytes[5760]==0x6d);
  assert(core.MuseDiscardOutputChecked(lease,1)==Discard::Unavailable&&info_calls==2&&preload_calls==23);
  Cleanup(core,lease);
 }
 {
  ResetDma();auto core=Make();auto lease=Take();Stopped(core,lease);
  assert(core.MuseDiscardOutputChecked(lease,1)==Discard::StoppedDmaCleared);
  assert(core.MuseEnableOutputChecked(lease,true)&&core.MuseStopOutputChecked(lease));
  std::fill(dma_bytes.begin(),dma_bytes.end(),0x6d);
  assert(core.MuseDiscardOutputChecked(lease,2)==Discard::StoppedDmaCleared&&preload_calls==46&&dma_cursor==5760);Cleanup(core,lease);
 }
 {
  ResetDma();auto core=Make();auto lease=Take();Stopped(core,lease);
  assert(core.MuseDiscardOutputChecked(lease,1)==Discard::StoppedDmaCleared);
  // No fresh disable means preload cursor remains full; never invent success.
  assert(core.MuseDiscardOutputChecked(lease,2)==Discard::Failed&&preload_calls==24);Cleanup(core,lease);
 }
 for(unsigned fault=0;fault<10;++fault){
  ResetDma();auto core=Make();auto lease=Take();Stopped(core,lease);
  if(fault==0)dma_info.total_dma_buf_size=0;
  if(fault==1)dma_info.total_dma_buf_size=7;
  if(fault==2)dma_info.total_dma_buf_size=16388;
  if(fault==3)dma_info.is_enabled=true;
  if(fault==4)dma_info.dir=I2S_DIR_RX;
  if(fault==5)dma_info.mode=99;
  if(fault==6)info_error=-1;
  if(fault==7)preload_hook=[]{dma_info.total_dma_buf_size=4;};
  if(fault==8)preload_hook=[]{dma_info.is_enabled=true;};
  if(fault==9)preload_hook=[]{info_error=-1;};
  assert(core.MuseDiscardOutputChecked(lease,1)==Discard::Failed&&!CommonAudioAuthority().Current(lease));
  assert(preload_calls==(fault<7?0:23));const int calls=info_calls+preload_calls;
  assert(core.MuseDiscardOutputChecked(lease,1)==Discard::Unavailable&&calls==info_calls+preload_calls);
  // Failed discard never claims flush; existing checked stop/release obligations remain.
  Cleanup(core,lease);if(fault==3||fault==8)assert(output_disables==2);
 }
 for(int at:{1,12,23})for(bool partial:{false,true}){
  ResetDma();auto core=Make();auto lease=Take();Stopped(core,lease);
  if(partial)partial_preload=at;else error_preload=at;
  assert(core.MuseDiscardOutputChecked(lease,1)==Discard::Failed&&preload_calls==at&&CommonAudioAuthority().Holds(lease)&&!CommonAudioAuthority().Current(lease));
  assert(core.MuseDiscardOutputChecked(lease,1)==Discard::Unavailable&&preload_calls==at);Cleanup(core,lease);
 }
 {
  ResetDma();auto core=Make();auto lease=Take();Stopped(core,lease);dma_info.total_dma_buf_size=16384;dma_capacity=16384;
  preload_hook=[&]{assert(core.MuseDiscardOutputChecked(lease,2)==Discard::Unavailable);assert(!core.MuseEnableInputChecked(lease,true)&&!core.MuseEnableOutputChecked(lease,true)&&!core.MuseCodecRelease(lease));};
  assert(core.MuseDiscardOutputChecked(lease,1)==Discard::StoppedDmaCleared&&preload_calls==64&&largest_preload==256&&!opens);
  assert(std::all_of(dma_bytes.begin(),dma_bytes.end(),[](auto x){return x==0;}));Cleanup(core,lease);
 }
 {
  ResetDma();auto core=Make();auto lease=Take();Stopped(core,lease);RevokeAudio(lease);
  assert(core.MuseDiscardOutputChecked(lease,1)==Discard::StoppedDmaCleared&&!CommonAudioAuthority().Current(lease));Cleanup(core,lease);
 }
 {
  ResetDma();auto core=Make();auto lease=Take();Stopped(core,lease);preload_hook=[&]{RevokeAudio(lease);};
  assert(core.MuseDiscardOutputChecked(lease,1)==Discard::StoppedDmaCleared&&!CommonAudioAuthority().Current(lease));Cleanup(core,lease);
 }
 {
  ResetDma();auto core=Make();auto lease=Take();Stopped(core,lease);AudioOwnerAuthority foreign;AudioLease other;
  assert(foreign.Acquire(AudioOwner::Muse,AudioPurpose::MusePushToTalk,7,other));
  assert(core.MuseDiscardOutputChecked(other,1)==Discard::Unavailable&&core.MuseDiscardOutputChecked(lease,0)==Discard::Unavailable&&!info_calls&&!preload_calls);
  auto stale=lease;Cleanup(core,lease);auto successor=Take();assert(core.MuseCodecAcquire(successor));
  assert(core.MuseDiscardOutputChecked(stale,1)==Discard::Unavailable&&CommonAudioAuthority().Current(successor)&&!info_calls&&!preload_calls);
  assert(core.MuseStopInputChecked(successor)&&core.MuseStopOutputChecked(successor));
  assert(core.MuseDiscardOutputChecked(successor,1)==Discard::StoppedDmaCleared);Cleanup(core,successor);
 }
 {
  ResetDma();auto core=Make();auto lease=Take();assert(core.MuseCodecAcquire(lease));assert(core.MuseStopOutputChecked(lease));disable_result=-1;assert(!core.MuseStopInputChecked(lease));
  assert(core.MuseDiscardOutputChecked(lease,1)==Discard::Unavailable&&!info_calls&&!preload_calls);disable_result=0;Cleanup(core,lease);
 }
 std::puts("PASS actual stopped DMA discard, bounds/replay/fault/reentrant/original-scope tests; no native permission or acoustic/physical flush claim");
}
