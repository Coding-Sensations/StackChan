#include "sdk_boundary.hpp"
#include <companion/audio_owner.hpp>
#include <stdexcept>
#include <cstdio>
using companion::diagnostic::FaultCode;
struct esp_codec_dev_sample_info_t{int bits_per_sample,channel,channel_mask;uint32_t sample_rate;int mclk_multiple;};
#define ESP_CODEC_DEV_MAKE_CHANNEL_MASK(x) (1<<(x))
static int open_result=0,close_result=0,gain_result=0,volume_result=0,read_result=0,write_result=0;
static FaultCode expected=FaultCode::None;
static bool fatal_seen=false;
static int esp_codec_dev_open(void*,esp_codec_dev_sample_info_t*){return open_result;}
static int esp_codec_dev_close(void*){return close_result;}
static int esp_codec_dev_set_in_channel_gain(void*,int,float){return gain_result;}
static int esp_codec_dev_set_out_vol(void*,int){return volume_result;}
static int esp_codec_dev_read(void*,void*,size_t){return read_result;}
static int esp_codec_dev_write(void*,void*,size_t){return write_result;}
static void fatal_check(int status){if(status){fatal_seen=true;if(companion::CurrentAudioOwner()==companion::AudioOwner::Mira)assert(codec_fixture::fault_recorded&&codec_fixture::fault_code==expected&&codec_fixture::fault_detail==status);throw std::runtime_error("SDK fatal check");}}
#define ESP_ERROR_CHECK(x) fatal_check(x)
#define ESP_ERROR_CHECK_WITHOUT_ABORT(x) static_cast<void>(x)
ACTUAL_HELPERS
class CoreS3AudioCodec:public AudioCodec {
public:
 bool &input_enabled_=input_,&output_enabled_=output_;bool input_reference_=false;
 int output_sample_rate_=24000,input_gain_=60,output_volume_=10;
 void* input_dev_=reinterpret_cast<void*>(1);void* output_dev_=reinterpret_cast<void*>(2);
 void EnableInput(bool)override;void EnableOutput(bool)override;
 int Read(int16_t*,int);int Write(const int16_t*,int);
 bool InputData(std::vector<int16_t>&)override{return false;}void OutputData(std::vector<int16_t>&)override{}
};
ACTUAL_METHODS
static void reset(FaultCode code){open_result=close_result=gain_result=volume_result=read_result=write_result=0;expected=code;fatal_seen=false;codec_fixture::fault_recorded=false;codec_fixture::fault_calls=0;codec_fixture::samples=0;}
int main(){
 using companion::AudioOwner;assert(companion::AcquireAudio(AudioOwner::Mira));
 const FaultCode modes[]={FaultCode::DriverInputOpen,FaultCode::DriverInputGain,FaultCode::DriverInputClose,FaultCode::DriverOutputOpen,FaultCode::DriverOutputVolume,FaultCode::DriverOutputClose,FaultCode::DriverRead,FaultCode::DriverWrite};
 int16_t pcm[8]={};
 for(auto code:modes){
  reset(code);CoreS3AudioCodec codec;int status=-100-static_cast<int>(code);bool fatal=code!=FaultCode::DriverRead&&code!=FaultCode::DriverWrite;
  try{
   if(code==FaultCode::DriverInputOpen){open_result=status;codec.EnableInput(true);}
   if(code==FaultCode::DriverInputGain){gain_result=status;codec.EnableInput(true);}
   if(code==FaultCode::DriverInputClose){codec.EnableInput(true);close_result=status;codec.EnableInput(false);}
   if(code==FaultCode::DriverOutputOpen){open_result=status;codec.EnableOutput(true);}
   if(code==FaultCode::DriverOutputVolume){volume_result=status;codec.EnableOutput(true);}
   if(code==FaultCode::DriverOutputClose){codec.EnableOutput(true);close_result=status;codec.EnableOutput(false);}
   if(code==FaultCode::DriverRead){codec.EnableInput(true);read_result=status;assert(codec.Read(pcm,8)==8);}
   if(code==FaultCode::DriverWrite){codec.EnableOutput(true);write_result=status;assert(codec.Write(pcm,8)==8);}
  }catch(const std::runtime_error&){assert(fatal);}
  assert(fatal_seen==fatal&&codec_fixture::fault_recorded&&codec_fixture::fault_code==code&&codec_fixture::fault_detail==status);
  assert(codec_fixture::fault_task==companion::diagnostic::Task::Codec&&codec_fixture::samples==1);
 }
 // Successful SDK calls do not manufacture faults; stock owner never publishes.
 reset(FaultCode::None);CoreS3AudioCodec success;success.EnableOutput(true);success.EnableOutput(false);assert(!codec_fixture::fault_recorded);
 companion::ReleaseAudio(AudioOwner::Mira);assert(companion::AcquireAudio(AudioOwner::Stock));reset(FaultCode::DriverOutputOpen);CoreS3AudioCodec stock;open_result=-1;try{stock.EnableOutput(true);assert(false);}catch(const std::runtime_error&){}
 assert(fatal_seen&&!codec_fixture::fault_recorded&&codec_fixture::samples==0);companion::ReleaseAudio(AudioOwner::Stock);
 std::puts("CoreS3 eight fault boundaries record before checks; Mira-only; SDK return behavior unchanged");
}
