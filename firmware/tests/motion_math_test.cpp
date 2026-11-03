/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stackchan/motion/motion_math.h>

namespace {

using stackchan::motion::MotionAngles;
using stackchan::motion::calculateNormalizedLookAngles;
using stackchan::motion::calculatePointLookAngles;
using stackchan::motion::mapNormalizedValueToAngle;

void expectEqual(int actual, int expected, const char* label)
{
    if (actual != expected) {
        std::cerr << label << ": expected " << expected << ", got " << actual << '\n';
        std::exit(1);
    }
}

void expectAngles(MotionAngles actual, MotionAngles expected, const char* label)
{
    expectEqual(actual.yaw, expected.yaw, label);
    expectEqual(actual.pitch, expected.pitch, label);
    expectEqual(actual.valid, expected.valid, label);
}

void testNormalizedMapping()
{
    expectEqual(mapNormalizedValueToAngle(-1.0f, -1280, 1280), -1280, "normalized min");
    expectEqual(mapNormalizedValueToAngle(0.0f, -1280, 1280), 0, "normalized center");
    expectEqual(mapNormalizedValueToAngle(1.0f, -1280, 1280), 1280, "normalized max");
    expectEqual(mapNormalizedValueToAngle(2.0f, -1280, 1280), 1280, "normalized clamp high");
    expectEqual(mapNormalizedValueToAngle(-2.0f, 30, 870), 30, "normalized clamp low");
}

void testNormalizedLookAngles()
{
    expectAngles(
        calculateNormalizedLookAngles(0.5f, -0.5f, -1280, 1280, 30, 870),
        MotionAngles{640, 240},
        "normalized look"
    );
}

void testPointLookAngles()
{
    expectAngles(calculatePointLookAngles(1.0f, 0.0f, 0.0f), MotionAngles{0, 0}, "point forward");
    expectAngles(calculatePointLookAngles(0.0f, 1.0f, 0.0f), MotionAngles{900, 0}, "point left");
    expectAngles(calculatePointLookAngles(0.0f, -1.0f, 0.0f), MotionAngles{-900, 0}, "point right");

    MotionAngles diagonal = calculatePointLookAngles(1.0f, 1.0f, 1.0f);
    expectEqual(diagonal.yaw, 450, "point diagonal yaw");
    expectEqual(diagonal.pitch, 352, "point diagonal pitch");
    const float large = std::numeric_limits<float>::max();
    expectAngles(calculatePointLookAngles(large, large, large), MotionAngles{450, 352}, "large finite point");
    expectEqual(mapNormalizedValueToAngle(1.0f, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()),
                std::numeric_limits<int>::max(), "full integer range high endpoint");
}

void testInvalidAndBoundaryInputs()
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (float invalid : {nan, inf, -inf}) {
        expectEqual(mapNormalizedValueToAngle(invalid, -1280, 1280), 0, "invalid scalar");
        expectAngles(calculateNormalizedLookAngles(invalid, 0, -1280, 1280, 30, 870), {0,0,false}, "invalid x");
        expectAngles(calculateNormalizedLookAngles(0, invalid, -1280, 1280, 30, 870), {0,0,false}, "invalid y");
        expectAngles(calculatePointLookAngles(invalid, 1, 1), {0,0,false}, "invalid point x");
        expectAngles(calculatePointLookAngles(1, invalid, 1), {0,0,false}, "invalid point y");
        expectAngles(calculatePointLookAngles(1, 1, invalid), {0,0,false}, "invalid point z");
    }
    expectAngles(calculatePointLookAngles(0,0,0), {0,0,false}, "zero direction");
    expectAngles(calculateNormalizedLookAngles(0,0,10,-10,30,870), {0,0,false}, "reversed yaw limits");
    expectAngles(calculateNormalizedLookAngles(0,0,-10,10,870,30), {0,0,false}, "reversed pitch limits");
    expectAngles(calculateNormalizedLookAngles(1,-1,70,70,30,30), {70,30}, "fixed axes");
    expectAngles(calculatePointLookAngles(0,0,1), {0,900}, "vertical up");
    expectAngles(calculatePointLookAngles(0,0,-1), {0,-900}, "vertical down");
    const float tiny = std::numeric_limits<float>::denorm_min();
    expectAngles(calculatePointLookAngles(tiny,tiny,tiny), {450,352}, "tiny finite point");
    expectEqual(mapNormalizedValueToAngle(-1, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()),
                std::numeric_limits<int>::min(), "full integer range low endpoint");
}

}  // namespace

int main()
{
    testNormalizedMapping();
    testNormalizedLookAngles();
    testPointLookAngles();
    testInvalidAndBoundaryInputs();
    return 0;
}
