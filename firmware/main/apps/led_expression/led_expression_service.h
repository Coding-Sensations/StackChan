#pragma once
#include <apps/network_sensor/expression_inputs.hpp>
#include <cstdint>
namespace led_expression {
// Cached local observation only; never performs a HAL query or LED write.
void PublishExpression(proximity_expression::Expression expression,
                       uint64_t observed_ms);
} // namespace led_expression
