#pragma once
#include <apps/network_sensor/expression_inputs.hpp>
namespace proximity_expression {
bool StartImuExpressionOwner();
ImuSample ReadImuExpressionSample(uint64_t now);
}
