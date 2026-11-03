#pragma once
#include "usb_pcm_actor.h"
#include <ctime>
#include <esp_timer.h>
// Public selection only. Future exact bundle/image may bind these three values;
// neither these values nor menu Connect issue pairing/audio authority.
#ifndef STACKCHAN_USB_PCM_PUBLIC_EPOCH
#define STACKCHAN_USB_PCM_PUBLIC_EPOCH 0
#endif
#ifndef STACKCHAN_USB_PCM_PUBLIC_STREAM
#define STACKCHAN_USB_PCM_PUBLIC_STREAM 0
#endif
#ifndef STACKCHAN_USB_PCM_PUBLIC_UNTIL
#define STACKCHAN_USB_PCM_PUBLIC_UNTIL 0
#endif
namespace direct_usb_pcm {
inline bool StageCompiledSelection(){
 const uint64_t epoch=STACKCHAN_USB_PCM_PUBLIC_EPOCH,stream=STACKCHAN_USB_PCM_PUBLIC_STREAM,until=STACKCHAN_USB_PCM_PUBLIC_UNTIL;
 if(!epoch||epoch>UINT32_MAX||!stream||stream>65535||!until)return false;
 const auto wall=std::time(nullptr);if(wall<1735689600){ReportSetupIssue(SetupIssue::ClockNotReady);return false;}
 if(until<=uint64_t(wall)){ReportSetupIssue(SetupIssue::TrustUnavailable);return false;}
 const auto remaining=until-uint64_t(wall),now=uint64_t(esp_timer_get_time()/1000);
 if(remaining>(UINT64_MAX-now)/1000)return false;
 Selection selected;selected.epoch=uint32_t(epoch);selected.stream=uint16_t(stream);selected.cutoff=now+remaining*1000;return StageSelection(selected);
}
}
