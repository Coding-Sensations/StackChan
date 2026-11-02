#include "muse_codec_fixture.hpp"
int main(){
 for(auto rates:{std::pair<int,int>{16000,24000},{24000,16000},{48000,48000}}){
  Reset();CoreS3AudioCodec codec{nullptr,rates.first,rates.second,0,0,0,0,0,0,0,true};auto lease=Take();assert(!codec.MuseCodecAcquire(lease));assert(!opens&&!closes&&!disables);assert(CommonAudioAuthority().Current(lease));assert(ReleaseAudio(lease,true));
 }
 for(int direction=0;direction<3;++direction){
  Reset();auto codec=Make();auto lease=Take();assert(codec.MuseCodecAcquire(lease));
  if(direction==2)assert(codec.MuseEnableOutputChecked(lease,true));
  during=[&]{RevokeAudio(lease);};
  if(direction==0)assert(!codec.MuseEnableInputChecked(lease,true));
  else if(direction==1)assert(!codec.MuseEnableOutputChecked(lease,true));
  else {std::vector<int16_t> data(960,3);assert(!codec.MuseOutputDataChecked(lease,data));}
  assert(!CommonAudioAuthority().Current(lease)&&CommonAudioAuthority().Holds(lease));Cleanup(codec,lease);
 }

 {
  Reset();auto codec=Make();AudioLease none;assert(!codec.MuseCodecAcquire(none));AudioOwnerAuthority foreign;AudioLease other;assert(foreign.Acquire(AudioOwner::Muse,AudioPurpose::MusePushToTalk,7,other));assert(!codec.MuseCodecAcquire(other));assert(!opens&&!disables&&!closes);
 }
 for(auto owner:{AudioOwner::Mira,AudioOwner::Stock,AudioOwner::Native,AudioOwner::DirectUsb}){
  Reset();auto codec=Make();auto purpose=owner==AudioOwner::Mira?AudioPurpose::Session:owner==AudioOwner::Stock?AudioPurpose::StockRuntime:owner==AudioOwner::Native?AudioPurpose::NativeRuntime:AudioPurpose::DirectUsbSession;auto lease=Take(owner,purpose);assert(!codec.MuseCodecAcquire(lease));assert(CommonAudioAuthority().Current(lease));assert(!opens&&!disables&&!closes);assert(ReleaseAudio(lease,true));
 }
 {
  Reset();auto codec=Make();auto lease=Take();assert(codec.MuseCodecAcquire(lease));assert(CurrentAudioOwner()==AudioOwner::Muse);assert(!codec.MuseCodecAcquire(lease));assert(!opens&&!reads&&!writes&&!disables);
  auto g=lease.generation();std::vector<int16_t> data(960,0);
  assert(!codec.DirectUsbEnableInputChecked(g,true));assert(!codec.DirectUsbEnableOutputChecked(g,true));assert(!codec.DirectUsbInputDataChecked(g,data));assert(!codec.DirectUsbOutputDataChecked(g,data));assert(!codec.DirectUsbStopInputChecked(g));assert(!codec.DirectUsbStopOutputChecked(g));assert(!codec.DirectUsbCodecRelease(g));assert(!codec.EnableInputChecked(true)&&!codec.EnableOutputChecked(true));assert(!opens&&!closes&&!disables);
  assert(codec.MuseEnableInputChecked(lease,true));assert(codec.MuseInputDataChecked(lease,data));assert(std::all_of(data.begin(),data.end(),[](auto x){return x==42;}));
  assert(codec.MuseEnableOutputChecked(lease,true));assert(codec.MuseOutputDataChecked(lease,data));
  assert(!codec.MuseDrainBegin(lease,0));assert(codec.MuseDrainBegin(lease,11));assert(!codec.MuseDrainBegin(lease,12));assert(codec.MuseDrainState(lease,10)==CoreS3AudioCodec::MuseDrain::Unavailable);assert(codec.MuseDrainState(lease,11)==CoreS3AudioCodec::MuseDrain::Pending);assert(!codec.MuseOutputDataChecked(lease,data));
  // Writes during a drain refuse/revoke; cleanup still owns the same lease.
  for(int round=0;round<2;++round)for(uintptr_t id=1;id<=6;++id){i2s_event_data_t event{reinterpret_cast<void*>(id),64};CoreS3AudioCodec::DirectOnSent(reinterpret_cast<void*>(2),&event,&codec);}
  assert(codec.MuseDrainState(lease,11)==CoreS3AudioCodec::MuseDrain::Sent);
  assert(!codec.MuseCodecRelease(lease));assert(CommonAudioAuthority().Holds(lease));Cleanup(codec,lease);
  // Checked hardware release does not create a window for legacy IO before
  // original logical custody release; after release, Stock/Mira remain usable.
  auto stock=Take(AudioOwner::Stock,AudioPurpose::StockRuntime);assert(codec.EnableOutputChecked(true));assert(codec.EnableOutputChecked(false));assert(ReleaseAudio(stock,true));
 }
 {
  Reset();auto codec=Make();auto lease=Take();assert(codec.MuseCodecAcquire(lease));close_result=-1;assert(!codec.MuseStopInputChecked(lease));close_result=0;assert(codec.MuseStopOutputChecked(lease));assert(!codec.MuseCodecRelease(lease));Cleanup(codec,lease);
 }
 {
  Reset();auto codec=Make();assert(codec.DirectUsbCodecAcquire(1));close_result=-1;assert(!codec.DirectUsbStopInputChecked(1));close_result=0;assert(codec.DirectUsbStopOutputChecked(1));assert(!codec.DirectUsbCodecRelease(1));assert(codec.DirectUsbStopInputChecked(1));assert(codec.DirectUsbCodecRelease(1));
 }
 for(int failure=0;failure<3;++failure){
  Reset();auto codec=Make();auto lease=Take();assert(codec.MuseCodecAcquire(lease));
  if(failure==0){open_result=-1;assert(!codec.MuseEnableInputChecked(lease,true));open_result=0;}
  else {assert(codec.MuseEnableInputChecked(lease,true));if(failure==1)disable_result=-1;else close_result=-1;assert(!codec.MuseStopInputChecked(lease));disable_result=close_result=0;}
  assert(CommonAudioAuthority().Holds(lease));AudioLease denied;assert(!AcquireAudio(AudioOwner::Stock,AudioPurpose::StockRuntime,9,denied));assert(!codec.MuseCodecRelease(lease));Cleanup(codec,lease);
 }
 {
  Reset();auto codec=Make();auto lease=Take();assert(codec.MuseCodecAcquire(lease));assert(codec.MuseEnableInputChecked(lease,true));std::vector<int16_t> data(960,3);during=[&]{RevokeAudio(lease);};assert(!codec.MuseInputDataChecked(lease,data));assert(std::all_of(data.begin(),data.end(),[](auto x){return x==0;}));assert(!CommonAudioAuthority().Current(lease));Cleanup(codec,lease);
 }
 {
  Reset();auto codec=Make();auto lease=Take();assert(codec.MuseCodecAcquire(lease));assert(codec.MuseStopInputChecked(lease));assert(codec.MuseStopOutputChecked(lease));assert(codec.MuseCodecRelease(lease));assert(!codec.EnableInputChecked(true));AudioLease denied;assert(!AcquireAudio(AudioOwner::Mira,AudioPurpose::Session,9,denied));auto stale=lease;assert(ReleaseAudio(lease,true));
  auto fresh=Take(AudioOwner::Muse,AudioPurpose::MusePushToTalk,8);assert(codec.MuseCodecAcquire(fresh));const auto count=disables;assert(!codec.MuseStopInputChecked(stale));assert(disables==count);Cleanup(codec,fresh);
 }
 {
  Reset();auto codec=Make();assert(codec.DirectUsbCodecAcquire(1));auto lease=codec.direct_common_lease_;assert(lease.owner()==AudioOwner::DirectUsb);assert(!codec.MuseCodecAcquire(lease));assert(!codec.MuseEnableInputChecked(lease,true));assert(codec.DirectUsbEnableInputChecked(1,true));assert(codec.DirectUsbEnableOutputChecked(1,true));std::vector<int16_t> data(960);assert(codec.DirectUsbInputDataChecked(1,data)&&codec.DirectUsbOutputDataChecked(1,data));assert(codec.DirectUsbStopInputChecked(1));assert(codec.DirectUsbStopOutputChecked(1));assert(codec.DirectUsbCodecRelease(1));assert(CurrentAudioOwner()==AudioOwner::None);
 }
 {
  Reset();auto codec=Make();assert(codec.DirectUsbCodecAcquire(3));assert(codec.DirectUsbStopInputChecked(3)&&codec.DirectUsbStopOutputChecked(3)&&codec.DirectUsbCodecRelease(3));
  auto muse=Take(AudioOwner::Muse,AudioPurpose::MusePushToTalk,81);assert(codec.MuseCodecAcquire(muse));Cleanup(codec,muse);
  assert(codec.DirectUsbCodecAcquire(4));assert(codec.DirectUsbStopInputChecked(4)&&codec.DirectUsbStopOutputChecked(4)&&codec.DirectUsbCodecRelease(4));
 }
 {
  Reset();auto codec=Make();auto first=Take(AudioOwner::Muse,AudioPurpose::MusePushToTalk,82);assert(codec.MuseCodecAcquire(first));Cleanup(codec,first);
  const auto stops=disables,closed=closes,rx_stops=input_disables,tx_stops=output_disables;
  auto second=Take(AudioOwner::Muse,AudioPurpose::MusePushToTalk,83);assert(codec.MuseCodecAcquire(second));
  assert(codec.MuseStopInputChecked(second)&&codec.MuseStopOutputChecked(second)&&codec.MuseCodecRelease(second));
  assert(disables==stops&&closes==closed);assert(ReleaseAudio(second,true));
  auto third=Take(AudioOwner::Muse,AudioPurpose::MusePushToTalk,84);assert(codec.MuseCodecAcquire(third));
  assert(codec.MuseEnableInputChecked(third,true));assert(codec.MuseStopInputChecked(third)&&codec.MuseStopOutputChecked(third)&&codec.MuseCodecRelease(third));
  // Input set_fs can restart paired TX: fresh RX and TX stops, only input codec close.
  assert(disables==stops+2&&input_disables==rx_stops+1&&output_disables==tx_stops+1&&closes==closed+1);assert(ReleaseAudio(third,true));
 }

 // Cached receipts survive a new Muse lease only while hardware has not been
 // reopened. Legacy Stock/Mira opens invalidate proof even on partial failure.
 for(auto owner:{AudioOwner::Stock,AudioOwner::Mira})for(int fault=0;fault<2;++fault)for(int mode=0;mode<6;++mode){
  Reset();auto codec=Make();auto first=Take();assert(codec.MuseCodecAcquire(first));Cleanup(codec,first);
  const bool output=mode%2;auto legacy=Take(owner,owner==AudioOwner::Mira?AudioPurpose::Session:AudioPurpose::StockRuntime);
  if(mode<2){if(output){codec.EnableOutput(true);codec.EnableOutput(false);}else{codec.EnableInput(true);codec.EnableInput(false);}}
  else {if(mode>=4)open_result=-1;const bool opened=output?codec.EnableOutputChecked(true):codec.EnableInputChecked(true);assert(opened==(mode<4));open_result=0;assert(output?codec.EnableOutputChecked(false):codec.EnableInputChecked(false));}
  assert(ReleaseAudio(legacy,true));auto next=Take();assert(codec.MuseCodecAcquire(next));const int stops=disables,closed=closes;if(fault==0)disable_result=-1;else close_result=-1;
  assert(!(output?codec.MuseStopOutputChecked(next):codec.MuseStopInputChecked(next)));assert(disables==stops+1&&closes==closed+(fault?1:0)&&CommonAudioAuthority().Holds(next));AudioLease denied;assert(!AcquireAudio(AudioOwner::Stock,AudioPurpose::StockRuntime,9,denied));disable_result=close_result=0;Cleanup(codec,next);
 }

 // The pinned I2S set_fs input path may restart paired TX for its clock.
 // An input-only reopen must invalidate TX digital stop, not codec-close proof.
 for(auto owner:{AudioOwner::Stock,AudioOwner::Mira})for(int mode=0;mode<3;++mode){
  Reset();auto codec=Make();auto first=Take();assert(codec.MuseCodecAcquire(first));Cleanup(codec,first);auto legacy=Take(owner,owner==AudioOwner::Mira?AudioPurpose::Session:AudioPurpose::StockRuntime);
  if(mode==0){codec.EnableInput(true);codec.EnableInput(false);}else{if(mode==2)open_result=-1;assert(codec.EnableInputChecked(true)==(mode==1));open_result=0;assert(codec.EnableInputChecked(false));}assert(ReleaseAudio(legacy,true));auto next=Take();assert(codec.MuseCodecAcquire(next));const int stops=disables,closed=closes;disable_result=-1;assert(!codec.MuseStopOutputChecked(next));assert(disables==stops+1&&closes==closed&&CommonAudioAuthority().Holds(next));disable_result=0;assert(codec.MuseStopOutputChecked(next));assert(closes==closed);Cleanup(codec,next);
 }
 {
  Reset();auto codec=Make();assert(codec.DirectUsbCodecAcquire(12));assert(codec.DirectUsbStopInputChecked(12)&&codec.DirectUsbStopOutputChecked(12));assert(codec.DirectUsbEnableInputChecked(12,true));const int stops=disables,closed=closes;disable_result=-1;assert(!codec.DirectUsbStopOutputChecked(12));assert(disables==stops+1&&closes==closed);disable_result=0;assert(codec.DirectUsbStopInputChecked(12)&&codec.DirectUsbStopOutputChecked(12)&&codec.DirectUsbCodecRelease(12));assert(closes==closed+1);
 }
 std::puts("PASS actual CoreS3 Muse original-lease binding, wrong-owner exclusion, retained failures, DMA ticket and Stock/Mira/DirectUsb custody regression; synthetic drivers only");
}
