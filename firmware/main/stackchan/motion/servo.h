/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <smooth_ui_toolkit.hpp>
#include <uitk/short_namespace.hpp>
#include <cstdint>

namespace stackchan::motion {

/**
 * @brief
 *
 */
#ifndef STACKCHAN_BOUNDED_MOTION
#define STACKCHAN_BOUNDED_MOTION 0
#endif
class Servo {
public:
    virtual ~Servo() = default;
#if STACKCHAN_BOUNDED_MOTION
    // Source-only entrypoint; trusted owner supplies exact generation authority.
    using MotionAuthority = bool (*)(void*, uint64_t, uint32_t);
    struct BoundedLimits { float speed_tenths_per_s=250; float acceleration_tenths_per_s2=800; };
    bool beginBoundedMotion(uint64_t generation, uint32_t expiry_ms, MotionAuthority authority, void* context, BoundedLimits limits);
    bool moveBounded(uint64_t generation, float angle_tenths);
    void stopBounded(uint64_t generation);
    bool boundedActive() const {return _bounded_state==1;}
    // Software cancellation cannot confirm a physical stop or drained bus.
    bool boundedPhysicalStopConfirmed() const {return false;}
#endif

    /**
     * @brief
     *
     */
    virtual void init();

    /**
     * @brief
     *
     */
    virtual void update();

    /**
     * @brief Move to angle
     *
     * @param angle
     */
    void move(int angle);

    /**
     * @brief Move to angle with custom spring params
     *
     * @param angle
     * @param stiffness
     * @param damping
     */
    void moveWithSpringParams(int angle, float stiffness = 170.0f, float damping = 26.0f);

    /**
     * @brief Move to angle with speed mapping
     *
     * @param angle
     * @param speed (0-1000)
     */
    void moveWithSpeed(int angle, int speed);

    /**
     * @brief Rotate servo with given velocity
     *
     * @param velocity (-1000, 1000)
     */
    virtual void rotate(int velocity)
    {
    }

    /**
     * @brief Get servo current angle
     *
     * @return int
     */
    virtual int getCurrentAngle();

    /**
     * @brief
     *
     * @return uitk::Vector2i
     */
    virtual uitk::Vector2i getAngleLimit() const
    {
        return _angle_limit;
    }

    /**
     * @brief
     *
     * @return true
     * @return false
     */
    bool isMoving();

    /**
     * @brief
     *
     * @param enabled
     */
    virtual void setTorqueEnabled(bool enabled)
    {
    }
    virtual bool getTorqueEnabled()
    {
        return false;
    }

    /**
     * @brief Auto release torque on rest
     *
     * @param enabled
     */
    void setAutoTorqueReleaseEnabled(bool enabled)
    {
        _auto_torque_release_enabled = enabled;
    }

    /**
     * @brief Enables or disables automatic synchronization of the animation start point
     *        with the current physical angle.
     *
     * @param enabled
     *        - If true: Prevents sudden "jumps" when the servo is moved manually by
     *          external forces, but may cause stuttering during high-frequency updates
     *          as it resets the animation's velocity.
     *        - If false: Maintains animation momentum and velocity for smooth,
     *          continuous motion, but may cause a "snap" if the actual angle differs
     *          significantly from the internal state.
     */
    void setAutoAngleSyncEnabled(bool enabled)
    {
        _auto_angle_sync_enabled = enabled;
    }

    /**
     * @brief
     *
     */
    virtual void setCurrentAngleAsZero()
    {
    }

    /**
     * @brief
     *
     */
    virtual void resetZeroCalibration()
    {
    }

protected:
    bool bounded_output_current();
    void bounded_output_fault();
    bool bounded_legacy_available() const {
#if STACKCHAN_BOUNDED_MOTION
        return _bounded_state==0;
#else
        return true;
#endif
    }
#if STACKCHAN_BOUNDED_MOTION
    // Each concrete driver must prove its actual position mode read-only.
    virtual bool bounded_position_mode_ready() { return false; }
    void retire_bounded();
    void update_bounded();
    uint8_t _bounded_state=0; // 0 legacy, 1 active, 2 retired; no automatic fallback
    uint64_t _bounded_generation=0;
    uint32_t _bounded_expiry=0,_bounded_clock=0,_bounded_tick=0;
    MotionAuthority _bounded_authority=nullptr;
    void* _bounded_context=nullptr;
    BoundedLimits _bounded_limits;
    float _bounded_position=0,_bounded_velocity=0,_bounded_target=0;
#endif
    Servo()
    {
    }

    void set_angle_limit(uitk::Vector2i angleLimit)
    {
        _angle_limit = angleLimit;
    }

    /**
     * @brief Servo set angle implementation
     *
     * @param angle
     */
    virtual void set_angle_impl(int angle) = 0;
    virtual bool is_moving_impl()
    {
        return false;
    }

protected:
    uitk::Vector2i _angle_limit;
    uitk::AnimateValue _angle_anim;

    uint32_t _last_tick               = 0;
    uint32_t _last_torque_check_tick  = 0;
    bool _snap_to_target_on_rest      = false;
    bool _auto_torque_release_enabled = true;
    bool _auto_angle_sync_enabled     = true;

    void apply_default_spring_options();
    void update_angle_anim_target(int angle);
    void stop_motion_at_angle(int angle);
    uitk::SpringOptions_t map_speed_to_spring_options(int speed);
};

}  // namespace stackchan::motion
