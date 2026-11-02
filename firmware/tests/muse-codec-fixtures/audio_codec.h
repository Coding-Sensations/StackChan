#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
using gpio_num_t=int;using i2s_chan_handle_t=void*;using portMUX_TYPE=int;
struct i2s_event_data_t{void* dma_buf;size_t size;};
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define portENTER_CRITICAL_ISR(p) ((void)(p))
#define portEXIT_CRITICAL_ISR(p) ((void)(p))
class AudioCodec {
protected:
 bool duplex_=true,input_reference_=false,input_enabled_=false,output_enabled_=false;
 int input_sample_rate_=24000,output_sample_rate_=24000,input_channels_=2,output_channels_=1,output_volume_=10,input_gain_=60;
 i2s_chan_handle_t tx_handle_=reinterpret_cast<void*>(2),rx_handle_=reinterpret_cast<void*>(1);
 virtual int Read(int16_t*,int)=0;virtual int Write(const int16_t*,int)=0;
public:
 int input_sample_rate()const{return input_sample_rate_;}int output_sample_rate()const{return output_sample_rate_;}int input_channels()const{return input_channels_;}int output_channels()const{return output_channels_;}
 virtual ~AudioCodec()=default;
 virtual void SetOutputVolume(int v){output_volume_=v;}
 virtual void EnableInput(bool b){input_enabled_=b;}virtual void EnableOutput(bool b){output_enabled_=b;}
 virtual bool EnableInputChecked(bool b){EnableInput(b);return input_enabled_==b;}
 virtual bool EnableOutputChecked(bool b){EnableOutput(b);return output_enabled_==b;}
 bool InputDataChecked(std::vector<int16_t>&v){return Read(v.data(),int(v.size()))==int(v.size());}
 bool OutputDataChecked(std::vector<int16_t>&v){return Write(v.data(),int(v.size()))==int(v.size());}
 enum class DirectUsbDrain{Unavailable,Pending,Sent,Failed};
 virtual bool DirectUsbCodecAcquire(uint64_t){return false;}
 virtual bool DirectUsbEnableInputChecked(uint64_t,bool){return false;}
 virtual bool DirectUsbEnableOutputChecked(uint64_t,bool){return false;}
 virtual bool DirectUsbInputDataChecked(uint64_t,std::vector<int16_t>&){return false;}
 virtual bool DirectUsbOutputDataChecked(uint64_t,std::vector<int16_t>&){return false;}
 virtual bool DirectUsbDrainBegin(uint64_t){return false;}
 virtual DirectUsbDrain DirectUsbDrainState(uint64_t){return DirectUsbDrain::Unavailable;}
 virtual bool DirectUsbStopInputChecked(uint64_t){return false;}
 virtual bool DirectUsbStopOutputChecked(uint64_t){return false;}
 virtual bool DirectUsbCodecRelease(uint64_t){return false;}
};
