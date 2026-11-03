#pragma once
#include "usb_pcm_worker.h"
#include "usb_records.hpp"
namespace direct_usb_pcm {
// Own adaptation: retained configured input/output with per-turn half duplex.
// Framing matches upstream; this is not an unchanged factory runtime.
class ControlDispatcher {
 Worker& worker_;Identity identity_;std::function<bool(const uint8_t*,size_t)> reply_;
 uint32_t incoming_=0,outgoing_=0;size_t outstanding_=0;
 bool compatible_=false,speaker_=false,ending_=false,mic_=false,failed_=false;
 bool Reply(uint16_t code,uint16_t stream,uint32_t rate,const uint8_t* payload=nullptr,size_t n=0);
 bool Fail();
public:
 ControlDispatcher(Worker& w,const Identity& i,std::function<bool(const uint8_t*,size_t)> r):worker_(w),identity_(i),reply_(std::move(r)){}
 bool Handle(const uint8_t*,size_t);
 bool Tick();
 bool Credit(size_t);
 bool Mic(const Identity&,uint32_t,const uint8_t*,size_t);
 bool MicCapacity()const{return mic_&&!speaker_&&!ending_&&!failed_;}
};
}
