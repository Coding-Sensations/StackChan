/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "stackchan_camera.h"
#include "companion/board_camera_session.hpp"
#include <cstdint>
#include <lvgl.h>
#include <driver/i2c_master.h>
#include <string_view>

namespace hal_bridge {

struct TouchPoint_t {
    int num = 0;
    int x   = -1;
    int y   = -1;
};

struct Data_t {
    TouchPoint_t touchPoint;
    bool isXiaozhiMode              = false;
    bool isXiaozhiModeToggleEnabled = false;
};

struct XiaozhiConfig_t {
    uint32_t idleShutdownTimeSeconds = 600;
    bool allowShutdownWhenCharging   = false;
    uint8_t idleRandomMovementLevel  = 2;
    bool startAiAgentOnBoot          = false;
};

void lock();
void unlock();
Data_t& get_data();

void set_touch_point(int num, int x, int y);
TouchPoint_t get_touch_point();

bool is_xiaozhi_mode();
void set_xiaozhi_mode(bool mode);
void toggle_xiaozhi_chat_state();

void disply_lvgl_lock();
void disply_lvgl_unlock();
lv_disp_t* display_get_lvgl_display();

void xiaozhi_board_init();
void start_xiaozhi_app();
bool is_xiaozhi_ready();
bool is_xiaozhi_idle();
// In the companion build, startAiAgentOnBoot is validated against the explicit
// custom-firmware stock_optin setting; a legacy boot_ai value alone is ignored.
XiaozhiConfig_t get_xiaozhi_config();
void set_xiaozhi_config(const XiaozhiConfig_t& config);

i2c_master_bus_handle_t board_get_i2c_bus();
StackChanCamera* board_get_camera();
// These are local camera leases; they grant no upload/transport consent.
companion::desk::CameraReadiness board_camera_readiness();
companion::desk::CameraSessionResult board_camera_acquire(uint64_t media_epoch,bool local_opt_in,
                                                        companion::desk::CameraBinding& lease);
companion::desk::CameraSessionResult board_camera_start(companion::desk::CameraBinding lease);
companion::desk::CameraSessionResult board_camera_stop(companion::desk::CameraBinding lease);
companion::desk::BoardCameraStopReport board_camera_stop_checked(companion::desk::CameraBinding lease);
bool board_camera_hold_restart(companion::desk::CameraBinding lease);
bool board_camera_release_restart(const companion::desk::BoardCameraStopReport& report);
void board_camera_configure_timing(std::shared_ptr<companion::desk::QualifiedCaptureTimingAuthority> qualified);
uint32_t board_camera_driver_revision();
uint64_t board_camera_authority_id(companion::desk::CameraBinding lease);
bool board_camera_current(companion::desk::CameraBinding lease);
void board_camera_revoke(companion::desk::CameraBinding lease);
bool board_camera_capture(companion::desk::CameraBinding lease);
bool board_camera_copy_bgra(companion::desk::CameraBinding lease,uint8_t* out,size_t capacity,uint64_t& sequence,uint64_t& captured_ns);
bool board_camera_copy_lv01(companion::desk::CameraBinding lease,uint8_t* out,size_t capacity,size_t& written);

int board_get_battery_level();
bool board_is_battery_charging();
void board_set_backlight_brightness(uint8_t brightness, bool permanent = false);
uint8_t board_get_backlight_brightness();
void board_set_speaker_volume(uint8_t volume, bool permanent = false);
uint8_t board_get_speaker_volume();

void app_play_sound(const std::string_view& sound);

}  // namespace hal_bridge
