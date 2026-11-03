#pragma once
#include "proximity_gestures.hpp"
namespace proximity_expression {
// Local cached hardware observations only. No identity, grant or permission.
enum class ImuEvent { None, Tilt, TiltLeft, TiltRight, PickedUp, Still };
struct ImuSample {bool valid=false;uint64_t observed_ms=0;ImuEvent event=ImuEvent::None;};
enum class Source { None, Proximity, Imu };
struct Expression {Face face=Face::Neutral;Source source=Source::None;};
class FaceCoordinator {
 uint64_t imu_seen_=0,until_=0;bool seen_=false;Face face_=Face::Neutral;
public:
 void Reset(){*this=FaceCoordinator();}
 Expression Resolve(const Observation& ir,const ImuSample& imu,uint64_t now){
  const bool fresh=imu.valid&&now>=imu.observed_ms&&now-imu.observed_ms<=500;
  if(!fresh||(seen_&&imu.observed_ms<imu_seen_)){face_=Face::Neutral;until_=0;}
  else if(!seen_||imu.observed_ms!=imu_seen_){
   seen_=true;imu_seen_=imu.observed_ms;
   switch(imu.event){case ImuEvent::PickedUp:face_=Face::Happy;until_=now+800;break;
    case ImuEvent::Tilt:case ImuEvent::TiltLeft:case ImuEvent::TiltRight:face_=Face::Curious;until_=now+800;break;
    case ImuEvent::Still:face_=Face::Neutral;until_=0;break;default:break;}
  }
  // Current calibrated reflected-object feedback wins over brief motion cues.
  if(ir.calibrated&&ir.state!=State::NoSignal&&ir.face!=Face::Neutral)return {ir.face,Source::Proximity};
  if(fresh&&face_!=Face::Neutral&&now<until_)return {face_,Source::Imu};
  return {};
 }
};
}
