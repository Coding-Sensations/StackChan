/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "motion_math.h"
#include <algorithm>
#include <cmath>

namespace stackchan::motion {

namespace {

constexpr double kRadiansToDegrees = 180.0 / 3.14159265358979323846;

float clampNormalized(float value)
{
    return std::clamp(value, -1.0f, 1.0f);
}

double mapRange(double value, double inMin, double inMax, double outMin, double outMax)
{
    if (inMax == inMin) {
        return outMin;
    }
    double ratio = (value - inMin) / (inMax - inMin);
    return outMin + ratio * (outMax - outMin);
}

int radiansToTenthsOfDegrees(double radians)
{
    return static_cast<int>(radians * kRadiansToDegrees * 10.0f);
}

}  // namespace

int mapNormalizedValueToAngle(float value, int minimum, int maximum)
{
    if (!std::isfinite(value) || minimum > maximum) return 0;
    // Double preserves every int endpoint; float can round INT_MAX above the
    // representable integer range before conversion.
    return static_cast<int>(mapRange(clampNormalized(value), -1.0, 1.0, minimum, maximum));
}

MotionAngles calculateNormalizedLookAngles(float x, float y, int yawMin, int yawMax, int pitchMin, int pitchMax)
{
    if (!std::isfinite(x) || !std::isfinite(y) || yawMin > yawMax || pitchMin > pitchMax) {
        return {0, 0, false};
    }
    return {
        .yaw = mapNormalizedValueToAngle(x, yawMin, yawMax),
        .pitch = mapNormalizedValueToAngle(y, pitchMin, pitchMax),
    };
}

MotionAngles calculatePointLookAngles(float x, float y, float z)
{
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || (x == 0 && y == 0 && z == 0)) {
        return {0, 0, false};
    }
    const double yawRadians = std::atan2(double(y), double(x));
    const double groundDistance = std::hypot(double(x), double(y));
    const double pitchRadians = std::atan2(double(z), groundDistance);

    return {
        .yaw = radiansToTenthsOfDegrees(yawRadians),
        .pitch = radiansToTenthsOfDegrees(pitchRadians),
    };
}

}  // namespace stackchan::motion
