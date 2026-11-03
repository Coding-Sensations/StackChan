// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace companion::room {
// Display-only. The caller supplies current, verified conversation evidence;
// connection, background work and idle animation are not conversation evidence.
class Presence {
 uint64_t last_=0,fade_started_=0;bool seen_=false,fading_=false;
 uint8_t opacity_=0;
public:
 void Reset(){last_=fade_started_=0;seen_=fading_=false;opacity_=0;}
 uint8_t Update(uint64_t now,bool verified,bool communicating,bool modal){
  if(seen_&&now<last_){Reset();return 0;}
  seen_=true;last_=now;
  if(!verified||modal){fading_=false;opacity_=0;return 0;}
  if(communicating){fading_=false;opacity_=255;return opacity_;}
  if(opacity_&&!fading_){fading_=true;fade_started_=now;}
  if(fading_){const auto elapsed=now-fade_started_;opacity_=elapsed>=400?0:uint8_t(255*(400-elapsed)/400);if(!opacity_)fading_=false;}
  return opacity_;
 }
};
}
