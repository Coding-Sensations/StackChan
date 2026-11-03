#include "imu_expression_adapter.h"
#include <hal/hal.h>
#include <hal/imu_expression_cache.h>
namespace proximity_expression {
bool StartImuExpressionOwner(){return GetHAL().prepareImuExpression();}
ImuSample ReadImuExpressionSample(uint64_t now){const auto r=imu_expression::ReadCached(now);ImuEvent e=ImuEvent::None;switch(r.cue){case imu_expression::Cue::Tilt:e=ImuEvent::Tilt;break;case imu_expression::Cue::PickupLike:e=ImuEvent::PickedUp;break;case imu_expression::Cue::Still:e=ImuEvent::Still;break;default:break;}return {r.valid,r.observed_ms,e};}
}
