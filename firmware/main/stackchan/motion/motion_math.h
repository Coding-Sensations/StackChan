/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

namespace stackchan::motion {

struct MotionAngles {
    int yaw;
    int pitch;
    // Invalid input must not be translated into a new physical target.
    bool valid = true;
};

// Invalid scalar input/range returns zero. Actuator callers must use the
// checked pair functions below and inspect valid before issuing movement.
int mapNormalizedValueToAngle(float value, int minimum, int maximum);
// Nonfinite coordinates or reversed limits return valid=false. Equal limits
// are a valid fixed axis. Finite normalized inputs retain endpoint clamping.
MotionAngles calculateNormalizedLookAngles(float x, float y, int yawMin, int yawMax, int pitchMin, int pitchMax);
// The zero vector has no direction and returns valid=false.
MotionAngles calculatePointLookAngles(float x, float y, float z);

}  // namespace stackchan::motion
