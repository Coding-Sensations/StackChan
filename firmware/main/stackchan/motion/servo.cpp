/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "servo.h"
#include <hal/hal.h>
#include <cmath>
#include <algorithm>

using namespace uitk;

namespace stackchan::motion {

static SpringOptions_t _default_spring_options = {
    .stiffness = 170.0,
    .damping   = 26.0,

    .mass     = 1.0,
    .velocity = 0.0,

    .restSpeed = 0.1,
    .restDelta = 0.1,

    .duration       = 0.0,
    .bounce         = 0.0,
    .visualDuration = 0.0,
};

void Servo::init()
{
    apply_default_spring_options();

    _angle_anim.teleport(getCurrentAngle());
    update();

    setTorqueEnabled(false);
}

void Servo::update()
{
#if STACKCHAN_BOUNDED_MOTION
    if (_bounded_state) {update_bounded();return;}
#endif
    // Keep update in at most 50Hz
    if (GetHAL().millis() - _last_tick < 20) {
        return;
    }
    _last_tick = GetHAL().millis();

    // Apply animation
    if (!_angle_anim.done()) {
        _angle_anim.updateWithDelta(0.02f);  // Fixed delta time for consistency
        set_angle_impl(static_cast<int>(_angle_anim.directValue()));
    }

    // Snap to target angle when animation ends
    else if (_snap_to_target_on_rest) {
        _snap_to_target_on_rest = false;
        set_angle_impl(_angle_anim.end);
    }

    // Auto release torque on rest
    else if (_auto_torque_release_enabled && !isMoving()) {
        if (GetHAL().millis() - _last_torque_check_tick > 200) {
            if (getTorqueEnabled()) {
                setTorqueEnabled(false);
            }
            _last_torque_check_tick = GetHAL().millis();
        }
    }
}

void Servo::move(int angle)
{
#if STACKCHAN_BOUNDED_MOTION
    if (_bounded_state) {retire_bounded();return;}
#endif
    apply_default_spring_options();
    update_angle_anim_target(angle);
}

void Servo::moveWithSpringParams(int angle, float stiffness, float damping)
{
#if STACKCHAN_BOUNDED_MOTION
    if (_bounded_state) {retire_bounded();return;}
#endif
    _angle_anim.springOptions().visualDuration = 0.0f;  // Disable timing override
    _angle_anim.springOptions().stiffness      = stiffness;
    _angle_anim.springOptions().damping        = damping;

    update_angle_anim_target(angle);
}

void Servo::moveWithSpeed(int angle, int speed)
{
#if STACKCHAN_BOUNDED_MOTION
    if (_bounded_state) {retire_bounded();return;}
#endif
    auto spring_options = map_speed_to_spring_options(speed);
    moveWithSpringParams(angle, spring_options.stiffness, spring_options.damping);
}

int Servo::getCurrentAngle()
{
    return _angle_anim.directValue();
}

bool Servo::isMoving()
{
#if STACKCHAN_BOUNDED_MOTION
    if (_bounded_state) return _bounded_state==1 && (std::fabs(_bounded_velocity)>0.1f || std::fabs(_bounded_target-_bounded_position)>0.1f);
#endif
    return _angle_anim.done() == false || is_moving_impl();
}

void Servo::apply_default_spring_options()
{
    auto& options          = _angle_anim.springOptions();
    options.visualDuration = 0.0f;  // Disable timing override
    options.stiffness      = _default_spring_options.stiffness;
    options.damping        = _default_spring_options.damping;
}

void Servo::update_angle_anim_target(int angle)
{
    angle = uitk::clamp(angle, _angle_limit.x, _angle_limit.y);

    if (_auto_angle_sync_enabled) {
        _angle_anim.teleport(getCurrentAngle());  // Use current angle as start
    }
    _angle_anim             = angle;  // Apply new target
    _snap_to_target_on_rest = true;
}

void Servo::stop_motion_at_angle(int angle)
{
#if STACKCHAN_BOUNDED_MOTION
    if (_bounded_state==1) {_bounded_position=_bounded_target=uitk::clamp(angle,_angle_limit.x,_angle_limit.y);_bounded_velocity=0;}
#endif
    angle = uitk::clamp(angle, _angle_limit.x, _angle_limit.y);
    _angle_anim.teleport(angle);
    _snap_to_target_on_rest = false;
}

uitk::SpringOptions_t Servo::map_speed_to_spring_options(int speed)
{
    speed = uitk::clamp(speed, 0, 1000);

    // 1. 计算 Stiffness (刚度)
    // 使用二次方映射: k = k_min + (speed/1000)^2 * k_range
    // 当 speed=500 时，k 约为 10 + 0.25 * 640 = 170
    float k_min           = 10.0f;
    float k_max           = 650.0f;
    float normalizedSpeed = speed / 1000.0f;
    float stiffness       = k_min + (normalizedSpeed * normalizedSpeed) * (k_max - k_min);

    // 2. 计算 Damping (阻尼)
    // 为了保持临界阻尼(无过冲，最快稳定)，公式为 d = 2 * sqrt(m * k)
    // 如果想要带一点点弹性感(bounce)，可以将系数从 2.0 降到 1.5~1.8
    float mass    = 1.0f;
    float damping = 2.0f * sqrtf(mass * stiffness);

    // 3. 构造选项
    uitk::SpringOptions_t options = _default_spring_options;
    options.stiffness             = stiffness;
    options.damping               = damping;
    options.mass                  = mass;

    // 4. 动态调整静止阈值
    // 高速时阈值大一点可以防止由于离散计算导致的微小抖动
    if (speed > 800) {
        options.restDelta = 0.5f;
        options.restSpeed = 0.5f;
    } else {
        options.restDelta = 0.1f;
        options.restSpeed = 0.1f;
    }

    return options;
}

}  // namespace stackchan::motion

