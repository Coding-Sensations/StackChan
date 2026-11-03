#pragma once
#include "imu_expression_model.hpp"
namespace imu_expression {
void PublishOwnerReading(const Reading& reading);
void PublishOwnerUnavailable(Availability status);
Report ReadCached(uint64_t now);
}
