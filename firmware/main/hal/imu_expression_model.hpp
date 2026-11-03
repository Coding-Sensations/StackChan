#pragma once
#include <cmath>
#include <cstdint>
namespace imu_expression {
struct Vec3{float x=0,y=0,z=0;};
inline bool Finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
inline float Norm(Vec3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
inline Vec3 Sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec3 Blend(Vec3 a,Vec3 b,float f){return {a.x+(b.x-a.x)*f,a.y+(b.y-a.y)*f,a.z+(b.z-a.z)*f};}
enum class Availability{NotStarted,Calibrating,Ready,InitFailed,ReadFailed,TaskUnavailable,BadSample};
enum class Orientation{Unknown,RelativeAligned,RelativeTilted};
enum class Cue{None,Tilt,PickupLike,Still};
struct Reading{Vec3 acceleration_mps2{},gyro_dps{};uint64_t observed_ms=0;};
struct Report{Availability availability=Availability::NotStarted;bool valid=false,reference_ready=false,magnetometer_available=false,heading_available=false,moving=false;uint64_t observed_ms=0;float relative_tilt_degrees=0;Orientation orientation=Orientation::Unknown;Cue cue=Cue::None;};
// No IO, timers or world/heading/absolute pickup inference. Reference is learned
// from an initial stationary window in the device's current placement.
class Model{
 Report out_{};Reading previous_{};Vec3 mean_acc_{},bias_{},smooth_{};unsigned reference_count_=0;
 uint64_t reference_start_=0,still_start_=0,pickup_at_=0;bool have_previous_=false,reference_=false,was_still_=false;float reference_norm_=9.80665f;
public:
 void Unavailable(Availability status){out_={};out_.availability=status;have_previous_=reference_=was_still_=false;reference_count_=0;reference_start_=still_start_=pickup_at_=0;mean_acc_=bias_=smooth_={};}
 bool Push(const Reading& r){
  if(!r.observed_ms||!Finite(r.acceleration_mps2)||!Finite(r.gyro_dps)||Norm(r.acceleration_mps2)>40||Norm(r.gyro_dps)>3500){Unavailable(Availability::BadSample);return false;}
  if(have_previous_&&r.observed_ms<=previous_.observed_ms)return false;
  if(have_previous_&&r.observed_ms-previous_.observed_ms>500)Unavailable(Availability::Calibrating);
  const float magnitude=Norm(r.acceleration_mps2),gyro=Norm(r.gyro_dps);
  const float change=have_previous_?Norm(Sub(r.acceleration_mps2,previous_.acceleration_mps2)):0;
  out_.observed_ms=r.observed_ms;
  if(!reference_){
   out_.availability=Availability::Calibrating;
   if(magnitude>=8.7f&&magnitude<=10.9f&&gyro<=5&&change<=.3f){
    if(!reference_count_)reference_start_=r.observed_ms;
    ++reference_count_;mean_acc_=Blend(mean_acc_,r.acceleration_mps2,1.f/reference_count_);bias_=Blend(bias_,r.gyro_dps,1.f/reference_count_);
    if(reference_count_>=10&&r.observed_ms-reference_start_>=1000){reference_=true;reference_norm_=Norm(mean_acc_);smooth_=r.acceleration_mps2;still_start_=reference_start_;}
   }else{reference_count_=0;reference_start_=0;mean_acc_=bias_={};}
  }
  if(reference_){
   out_.availability=Availability::Ready;out_.valid=out_.reference_ready=true;
   smooth_=Blend(smooth_,r.acceleration_mps2,.25f);
   const float rate=Norm(Sub(r.gyro_dps,bias_)),deviation=std::fabs(magnitude-reference_norm_);
   const bool active=rate>25||deviation>1.8f;
   const bool quiet=rate<5&&deviation<.6f&&change<.3f;
   if(active){if(was_still_&&!out_.moving)pickup_at_=r.observed_ms;out_.moving=true;still_start_=0;was_still_=false;}
   else if(quiet){if(!still_start_)still_start_=r.observed_ms;if(r.observed_ms-still_start_>=1000){out_.moving=false;was_still_=true;}}
   else{still_start_=0;was_still_=false;}
   // Gravity-derived tilt is meaningful only in a modest acceleration window.
   const float sn=Norm(smooth_);
   if(deviation<1.8f&&sn>1){float cosine=(smooth_.x*mean_acc_.x+smooth_.y*mean_acc_.y+smooth_.z*mean_acc_.z)/(sn*reference_norm_);if(cosine>1)cosine=1;if(cosine< -1)cosine=-1;out_.relative_tilt_degrees=std::acos(cosine)*57.2957795f;if(out_.relative_tilt_degrees>=25)out_.orientation=Orientation::RelativeTilted;else if(out_.relative_tilt_degrees<=15)out_.orientation=Orientation::RelativeAligned;}
   else out_.orientation=Orientation::Unknown;
   out_.cue=pickup_at_&&r.observed_ms-pickup_at_<=400?Cue::PickupLike:out_.orientation==Orientation::RelativeTilted?Cue::Tilt:was_still_?Cue::Still:Cue::None;
  }
  previous_=r;have_previous_=true;return true;
 }
 Report Read(uint64_t now)const{auto value=out_;if(!value.valid||now<value.observed_ms||now-value.observed_ms>500){value.valid=false;value.cue=Cue::None;value.orientation=Orientation::Unknown;}else if(value.cue==Cue::PickupLike&&now-pickup_at_>400)value.cue=Cue::None;return value;}
};
}
