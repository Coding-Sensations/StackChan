#include <companion/audio_owner.hpp>
#include <mutex>
#include <vector>
#include <cassert>
#include <functional>
#include <algorithm>
#include <cstdio>
#define private public
#include "hal/board/cores3_audio_codec.h"
#undef private
namespace companion::diagnostic {
enum class StageCode {DriverInputOpen,DriverInputGain,DriverInputClose,DriverRead,DriverOutputOpen,DriverOutputVolume,DriverOutputClose,DriverWrite};
using FaultCode=StageCode;
}
template<class T>void companion_codec_stage(T){}
template<class T>void companion_codec_fault(T,int){}
#define ESP_ERROR_CHECK_WITHOUT_ABORT(x) ((void)(x))
#define ESP_ERROR_CHECK(x) assert((x)==0)
static int open_result=0,close_result=0,disable_result=0,read_result=0,write_result=0;
static int opens=0,closes=0,disables=0,reads=0,writes=0;
static std::function<void()> during;
static void Hook(){auto f=std::move(during);during={};if(f)f();}
static int esp_codec_dev_open(void*,esp_codec_dev_sample_info_t*){++opens;Hook();return open_result;}
static int esp_codec_dev_close(void*){++closes;Hook();return close_result;}
static int esp_codec_dev_set_in_channel_gain(void*,int,int){return 0;}
static int esp_codec_dev_set_out_vol(void*,int){return 0;}
static int esp_codec_dev_read(void*,void* out,size_t size){++reads;std::fill_n(static_cast<int16_t*>(out),size/2,42);Hook();return read_result;}
static int esp_codec_dev_write(void*,void*,size_t){++writes;Hook();return write_result;}
static int i2s_channel_disable(void*){++disables;Hook();return disable_result;}
CoreS3AudioCodec::CoreS3AudioCodec(void*,int input,int output,gpio_num_t,gpio_num_t,gpio_num_t,gpio_num_t,gpio_num_t,uint8_t,uint8_t,bool reference){input_sample_rate_=input;output_sample_rate_=output;input_reference_=reference;input_channels_=reference?2:1;direct_callback_ready_=true;input_dev_=reinterpret_cast<void*>(1);output_dev_=reinterpret_cast<void*>(2);}
CoreS3AudioCodec::~CoreS3AudioCodec()=default;
ACTUAL_METHODS
using namespace companion;
static AudioLease Take(AudioOwner owner=AudioOwner::Muse,AudioPurpose purpose=AudioPurpose::MusePushToTalk,uint64_t epoch=7){AudioLease lease;assert(AcquireAudio(owner,purpose,epoch,lease));return lease;}
static void Reset(){open_result=close_result=disable_result=read_result=write_result=0;opens=closes=disables=reads=writes=0;during={};}
static void Cleanup(CoreS3AudioCodec& codec,AudioLease& lease){assert(codec.MuseStopInputChecked(lease));assert(codec.MuseStopOutputChecked(lease));assert(codec.MuseCodecRelease(lease));assert(CommonAudioAuthority().Holds(lease));assert(ReleaseAudio(lease,true));}
static CoreS3AudioCodec Make(){return {nullptr,24000,24000,0,0,0,0,0,0,0,true};}
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
 std::puts("PASS actual CoreS3 Muse original-lease binding, wrong-owner exclusion, retained failures, DMA ticket and Stock/Mira/DirectUsb custody regression; synthetic drivers only");
}
