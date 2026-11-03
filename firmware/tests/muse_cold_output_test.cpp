#include "muse_codec_fixture.hpp"
#include "muse_amplifier_fixture.hpp"
#include <cstdlib>
#include <thread>
#include <type_traits>
template<class T,class=void>struct HasCallerAddress:std::false_type{};
template<class T>struct HasCallerAddress<T,std::void_t<decltype(T{}.amplifier_address)>>:std::true_type{};
static_assert(!HasCallerAddress<CoreS3AudioCodec::MuseColdOutputConfig>::value,"cold CoreS3 interface must not target a caller-selected I2C slave");
static int Calls(){return AudioCodec::legacy_starts+cold_calls+amp_calls+opens+closes+disables+reads+writes+info_calls+preload_calls+volume_sets;}
static void Blocked(Codec&core,AudioLease&lease,const Codec::MuseColdOutputConfig&config){
 const int calls=Calls();assert(CommonAudioAuthority().Holds(lease)&&!CommonAudioAuthority().Current(lease));
 assert(!core.MuseColdInitialize(lease,config).Complete());assert(!core.MuseStopInputChecked(lease)&&!core.MuseStopOutputChecked(lease)&&!core.MuseCodecRelease(lease));
 assert(core.MuseQuiesceForRetirementChecked(lease,9).outcome==Outcome::Unavailable);assert(!core.direct_input_stopped_&&!core.direct_input_closed_&&!core.direct_output_stopped_&&!core.direct_output_closed_);assert(Calls()==calls);
}
static void NoPcm(Codec&core,AudioLease&lease){
 const int calls=Calls();std::vector<int16_t>data(96,42);
 core.Start();core.EnableInput(true);core.EnableOutput(true);core.EnableInput(false);core.EnableOutput(false);core.SetOutputVolume(99);assert(!core.EnableInputChecked(true)&&!core.EnableOutputChecked(true));assert(!core.MuseEnableInputChecked(lease,true)&&!core.MuseEnableOutputChecked(lease,true)&&!core.MuseEnableInputChecked(lease,false)&&!core.MuseEnableOutputChecked(lease,false));
 assert(!core.MuseInputDataChecked(lease,data)&&!core.MuseOutputDataChecked(lease,data));assert(!core.DirectUsbCodecAcquire(10)&&!core.DirectUsbEnableInputChecked(lease.generation(),true)&&!core.DirectUsbEnableOutputChecked(lease.generation(),true));assert(!core.MuseCodecAcquire(lease));assert(Calls()==calls);
}
static void Success(Codec&core,AudioLease&lease){
 const bool previously_confirmed=core.muse_cold_output_confirmed_;
 companion::muse::QuiesceDiagnostic report;auto result=core.MuseQuiesceForRetirementChecked(lease,1,&report);
 assert(result.outcome==Outcome::RegistersAndStoppedDma&&result.input_never_created&&result.output_never_started);
 assert(result.input_stop_attempts==0&&result.output_stop_attempts==(previously_confirmed?0:4));
 assert(input_disables==0&&output_disables==0&&opens==0&&closes==0);
 assert(!core.direct_input_stopped_&&!core.direct_input_closed_&&!core.direct_output_stopped_&&!core.direct_output_closed_);
 assert(report.Read().evidence.input_never_created);char text[companion::muse::QuiesceDiagnostic::TextCapacity];assert(report.Format(text,sizeof text));
 NoPcm(core,lease);assert(core.MuseCodecRelease(lease));assert(CommonAudioAuthority().Holds(lease));assert(ReleaseAudio(lease,true));
}
int main(int argc,char**argv){assert(argc==2);const int test=std::atoi(argv[1]);Reset();auto normal=Make();assert(!normal.ColdOutputOnlyInstance());normal.Start();assert(AudioCodec::legacy_starts==1);AudioCodec::legacy_starts=0;cold_amplifier=&amplifier;
 auto&core=Codec::MuseColdOutputOnly();assert(Calls()==0&&CurrentAudioOwner()==AudioOwner::None);auto lease=Take();Codec::MuseColdOutputConfig config{};config.bus=reinterpret_cast<void*>(9);
 if(test==62)NoPcm(core,lease);
 if(test>=1&&test<=5)cold_fail=test;
 if(test==6)cold_null=true;
 if(test==7)cold_internal=false;
 if(test==8)info_error=81;
 if(test==9)dma_info.is_enabled=true;
 if(test==10)dma_info.dir=I2S_DIR_RX;
 if(test==11)dma_info.mode=9;
 if(test==12)dma_info.total_dma_buf_size=0;
 if(test==13)dma_info.total_dma_buf_size=16388;
 if(test>=14&&test<=20){fail_at=test-13;fail_status=test%2?97:-73;}
 if(test==21)cold_amplifier=nullptr;
 if(test==22){config.bus=nullptr;assert(!core.MuseColdInitialize(lease,config).Complete()&&Calls()==0&&CommonAudioAuthority().Current(lease));assert(ReleaseAudio(lease,true));return 0;}
 if(test==23){assert(ReleaseAudio(lease,true));lease=Take(AudioOwner::Stock,AudioPurpose::StockRuntime);assert(!core.MuseColdInitialize(lease,config).Complete()&&Calls()==0&&CommonAudioAuthority().Current(lease));assert(ReleaseAudio(lease,true));return 0;}
 if(test==24){assert(!normal.MuseColdInitialize(lease,config).Complete()&&Calls()==0&&CommonAudioAuthority().Current(lease));assert(ReleaseAudio(lease,true));return 0;}
 std::function<void()> hook;
 if(test>=26&&test<=30){const int target=test-25;hook=[&,target]{if(cold_calls==target){assert(ReleaseAudio(lease,true));lease=Take(AudioOwner::Stock,AudioPurpose::StockRuntime,99);}else cold_hook=hook;};cold_hook=hook;}
 if(test==31)info_hook=[&]{assert(ReleaseAudio(lease,true));lease=Take(AudioOwner::Stock,AudioPurpose::StockRuntime,99);};
 if(test>=32&&test<=38)amp_hook=[&](int n){if(n==test-31){assert(ReleaseAudio(lease,true));lease=Take(AudioOwner::Stock,AudioPurpose::StockRuntime,99);}};
 if(test==54)cold_hook=[&]{assert(RevokeAudio(lease));};
 if(test==55)amplifier.get_reg=nullptr;
 if(test==58)dma_info.total_dma_buf_size=5759;
 if(test==60)cold_hook=[&]{config.bus=reinterpret_cast<void*>(0x66);};
 if(test==53)info_hook=[&]{assert(core.MuseQuiesceForRetirementChecked(lease,1).outcome==Outcome::Unavailable);};
 if(test==39)cold_hook=[&]{const auto n=Calls();assert(!core.MuseColdInitialize(lease,config).Complete());assert(core.MuseQuiesceForRetirementChecked(lease,1).outcome==Outcome::Unavailable);assert(!core.MuseStopInputChecked(lease)&&!core.MuseStopOutputChecked(lease)&&!core.MuseCodecRelease(lease));NoPcm(core,lease);assert(Calls()==n);};
 if(test==61){Codec::MuseColdEvidence a,b;std::thread first([&]{a=core.MuseColdInitialize(lease,config);});std::thread second([&]{b=core.MuseColdInitialize(lease,config);});first.join();second.join();assert(a.Complete()!=b.Complete()&&cold_calls==5&&amp_calls==7&&info_calls==1);Success(core,lease);return 0;}
 auto evidence=core.MuseColdInitialize(lease,config);
 if(test==54||test==55||test==58){assert(!evidence.Complete());Blocked(core,lease,config);return 0;}
 if(test>=1&&test<=21){assert(!evidence.Complete());if(test==1||test==2)assert(evidence.fault==Codec::MuseColdFault::Driver&&evidence.status==cold_code);if((test>=3&&test<=6)||test==21)assert(evidence.fault==Codec::MuseColdFault::NoObject);if(test==8)assert(evidence.status==81);if(test>=14&&test<=20)assert(evidence.amplifier.status==fail_status);Blocked(core,lease,config);return 0;}
 if(test>=26&&test<=38){assert(!evidence.Complete()&&CommonAudioAuthority().Current(lease)&&lease.owner()==AudioOwner::Stock);const auto calls=Calls();assert(!core.MuseCodecRelease(lease)&&core.MuseQuiesceForRetirementChecked(lease,4).outcome==Outcome::Unavailable&&Calls()==calls);return 0;}
 assert(evidence.Complete()&&evidence.dma_bytes==5760&&core.rx_handle_==nullptr&&core.input_dev_==nullptr&&core.in_codec_if_==nullptr&&core.data_if_==nullptr&&core.output_dev_==nullptr);
 if(test==59){const auto calls=Calls();assert(&Codec::MuseColdOutputOnly()==&core&&Calls()==calls);}
 if(test==63||test==64){if(test==63)info_error=81;else dma_info.is_enabled=true;assert(!core.MuseStopOutputChecked(lease));Blocked(core,lease,config);return 0;}
 if(test==65){info_hook=[&]{assert(ReleaseAudio(lease,true));lease=Take(AudioOwner::Stock,AudioPurpose::StockRuntime,99);};assert(!core.MuseStopOutputChecked(lease)&&CommonAudioAuthority().Current(lease));return 0;}
 if(test>=66&&test<=70){assert(core.MuseStopInputChecked(lease)&&core.MuseStopOutputChecked(lease));
  if(test==66)dma_info.is_enabled=true;
  if(test==67)preload_hook=[&]{dma_info.is_enabled=true;};
  if(test==68)partial_preload=1;
  if(test==69)info_error=81;
  if(test==70)info_hook=[&]{assert(ReleaseAudio(lease,true));lease=Take(AudioOwner::Stock,AudioPurpose::StockRuntime,99);};
  assert(core.MuseDiscardOutputChecked(lease,1)==Codec::MuseDiscard::Failed);
  if(test==70){assert(CommonAudioAuthority().Current(lease));return 0;}
  Blocked(core,lease,config);return 0;
 }
 if(test==25){const auto calls=Calls();assert(!core.MuseColdInitialize(lease,config).Complete()&&Calls()==calls);}
 if(test==52){info_hook=[&]{const auto calls=Calls();assert(!core.MuseStopOutputChecked(lease)&&Calls()==calls);};assert(core.MuseStopOutputChecked(lease));}
 if(test==56||test==57){if(test==56)dma_info.mode=7;else dma_info.total_dma_buf_size=4096;assert(core.MuseQuiesceForRetirementChecked(lease,1).outcome==Outcome::Failed);Blocked(core,lease,config);return 0;}
 if(test==40||test==49){if(test==40)info_error=81;else dma_info.is_enabled=true;auto result=core.MuseQuiesceForRetirementChecked(lease,1);assert(result.outcome==Outcome::Failed&&result.phase==Phase::OutputStop&&result.output_stop_attempts==4&&!result.channel_disable_failure);Blocked(core,lease,config);return 0;}
 if(test==41){partial_preload=1;auto result=core.MuseQuiesceForRetirementChecked(lease,1);assert(result.outcome==Outcome::Failed&&result.phase==Phase::DigitalDiscard&&result.discard_detail.loaded==128);Blocked(core,lease,config);return 0;}
 if(test==42){fail_at=amp_calls+2;assert(core.MuseQuiesceForRetirementChecked(lease,1).outcome==Outcome::Failed);Blocked(core,lease,config);return 0;}
 if(test==43){auto wrong=AudioLease{};const auto calls=Calls();assert(core.MuseQuiesceForRetirementChecked(wrong,1).outcome==Outcome::Unavailable&&Calls()==calls);}
 if(test==44)assert(RevokeAudio(lease));
 if(test==45)info_hook=[&]{const auto calls=Calls();assert(!core.MuseStopOutputChecked(lease));assert(core.MuseQuiesceForRetirementChecked(lease,9).outcome==Outcome::Unavailable&&Calls()==calls);};
 if(test==46){info_hook=[&]{assert(ReleaseAudio(lease,true));lease=Take(AudioOwner::Stock,AudioPurpose::StockRuntime,99);};assert(core.MuseQuiesceForRetirementChecked(lease,1).outcome==Outcome::Failed);assert(CommonAudioAuthority().Current(lease));return 0;}
 if(test==50)assert(core.MuseDiscardOutputChecked(lease,9)==Codec::MuseDiscard::Unavailable);
 if(test==51){auto wrong=AudioLease{};assert(!core.MuseStopInputChecked(wrong)&&!core.MuseStopOutputChecked(wrong));}
 Success(core,lease);
 if(test==47){auto stock=Take(AudioOwner::Stock,AudioPurpose::StockRuntime,100);NoPcm(core,stock);assert(CommonAudioAuthority().Current(stock));assert(ReleaseAudio(stock,true));}
 std::printf("PASS cold production-body case %d; no physical IO or flush claim\n",test);
}
