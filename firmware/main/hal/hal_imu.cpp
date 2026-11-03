/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include "board/hal_bridge.h"
#include "drivers/bmi270/bmi270.h"
#include "utils/motion_detector/motion_detector.h"
#include <mooncake_log.h>
#include <memory>
#include <mutex>
#include <esp_timer.h>
#include "imu_expression_cache.h"

static const std::string_view _tag = "HAL-IMU";

static std::unique_ptr<BMI270> _bmi270;
static std::mutex _imu_start_mutex;
static bool _imu_attempted=false,_imu_task_started=false;

static void _imu_task(void* param)
{
    auto motion_detector = std::make_unique<MotionDetector>();
    motion_detector->setShakeThreshold(16.0f);

    while (1) {
        if (_bmi270 && _bmi270->update()) {
            auto& data = _bmi270->getData();
            imu_expression::PublishOwnerReading({{data.accel_x,data.accel_y,data.accel_z},{data.gyro_x,data.gyro_y,data.gyro_z},static_cast<uint64_t>(esp_timer_get_time()/1000)});
            // mclog::debug("IMU Accel: {:.2f}\t{:.2f}\t{:.2f}", data.accel_x, data.accel_y, data.accel_z);

            motion_detector->update(data.accel_x, data.accel_y, data.accel_z);

            if (motion_detector->isShakeDetected()) {
                mclog::tagInfo(_tag, "Shake Detected!");
                GetHAL().onImuMotionEvent.emit(ImuMotionEvent::Shake);
            }
            // if (motion_detector->isPickUpDetected()) {
            //     mclog::tagInfo(_tag, "Pick Up Detected!");
            //     GetHAL().onImuMotionEvent.emit(ImuMotionEvent::PickUp);
            // }
        }
        else imu_expression::PublishOwnerUnavailable(imu_expression::Availability::ReadFailed);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void Hal::imu_init()
{
    std::lock_guard<std::mutex> owner_lock(_imu_start_mutex);
    if(_imu_attempted)return;
    _imu_attempted=true;
    imu_expression::PublishOwnerUnavailable(imu_expression::Availability::Calibrating);
    mclog::tagInfo(_tag, "init");

    auto i2c_bus = hal_bridge::board_get_i2c_bus();

    if(!i2c_bus){imu_expression::PublishOwnerUnavailable(imu_expression::Availability::InitFailed);return;}
    _bmi270 = std::make_unique<BMI270>(i2c_bus, 0x69);
    if (!_bmi270->begin()) {
        imu_expression::PublishOwnerUnavailable(imu_expression::Availability::InitFailed);
        // Boot owner retains the attached handle; no guessed failed-detach closure.
        mclog::tagError(_tag, "BMI270 init failed");
        return;
    }
    mclog::tagInfo(_tag, "BMI270 init ok");

    // xTaskCreateWithCaps(_imu_task, "imu", 4096, NULL, 2, NULL, MALLOC_CAP_SPIRAM);
    _imu_task_started=xTaskCreatePinnedToCoreWithCaps(_imu_task, "imu", 4096, NULL, 2, NULL, 1, MALLOC_CAP_SPIRAM)==pdPASS;
    if(!_imu_task_started)imu_expression::PublishOwnerUnavailable(imu_expression::Availability::TaskUnavailable);
}

bool Hal::prepareImuExpression(){imu_init();std::lock_guard<std::mutex> lock(_imu_start_mutex);return _imu_task_started;}
