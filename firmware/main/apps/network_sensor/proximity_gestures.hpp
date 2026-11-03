#pragma once
#include <algorithm>
#include <cstdint>
namespace proximity_expression {
enum class State { Uncalibrated, Calibrating, Clear, Present, NoSignal };
enum class Event { None, Approach, Retreat, Dwell, Pass };
enum class Face { Neutral, Curious, Happy, Sad };
struct Observation {
 State state=State::Uncalibrated; Event event=Event::None; Face face=Face::Neutral;
 uint16_t raw=0,filtered=0,baseline=0,enter=24,leave=12;
 unsigned calibration_samples=0; bool calibrated=false,unstable=false;
 int delta=0; uint64_t event_number=0;
};
// Single foreground owner; raw reflected-IR counts, never calibrated distance.
class Gestures {
 Observation view_;
 uint32_t sum_=0;uint16_t minimum_=2047,maximum_=0;
 uint64_t observed_=0,entered_=0,face_until_=0,calibration_begin_=0;bool seen_=false,filter_=false,dwelled_=false,calibrating_=false;
 unsigned high_=0,low_=0;
 void Emit(Event event,Face face,uint64_t now){view_.event=event;view_.face=face;++view_.event_number;face_until_=now+1200;}
public:
 static constexpr unsigned BaselineSamples=16;
 void Reset(){*this=Gestures();}
 void Calibrate(uint64_t now=0){Reset();calibrating_=true;calibration_begin_=now;view_.state=State::Calibrating;}
 const Observation& View()const{return view_;}
 void NoSignal(){view_.state=State::NoSignal;view_.event=Event::None;view_.face=Face::Neutral;filter_=false;high_=low_=0;dwelled_=false;entered_=face_until_=0;
  // Keep the explicit calibration request, but never combine samples over gaps.
  if(!view_.calibrated){sum_=0;minimum_=2047;maximum_=0;view_.calibration_samples=0;}
 }
 void Tick(uint64_t now){if(seen_&&(now<observed_||now-observed_>500))NoSignal();else if(view_.state==State::Clear&&face_until_&&now>=face_until_)view_.face=Face::Neutral;}
 bool Feed(uint16_t raw,bool valid,bool saturated,uint64_t sampled,uint64_t now){
  if(!valid||saturated||raw>2047||now<sampled||now-sampled>500||(seen_&&sampled<observed_)){NoSignal();return false;}
  if(calibrating_&&sampled<calibration_begin_)return false;
  if(seen_&&sampled==observed_){Tick(now);return false;}
  seen_=true;observed_=sampled;view_.raw=raw;view_.event=Event::None;
  view_.filtered=filter_?uint16_t((3u*view_.filtered+raw)/4u):raw;filter_=true;
  if(calibrating_){
   view_.state=State::Calibrating;
   sum_+=raw;minimum_=std::min(minimum_,raw);maximum_=std::max(maximum_,raw);++view_.calibration_samples;
   if(view_.calibration_samples==BaselineSamples){
    calibrating_=false;const unsigned spread=maximum_-minimum_;const unsigned enter=std::max(24u,spread*4u+16u);
    const unsigned baseline=(sum_+BaselineSamples/2)/BaselineSamples;
    if(spread>24||baseline+enter>2047){view_.unstable=true;view_.state=State::Uncalibrated;view_.face=Face::Neutral;}
    else {view_.baseline=uint16_t(baseline);view_.enter=uint16_t(enter);view_.leave=uint16_t(enter/2);view_.calibrated=true;view_.state=State::Clear;view_.filtered=view_.baseline;}
   }
   return true;
  }
  if(!view_.calibrated){view_.state=State::Uncalibrated;view_.face=Face::Neutral;return true;}
  if(view_.state==State::NoSignal){view_.state=State::Clear;view_.face=Face::Neutral;}
  view_.delta=int(view_.filtered)-int(view_.baseline);
  if(view_.state==State::Clear){
   low_=0;high_=view_.delta>=view_.enter?high_+1:0;
   if(high_>=2){view_.state=State::Present;entered_=sampled;dwelled_=false;high_=0;Emit(Event::Approach,Face::Curious,sampled);}
   else Tick(now);
  }else if(view_.state==State::Present){
   high_=0;low_=view_.delta<=view_.leave?low_+1:0;
   if(low_>=2){view_.state=State::Clear;low_=0;const bool pass=sampled-entered_<=1500;Emit(pass?Event::Pass:Event::Retreat,pass?Face::Happy:Face::Sad,sampled);entered_=0;dwelled_=false;}
   else if(!dwelled_&&sampled-entered_>=1000){dwelled_=true;Emit(Event::Dwell,Face::Happy,sampled);}
  }
  return true;
 }
};
inline const char* EventText(Event e){switch(e){case Event::Approach:return "Approach";case Event::Retreat:return "Back away";case Event::Dwell:return "Held nearby";case Event::Pass:return "Quick pass";default:return "None";}}
inline unsigned RelativeLevel(const Observation& v){
 if(!v.calibrated||v.state==State::NoSignal||v.enter==0)return 0;
 return static_cast<unsigned>(std::clamp(v.delta*100/(int(v.enter)*2),0,100));
}
}
