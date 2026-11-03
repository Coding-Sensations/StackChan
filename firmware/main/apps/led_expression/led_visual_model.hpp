#pragma once
#include "led_service_api.h"
#include <string>
namespace led_visual {
struct Rect {int x,y,w,h;};
inline Rect Pixel(unsigned i){return {20+int(i%6)*44,8+int(i/6)*30,24,24};}
inline unsigned PixelColor(const led_expression::CachedPreview& p,unsigned i){
 if(i>=12||!p.has_accepted||p.result!=led_expression::Result::Submitted)return 0x254351;
 auto c=p.accepted[i];return unsigned(c.red)<<16|unsigned(c.green)<<8|unsigned(c.blue);
}
inline std::string Status(const led_expression::CachedPreview& p){
 using R=led_expression::Result;
 if(p.result==R::IoFault)return "Output unknown; explicit Retry";
 if(p.result==R::Unavailable)return "LED hardware unavailable";
 if(!p.has_accepted||p.result!=R::Submitted)return "No accepted frame; cache only";
 return p.source==led_expression::OutputSource::UserOff?"Off accepted; controller cache":"Frame cache; target "+std::to_string(p.brightness)+"% (cache)";
}
}