namespace stackchan::motion {
void Servo::bounded_output_fault()
{
#if STACKCHAN_BOUNDED_MOTION
    if (_bounded_state) retire_bounded();
#endif
}
bool Servo::bounded_output_current()
{
#if STACKCHAN_BOUNDED_MOTION
    if (!_bounded_state) return true;
    if (_bounded_state!=1) return false;
    const uint32_t now=GetHAL().millis();
    if (now<_bounded_clock || now-_bounded_clock>100 || now>=_bounded_expiry || !_bounded_authority ||
        !_bounded_authority(_bounded_context,_bounded_generation,now)) {retire_bounded();return false;}
    const uint32_t after=GetHAL().millis();
    if (_bounded_state!=1 || after<now || after-now>100 || after>=_bounded_expiry) {retire_bounded();return false;}
    _bounded_clock=after;
#endif
    return true;
}
#if STACKCHAN_BOUNDED_MOTION
void Servo::retire_bounded()
{
    _bounded_state=2;_bounded_velocity=_bounded_target=_bounded_position=0;
    _bounded_authority=nullptr;_bounded_context=nullptr;
    _angle_anim.teleport(0);_snap_to_target_on_rest=false;
}
bool Servo::beginBoundedMotion(uint64_t generation,uint32_t expiry_ms,MotionAuthority authority,void* context,BoundedLimits limits)
{
    if (_bounded_state) return false;
    // An attempted activation is one-shot: invalid input cannot fall into legacy.
    _bounded_state=1;_bounded_generation=generation;_bounded_expiry=expiry_ms;
    _bounded_clock=_bounded_tick=GetHAL().millis();_bounded_authority=authority;_bounded_context=context;
    if (!generation || !authority || expiry_ms<=_bounded_clock || expiry_ms-_bounded_clock>120000 ||
        !std::isfinite(limits.speed_tenths_per_s) || !std::isfinite(limits.acceleration_tenths_per_s2) ||
        limits.speed_tenths_per_s<=0 || limits.speed_tenths_per_s>250 ||
        limits.acceleration_tenths_per_s2<=0 || limits.acceleration_tenths_per_s2>800 ||
        _angle_limit.x>_angle_limit.y || !bounded_output_current()) {retire_bounded();return false;}
    if (!bounded_position_mode_ready() || !bounded_output_current()) {retire_bounded();return false;}
    _bounded_limits=limits;
    if (!bounded_output_current()) return false;
    const int angle=getCurrentAngle();
    if (!bounded_output_current()) return false;
    _bounded_position=_bounded_target=uitk::clamp(angle,_angle_limit.x,_angle_limit.y);_bounded_velocity=0;
    _angle_anim.teleport(_bounded_position);_snap_to_target_on_rest=false;
    return true;
}
bool Servo::moveBounded(uint64_t generation,float angle_tenths)
{
    if (generation!=_bounded_generation || _bounded_state!=1) return false;
    if (!std::isfinite(angle_tenths) || !bounded_output_current()) {retire_bounded();return false;}
    _bounded_target=std::clamp(angle_tenths,float(_angle_limit.x),float(_angle_limit.y));
    return true;
}
void Servo::stopBounded(uint64_t generation)
{
    if (_bounded_state && generation==_bounded_generation) retire_bounded();
}
void Servo::update_bounded()
{
    if (!bounded_output_current()) return;
    const uint32_t now=GetHAL().millis();
    if (now-_bounded_tick<20) return;
    _bounded_tick=now;
    if (!std::isfinite(_bounded_position) || !std::isfinite(_bounded_velocity) || _angle_limit.x>_angle_limit.y) {retire_bounded();return;}
    // Damped follower technique from CustomFW HeadMotion::Step. All model units
    // here are tenths of a degree, seconds. Retargeting can overshoot a target.
    const float a=std::clamp(16.0f*(_bounded_target-_bounded_position)-8.0f*_bounded_velocity,
                             -_bounded_limits.acceleration_tenths_per_s2,_bounded_limits.acceleration_tenths_per_s2);
    _bounded_velocity=std::clamp(_bounded_velocity+a*0.02f,-_bounded_limits.speed_tenths_per_s,_bounded_limits.speed_tenths_per_s);
    _bounded_position+=_bounded_velocity*0.02f;
    const float clamped=std::clamp(_bounded_position,float(_angle_limit.x),float(_angle_limit.y));
    if (clamped!=_bounded_position) _bounded_velocity=0;
    _bounded_position=clamped;
    if (!bounded_output_current()) return;
    set_angle_impl(static_cast<int>(std::lround(_bounded_position)));
    bounded_output_current(); // Physical completion/stop is deliberately unknown.
}
#endif
}
