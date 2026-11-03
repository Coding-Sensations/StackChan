#include <apps/stock_shell/sd_mount_diagnostics.h>
#include "wifi_board.h"
#include <apps/stock_services/policy.h>
#include <companion/settings_policy.hpp>
#include "cores3_audio_codec.h"
#include "display/lcd_display.h"
#include "stackchan_display.h"
#include "application.h"
#include "config.h"
#include "power_save_timer.h"
#include "i2c_device.h"
#include "axp2101.h"
#include "settings.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <wifi_station.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_ili9341.h>
#include <esp_timer.h>
#include <apps/runtime_lifecycle/session.h>
#include <algorithm>
#include <mutex>
#include "stackchan_camera.h"
#include <companion/audio_owner.hpp>
#include "hal_bridge.h"
#include <hal/hal.h>
#include "apps/stock_shell/stackchan_sd_ir.h"

#define TAG "M5Stack-StackChan-Board"

#define XPOWERS_AXP2101_ICC_CHG_SET (0x62)

namespace {

struct LcdInitCommand {
    uint8_t command;
    uint8_t data[15];
    uint8_t data_size;
    uint16_t delay_ms;
};

constexpr LcdInitCommand kIli9342eInitCommands[] = {
    {0xDD, {0x01}, 1, 0},
    {0x3A, {0x55}, 1, 0},
    {0x21, {}, 0, 0},
    {0x36, {0x08}, 1, 0},
    {0xD5, {0x00}, 1, 0},
    {0xB1, {0x22}, 1, 0},
    {0xC8, {0x38}, 1, 0},
    {0xCB, {0x1C}, 1, 0},
    {0xC9, {0x1A}, 1, 0},
    {0xCA, {0x1A}, 1, 0},
    {0xB7, {0x5A, 0x41, 0x11, 0x19}, 4, 0},
    {0xE4, {0x04, 0x08, 0x11, 0x06, 0x12, 0x07, 0x3A, 0x76, 0x47, 0x07, 0x0F, 0x0A, 0x11, 0x19, 0x05}, 15, 0},
    {0xE5, {0x02, 0x03, 0x07, 0x06, 0x12, 0x07, 0x36, 0x5F, 0x48, 0x06, 0x10, 0x0C, 0x16, 0x14, 0x09}, 15, 0},
    {0x11, {}, 0, 120},
    {0x29, {}, 0, 120},
};

esp_err_t ApplyIli9342eInit(esp_lcd_panel_io_handle_t panel_io)
{
    for (const auto& init_command : kIli9342eInitCommands) {
        const void* data = init_command.data_size == 0 ? nullptr : init_command.data;
        esp_err_t err    = esp_lcd_panel_io_tx_param(panel_io, init_command.command, data, init_command.data_size);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ILI9342E command 0x%02X failed: %s", init_command.command, esp_err_to_name(err));
            return err;
        }
        if (init_command.delay_ms != 0) {
            vTaskDelay(pdMS_TO_TICKS(init_command.delay_ms));
        }
    }
    return ESP_OK;
}

}  // namespace

class Pmic : public Axp2101 {
public:
    /**
     * @brief axp2101 charge currnet voltage parameters.
     */
    typedef enum __xpowers_axp2101_chg_curr {
        XPOWERS_AXP2101_CHG_CUR_0MA,
        XPOWERS_AXP2101_CHG_CUR_100MA = 4,
        XPOWERS_AXP2101_CHG_CUR_125MA,
        XPOWERS_AXP2101_CHG_CUR_150MA,
        XPOWERS_AXP2101_CHG_CUR_175MA,
        XPOWERS_AXP2101_CHG_CUR_200MA,
        XPOWERS_AXP2101_CHG_CUR_300MA,
        XPOWERS_AXP2101_CHG_CUR_400MA,
        XPOWERS_AXP2101_CHG_CUR_500MA,
        XPOWERS_AXP2101_CHG_CUR_600MA,
        XPOWERS_AXP2101_CHG_CUR_700MA,
        XPOWERS_AXP2101_CHG_CUR_800MA,
        XPOWERS_AXP2101_CHG_CUR_900MA,
        XPOWERS_AXP2101_CHG_CUR_1000MA,
    } xpowers_axp2101_chg_curr_t;

    // Power Init
    Pmic(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : Axp2101(i2c_bus, addr)
    {
        uint8_t data = ReadReg(0x90);
        data |= 0b10110100;
        WriteReg(0x90, data);
        // WriteReg(0x99, (0b11110 - 5));
        WriteReg(0x97, (0b11110 - 2));
        WriteReg(0x69, 0b00110101);
        WriteReg(0x30, 0b111111);
        WriteReg(0x90, 0xBF);
        WriteReg(0x94, 33 - 5);
        WriteReg(0x95, 33 - 5);
        WriteReg(0x27, 0x00);

        auto ret = setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_700MA);
        if (!ret) {
            ESP_LOGE(TAG, "Set charge current failed");
        } else {
            ESP_LOGI(TAG, "Set charge current success");
        }

        SetBrightness(0);
    }

    esp_err_t ReadSdControl(uint8_t& value){return TryReadRegs(0x90,&value,1,100);}
    esp_err_t ReadSdVoltage(uint8_t& value){return TryReadRegs(0x95,&value,1,100);}

    void SetBrightness(uint8_t brightness)
    {
        if (brightness == 0) {
            // DLDO1 off
            uint8_t val = ReadReg(0x90);
            WriteReg(0x90, val & 0x7F);
        } else {
            // 映射计算：将 1~100 映射到 寄存器值 20~28
            // 公式：MinReg + (input * (MaxReg - MinReg) / MaxInput)
            // 20 + (brightness * 8 / 100)
            if (brightness > 100) {
                brightness = 100;
            }
            uint8_t reg_val = 20 + ((uint16_t)brightness * 8 / 100);
            WriteReg(0x99, reg_val);

            // Make sure DLDO1 on
            uint8_t val = ReadReg(0x90);
            if (!(val & 0x80)) {
                WriteReg(0x90, val | 0x80);
            }
        }
    }

    /**
     * @brief Set charge current.
     * @param  opt: See xpowers_axp2101_chg_curr_t enum for details.
     * @retval
     */
    bool setChargerConstantCurr(uint8_t opt)
    {
        if (opt > XPOWERS_AXP2101_CHG_CUR_1000MA) {
            return false;
        }
        int val = ReadReg(XPOWERS_AXP2101_ICC_CHG_SET);
        if (val == -1) {
            return false;
        }
        val &= 0xE0;
        WriteReg(XPOWERS_AXP2101_ICC_CHG_SET, val | opt);
        return true;
    }

    esp_err_t TryReadPowerStatus(bool& has_external_power, bool& charging, bool& discharging)
    {
        uint8_t power_status = 0;
        esp_err_t err        = TryReadRegs(0x01, &power_status, 1);
        if (err != ESP_OK) {
            return err;
        }

        const uint8_t current_direction = (power_status & 0b01100000) >> 5;
        if (current_direction == 3) {
            return ESP_ERR_INVALID_RESPONSE;
        }
        const bool is_charging_done = (power_status & 0b00000111) == 0b00000100;

        // Treat any non-discharging state as externally powered so a plugged-in cable
        // still counts even after the battery is full.
        has_external_power = current_direction != 2 || is_charging_done;
        charging           = current_direction == 1;
        discharging        = current_direction == 2;
        return ESP_OK;
    }

    esp_err_t TryReadBatteryLevel(int& level)
    {
        uint8_t battery_level = 0;
        esp_err_t err         = TryReadRegs(0xA4, &battery_level, 1);
        if (err != ESP_OK) {
            return err;
        }
        if (battery_level > 100) {
            return ESP_ERR_INVALID_RESPONSE;
        }

        level = battery_level;
        return ESP_OK;
    }
};

class CustomBacklight : public Backlight {
public:
    CustomBacklight(Pmic* pmic) : pmic_(pmic)
    {
    }

    void SetBrightnessImpl(uint8_t brightness) override
    {
        pmic_->SetBrightness(target_brightness_);
        brightness_ = target_brightness_;
    }

private:
    Pmic* pmic_;
};

class Aw9523 : public I2cDevice {
public:
    // Exanpd IO Init
    Aw9523(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr)
    {
        WriteReg(0x02, 0b00000111);  // P0
        WriteReg(0x03, 0b10001111);  // P1
        WriteReg(0x04, 0b00011000);  // CONFIG_P0
        WriteReg(0x05, 0b00001100);  // CONFIG_P1
        WriteReg(0x11, 0b00010000);  // GCR P0 port is Push-Pull mode.
        WriteReg(0x12, 0b11111111);  // LEDMODE_P0
        WriteReg(0x13, 0b11111111);  // LEDMODE_P1
    }

    esp_err_t ReadSdSwitch(uint8_t& value){return TryReadRegs(0x00,&value,1,100);}

    void ResetAw88298()
    {
        ESP_LOGI(TAG, "Reset AW88298");
        WriteReg(0x02, 0b00000011);
        vTaskDelay(pdMS_TO_TICKS(10));
        WriteReg(0x02, 0b00000111);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    void ResetIli9342()
    {
        ESP_LOGI(TAG, "Reset IlI9342");
        WriteReg(0x03, 0b10000001);
        vTaskDelay(pdMS_TO_TICKS(20));
        WriteReg(0x03, 0b10000011);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
};

class Ft6336 : public I2cDevice {
public:
    struct ControllerVersion {
        uint8_t cipher  = 0;
        uint8_t firm_id = 0;
        uint8_t vendor  = 0;
    };

    struct TouchPoint_t {
        int num = 0;
        int x   = -1;
        int y   = -1;
    };

    Ft6336(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr)
    {
        read_buffer_ = new uint8_t[6];
    }

    ~Ft6336()
    {
        delete[] read_buffer_;
    }

    bool ReadControllerVersion(ControllerVersion& version)
    {
        constexpr int kMaxAttempts  = 5;
        constexpr int kRetryDelayMs = 20;

        version = {};
        for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
            const uint8_t work_mode[] = {0x00, 0x00};
            uint8_t info[6]           = {};
            esp_err_t err             = i2c_master_transmit(i2c_device_, work_mode, sizeof(work_mode), 100);
            if (err == ESP_OK) {
                err = TryReadRegs(0xA3, info, sizeof(info));
            }

            if (err == ESP_OK && info[0] != 0 && info[5] != 0) {
                version.cipher  = info[0];
                version.firm_id = info[3];
                version.vendor  = info[5];
                return true;
            }

            if (attempt + 1 < kMaxAttempts) {
                vTaskDelay(pdMS_TO_TICKS(kRetryDelayMs));
            }
        }
        return false;
    }

    bool UpdateTouchPoint()
    {
        auto err = TryReadRegs(0x02, read_buffer_, 6);
        if (err != ESP_OK) {
            tp_.num = 0;
            tp_.x   = -1;
            tp_.y   = -1;

            consecutive_failures_++;
            int64_t now_us = esp_timer_get_time();
            if (last_error_log_us_ == 0 || (now_us - last_error_log_us_) >= 1000 * 1000) {
                ESP_LOGW(TAG, "FT6336 read failed (%s), skipped %lu sample(s)", esp_err_to_name(err),
                         static_cast<unsigned long>(consecutive_failures_));
                last_error_log_us_ = now_us;
            }
            return false;
        }

        consecutive_failures_ = 0;
        tp_.num               = read_buffer_[0] & 0x0F;
        tp_.x                 = ((read_buffer_[1] & 0x0F) << 8) | read_buffer_[2];
        tp_.y                 = ((read_buffer_[3] & 0x0F) << 8) | read_buffer_[4];
        return true;
    }

    inline const TouchPoint_t& GetTouchPoint()
    {
        return tp_;
    }

private:
    uint8_t* read_buffer_ = nullptr;
    TouchPoint_t tp_;
    int64_t last_error_log_us_     = 0;
    uint32_t consecutive_failures_ = 0;
};

class M5StackCoreS3Board : public WifiBoard {
private:
    static constexpr int kPowerSaveSleepDelaySeconds  = 300;
    static constexpr int kPowerStatePollIntervalMs    = 1000;
    static constexpr int kPowerReadErrorLogIntervalMs = 10000;

    struct PowerState {
        bool has_external_power  = false;
        bool charging            = false;
        bool discharging         = false;
        int battery_level        = 0;
        bool status_valid        = false;
        bool battery_level_valid = false;
    };

    i2c_master_bus_handle_t i2c_bus_;
    Pmic* pmic_;
    Aw9523* aw9523_;
    Ft6336* ft6336_;
    LvglDisplay* display_;
    std::atomic<StackChanCamera*> camera_{nullptr};
    std::mutex camera_object_mutex_;
    std::mutex camera_stop_receipt_mutex_;
    companion::desk::BoardCameraStopReport retained_camera_stop_;
    std::shared_ptr<companion::desk::QualifiedCaptureTimingAuthority> camera_timing_;
    companion::desk::BoardCameraSession camera_session_;
    esp_video_init_dvp_config_t camera_dvp_config_{};
    esp_video_init_config_t camera_video_config_{};

    esp_timer_handle_t touchpad_timer_;
    PowerSaveTimer* power_save_timer_ = nullptr;
    hal_bridge::XiaozhiConfig_t xiaozhi_config_;
    std::mutex power_state_mutex_;
    PowerState power_state_;
    bool last_power_save_enabled_             = false;
    int64_t last_power_state_check_ms_        = 0;
    int64_t last_power_error_log_ms_          = 0;
    uint32_t consecutive_power_read_failures_ = 0;

    bool ShouldEnablePowerSave(bool has_external_power, bool is_discharging) const
    {
        return is_discharging || (has_external_power && xiaozhi_config_.allowShutdownWhenCharging);
    }

    void UpdatePowerSaveEnabled(bool has_external_power, bool is_discharging)
    {
        if (!power_save_timer_) return;
#ifdef STACKCHAN_COMPANION_BOOT
        if (companion::CurrentAudioOwner() != companion::AudioOwner::Stock) return;
#endif
        const bool should_enable_power_save = ShouldEnablePowerSave(has_external_power, is_discharging);
        if (should_enable_power_save == last_power_save_enabled_) {
            return;
        }

        ESP_LOGI(TAG, "Power save timer %s: external_power=%d, discharging=%d, allowShutdownWhenCharging=%d",
                 should_enable_power_save ? "enabled" : "disabled", has_external_power, is_discharging,
                 xiaozhi_config_.allowShutdownWhenCharging);
        power_save_timer_->SetEnabled(should_enable_power_save);
        last_power_save_enabled_ = should_enable_power_save;
    }

    void RefreshPowerState()
    {
        bool has_external_power = false;
        bool charging           = false;
        bool discharging        = false;
        int battery_level       = 0;
        esp_err_t status_err    = pmic_->TryReadPowerStatus(has_external_power, charging, discharging);
        esp_err_t level_err     = pmic_->TryReadBatteryLevel(battery_level);

        {
            std::lock_guard<std::mutex> lock(power_state_mutex_);
            if (status_err == ESP_OK) {
                power_state_.has_external_power = has_external_power;
                power_state_.charging           = charging;
                power_state_.discharging        = discharging;
                power_state_.status_valid       = true;
            }
            if (level_err == ESP_OK) {
                power_state_.battery_level       = battery_level;
                power_state_.battery_level_valid = true;
            }
        }

        if (status_err == ESP_OK) {
            UpdatePowerSaveEnabled(has_external_power, discharging);
        }

        if (status_err != ESP_OK || level_err != ESP_OK) {
            consecutive_power_read_failures_++;
            const int64_t now_ms = esp_timer_get_time() / 1000;
            if (last_power_error_log_ms_ == 0 || (now_ms - last_power_error_log_ms_) >= kPowerReadErrorLogIntervalMs) {
                ESP_LOGW(TAG, "AXP2101 read failed: status=%s, level=%s; keeping previous values (%lu failure(s))",
                         esp_err_to_name(status_err), esp_err_to_name(level_err),
                         static_cast<unsigned long>(consecutive_power_read_failures_));
                last_power_error_log_ms_ = now_ms;
            }
            return;
        }

        if (consecutive_power_read_failures_ != 0) {
            ESP_LOGI(TAG, "AXP2101 reads recovered after %lu failure(s)",
                     static_cast<unsigned long>(consecutive_power_read_failures_));
            consecutive_power_read_failures_ = 0;
            last_power_error_log_ms_         = 0;
        }
    }

    void PollPowerSaveState()
    {
        const int64_t now_ms = esp_timer_get_time() / 1000;
        if (last_power_state_check_ms_ != 0 && (now_ms - last_power_state_check_ms_) < kPowerStatePollIntervalMs) {
            return;
        }
        last_power_state_check_ms_ = now_ms;
        RefreshPowerState();
    }

    void InitializePowerSaveTimer()
    {
        xiaozhi_config_ = hal_bridge::get_xiaozhi_config();

        const int seconds_to_shutdown = xiaozhi_config_.idleShutdownTimeSeconds > 0
                                            ? static_cast<int>(xiaozhi_config_.idleShutdownTimeSeconds)
                                            : -1;
        const int seconds_to_sleep    = seconds_to_shutdown == -1
                                            ? kPowerSaveSleepDelaySeconds
                                            : std::min(kPowerSaveSleepDelaySeconds, seconds_to_shutdown);

        ESP_LOGI(TAG, "Init power save timer: sleep=%d s, shutdown=%d s, allow_shutdown_when_charging=%d",
                 seconds_to_sleep, seconds_to_shutdown, xiaozhi_config_.allowShutdownWhenCharging);

        power_save_timer_ = new PowerSaveTimer(-1, seconds_to_sleep, seconds_to_shutdown);
        power_save_timer_->OnEnterSleepMode([this]() {
#ifdef STACKCHAN_COMPANION_BOOT
            if(companion::CurrentAudioOwner()!=companion::AudioOwner::Stock||runtime_lifecycle::HomeRequested())return;
#endif
            GetDisplay()->SetPowerSaveMode(true);
            // GetBacklight()->SetBrightness(10);
        });
        power_save_timer_->OnExitSleepMode([this]() {
#ifdef STACKCHAN_COMPANION_BOOT
            if(companion::CurrentAudioOwner()!=companion::AudioOwner::Stock||runtime_lifecycle::HomeRequested())return;
#endif
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnShutdownRequest([this]() {
#ifdef STACKCHAN_COMPANION_BOOT
            if(companion::CurrentAudioOwner()!=companion::AudioOwner::Stock||runtime_lifecycle::HomeRequested())return;
#endif
            pmic_->PowerOff();
        });
        RefreshPowerState();
    }

    void InitializeI2c()
    {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port          = (i2c_port_t)1,
            .sda_io_num        = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num        = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source        = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority     = 0,
            .trans_queue_depth = 0,
            .flags =
                {
                    .enable_internal_pullup = 1,
                },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void I2cDetect()
    {
        uint8_t address;
        printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
        for (int i = 0; i < 128; i += 16) {
            printf("%02x: ", i);
            for (int j = 0; j < 16; j++) {
                fflush(stdout);
                address       = i + j;
                esp_err_t ret = i2c_master_probe(i2c_bus_, address, pdMS_TO_TICKS(200));
                if (ret == ESP_OK) {
                    printf("%02x ", address);
                } else if (ret == ESP_ERR_TIMEOUT) {
                    printf("UU ");
                } else {
                    printf("-- ");
                }
            }
            printf("\r\n");
        }
    }

    void InitializeAxp2101()
    {
        ESP_LOGI(TAG, "Init AXP2101");
        pmic_ = new Pmic(i2c_bus_, 0x34);
    }

    void InitializeAw9523()
    {
        ESP_LOGI(TAG, "Init AW9523");
        aw9523_ = new Aw9523(i2c_bus_, 0x58);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    void PollTouchpad()
    {
        if (!ft6336_->UpdateTouchPoint()) {
            return;
        }
        auto& touch_point = ft6336_->GetTouchPoint();

        // Update hal touch point
        hal_bridge::set_touch_point(touch_point.num, touch_point.x, touch_point.y);
    }

    void InitializeFt6336()
    {
        ESP_LOGI(TAG, "Init FT6336");
        ft6336_ = new Ft6336(i2c_bus_, 0x38);
    }

    void StartTouchpadTimer()
    {
        // 创建定时器，20ms 间隔
        esp_timer_create_args_t timer_args = {
            .callback =
                [](void* arg) {
                    M5StackCoreS3Board* board = (M5StackCoreS3Board*)arg;
                    board->PollTouchpad();
#ifdef STACKCHAN_COMPANION_BOOT
                    if(companion::CurrentAudioOwner()==companion::AudioOwner::Stock) {
                        if(!board->power_save_timer_)board->InitializePowerSaveTimer();
                    }else if(board->power_save_timer_){board->power_save_timer_->SetEnabled(false);board->last_power_save_enabled_=false;}
#endif
                    // Battery sampling remains live in Home/Native; enabling
                    // Stock's sleep/shutdown timer is separately owner gated.
                    board->PollPowerSaveState();
                },
            .arg                   = this,
            .dispatch_method       = ESP_TIMER_TASK,
            .name                  = "touchpad_timer",
            .skip_unhandled_events = true,
        };

        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &touchpad_timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(touchpad_timer_, 20 * 1000));
    }

    void InitializeSpi()
    {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num      = GPIO_NUM_37;
        buscfg.miso_io_num      = GPIO_NUM_35; // Factory SD MISO / LCD DC shared pin.
        buscfg.sclk_io_num      = GPIO_NUM_36;
        buscfg.quadwp_io_num    = GPIO_NUM_NC;
        buscfg.quadhd_io_num    = GPIO_NUM_NC;
        buscfg.max_transfer_sz  = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeIli9342Display()
    {
        ESP_LOGI(TAG, "Init IlI9342");

        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel       = nullptr;

        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num                   = GPIO_NUM_3;
        io_config.dc_gpio_num                   = GPIO_NUM_35;
        io_config.spi_mode                      = 2;
        io_config.pclk_hz                       = 40 * 1000 * 1000;
        io_config.trans_queue_depth             = 10;
        io_config.lcd_cmd_bits                  = 8;
        io_config.lcd_param_bits                = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num             = GPIO_NUM_NC;
        panel_config.rgb_ele_order              = LCD_RGB_ELEMENT_ORDER_BGR;
        panel_config.bits_per_pixel             = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        aw9523_->ResetIli9342();

        esp_lcd_panel_init(panel);

        Ft6336::ControllerVersion touch_version;
        const bool touch_version_valid = ft6336_->ReadControllerVersion(touch_version);
        const bool is_ili9342e = touch_version_valid && touch_version.firm_id == 0x12 && touch_version.vendor == 0x11;
        if (touch_version_valid) {
            ESP_LOGI(TAG, "CIPHER:0x%02X / FIRMID:0x%02X / VENDID:0x%02X, panel:%s", touch_version.cipher,
                     touch_version.firm_id, touch_version.vendor, is_ili9342e ? "ILI9342E" : "ILI9342C");
        } else {
            ESP_LOGW(TAG, "FT6336 version read failed, panel:ILI9342C");
        }

        if (is_ili9342e) {
            ESP_LOGI(TAG, "Apply ILI9342E panel initialization");
            ESP_ERROR_CHECK(ApplyIli9342eInit(panel_io));
        }

        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

        // display_ = new StackChanLcdDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X,
        //                                    DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        stock_tools::BindStackChanIrLcd(panel_io);
        display_ = new StackChanAvatarDisplay(panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X,
                                              DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    void InitializeCamera()
    {
        std::lock_guard<std::mutex> lock(camera_object_mutex_);
        if(camera_.load())return;
        ESP_LOGI(TAG, "Init Camera");

        static esp_cam_ctlr_dvp_pin_config_t dvp_pin_config = {
            .data_width = CAM_CTLR_DATA_WIDTH_8,
            .data_io =
                {
                    [0] = CAMERA_PIN_D0,
                    [1] = CAMERA_PIN_D1,
                    [2] = CAMERA_PIN_D2,
                    [3] = CAMERA_PIN_D3,
                    [4] = CAMERA_PIN_D4,
                    [5] = CAMERA_PIN_D5,
                    [6] = CAMERA_PIN_D6,
                    [7] = CAMERA_PIN_D7,
                },
            .vsync_io = CAMERA_PIN_VSYNC,
            .de_io    = CAMERA_PIN_HREF,
            .pclk_io  = CAMERA_PIN_PCLK,
            .xclk_io  = CAMERA_PIN_XCLK,
        };

        esp_video_init_sccb_config_t sccb_config = {
            .init_sccb  = false,
            .i2c_handle = i2c_bus_,
            .freq       = 100000,
        };

        camera_dvp_config_ = {
            .sccb_config = sccb_config,
            .reset_pin   = CAMERA_PIN_RESET,
            .pwdn_pin    = CAMERA_PIN_PWDN,
            .dvp_pin     = dvp_pin_config,
            .xclk_freq   = XCLK_FREQ_HZ,
        };

        camera_video_config_ = {};
        camera_video_config_.dvp = &camera_dvp_config_;

        camera_.store(new StackChanCamera(camera_video_config_));
        camera_.load()->ConfigureExternalTiming(camera_timing_);
    }

public:
    companion::desk::CameraReadiness CameraReadiness() const {return {};}
    companion::desk::CameraSessionResult AcquireCamera(uint64_t epoch,bool opt_in,companion::desk::CameraBinding& lease) {
        return camera_session_.Acquire(epoch,opt_in,CameraReadiness(),lease);
    }
    void ConfigureCameraTiming(std::shared_ptr<companion::desk::QualifiedCaptureTimingAuthority> qualified) {
        std::lock_guard<std::mutex> lock(camera_object_mutex_);camera_timing_=std::move(qualified);
        if(camera_.load())camera_.load()->ConfigureExternalTiming(camera_timing_);
    }
    uint64_t CameraAuthorityId(companion::desk::CameraBinding lease) {
        const auto never=camera_session_.NeverStartedAuthority(lease);
        if(never)return never;
        return camera_.load()?camera_.load()->StopAuthorityFor(lease):0;
    }
    bool CurrentCamera(companion::desk::CameraBinding lease) {return camera_session_.Current(lease);}
    void RevokeCamera(companion::desk::CameraBinding lease) {
        if(camera_session_.Revoke(lease)&&camera_.load())camera_.load()->Revoke(lease);
    }
    companion::desk::CameraSessionResult StartCamera(companion::desk::CameraBinding lease) {
        using Result=companion::desk::CameraSessionResult;
        // This checked qualification gate runs before constructing/configuring
        // a sensor. The current firmware's dependencies intentionally return false.
        if(!CameraReadiness().available())return Result::Unavailable;
        const auto permit=camera_session_.BeginStart(lease,CameraReadiness());
        if(permit!=Result::Reserved)return permit;
        InitializeCamera(); // rebuild fresh board-owned DVP/config values
        const bool started=camera_session_.Current(lease)&&camera_.load()->StartBound(camera_video_config_,lease,true,
            [](void* owner,companion::desk::CameraBinding b){return static_cast<M5StackCoreS3Board*>(owner)->CurrentCamera(b);},this);
        const auto result=camera_session_.FinishStart(lease,started);
        if(result!=Result::Started)camera_.load()->Revoke(lease);
        return result;
    }
    bool CaptureCamera(companion::desk::CameraBinding lease) {
        return camera_session_.Current(lease)&&camera_.load()&&camera_.load()->CaptureBound(lease);
    }
    bool CopyCameraBGRA(companion::desk::CameraBinding lease,uint8_t* out,size_t capacity,uint64_t& sequence,uint64_t& captured_ns) {
        sequence=captured_ns=0;
        if(camera_session_.Current(lease)&&camera_.load())return camera_.load()->CopyBGRA(lease,out,capacity,sequence,captured_ns);
        if(out){volatile uint8_t* p=out;const size_t n=capacity>companion::CameraBGRABytes?companion::CameraBGRABytes:capacity;for(size_t i=0;i<n;++i)p[i]=0;}
        return false;
    }
    bool CopyCameraLV01(companion::desk::CameraBinding lease,uint8_t* out,size_t capacity,size_t& written) {
        written=0;
        if(camera_session_.Current(lease)&&camera_.load())return camera_.load()->CopyLV01(lease,out,capacity,written);
        if(out){volatile uint8_t* p=out;const size_t n=capacity>companion::desk::BorrowedCameraAdapter::MaxEnvelope?companion::desk::BorrowedCameraAdapter::MaxEnvelope:capacity;for(size_t i=0;i<n;++i)p[i]=0;}
        return false;
    }
    bool HoldCameraRestart(companion::desk::CameraBinding lease) {return camera_session_.HoldRestart(lease);}
    bool ReleaseCameraRestart(const companion::desk::BoardCameraStopReport& report) {
        std::lock_guard<std::mutex> lock(camera_stop_receipt_mutex_);
        return report.exact_session()&&report.custody_released()&&report.driver().report_serial()==retained_camera_stop_.driver().report_serial()&&
            report.driver().authority_id()==retained_camera_stop_.driver().authority_id()&&camera_session_.ReleaseRestart(report.driver());
    }
    companion::desk::BoardCameraStopReport StopCameraChecked(companion::desk::CameraBinding lease) {
        using Outcome=companion::desk::CameraDriverStopOutcome;
        std::lock_guard<std::mutex> receipt_lock(camera_stop_receipt_mutex_);
        companion::desk::BoardCameraStopReport report;report.requested_=lease;
        if(!camera_session_.Revoke(lease)){
            if(!camera_session_.RetainedNoNewer(lease)||!(retained_camera_stop_.requested()==lease)||!retained_camera_stop_.custody_released())return report;
            report=retained_camera_stop_;
            if(report.driver_.owned_never_started()){
                uint64_t a=0,s=0;if(!camera_session_.RefreshNeverStarted(lease,a,s))return {};
                report.driver_.authority_=a;report.driver_.serial_=s;
            }else if(camera_.load())report.driver_=camera_.load()->StopCheckedReceipt(lease);
            if(!camera_session_.RetainedNoNewer(lease)||(!report.driver_.owned_driver_closed()&&!report.driver_.owned_never_started()))return {};
            retained_camera_stop_=report;return report;
        }
        report.exact_session_=true;
        uint64_t authority=0,serial=0;
        if(camera_session_.ReleaseNeverStarted(lease,authority,serial)){
            report.driver_.binding_=lease;report.driver_.authority_=authority;report.driver_.serial_=serial;
            report.driver_.outcome_=Outcome::NotStarted;report.driver_.revoked_=true;
            report.driver_.producer_quiesced_=true;report.driver_.raw_wiped_=true;
            report.custody_released_=true;retained_camera_stop_=report;return report;
        }
        if(!camera_.load())return report; // pending start may still publish a camera object
        report.driver_=camera_.load()->StopCheckedReceipt(lease);
        report.custody_released_=camera_session_.ReleaseReceipt(lease,report.driver_);
        if(report.custody_released_)retained_camera_stop_=report;
        return report;
    }
    companion::desk::CameraSessionResult StopCamera(companion::desk::CameraBinding lease) {
        using Result=companion::desk::CameraSessionResult;using Outcome=companion::desk::CameraDriverStopOutcome;
        const auto report=StopCameraChecked(lease);
        if(!report.exact_session())return Result::Stale;
        if(report.custody_released())return Result::Closed;
        switch(report.driver().outcome()){
            case Outcome::Busy:case Outcome::DriverClosed:case Outcome::NotStarted:return Result::Stopping;
            case Outcome::Stale:return Result::Stale;
            default:return Result::DriverFailure;
        }
    }
    M5StackCoreS3Board()
    {
        InitializeI2c();
        InitializeAxp2101();
#ifndef STACKCHAN_COMPANION_BOOT
        InitializePowerSaveTimer();
#endif
        InitializeAw9523();
#ifndef STACKCHAN_COMPANION_BOOT
        I2cDetect();
#endif
        InitializeFt6336();
        InitializeSpi();
        uint8_t switch_p0=0,sd_control=0,sd_voltage=0;
        const auto switch_error=aw9523_->ReadSdSwitch(switch_p0);
        const auto control_error=pmic_->ReadSdControl(sd_control);
        const auto voltage_error=pmic_->ReadSdVoltage(sd_voltage);
        stock_tools::RecordSdElectrical(switch_error,switch_p0,control_error,sd_control,voltage_error,sd_voltage);
        if(!stock_tools::MountStackChanIrSd())ESP_LOGW(TAG,"IR SD mount unavailable; card not formatted");
        InitializeIli9342Display();
#ifndef STACKCHAN_COMPANION_BOOT
        InitializeCamera();
#endif
        StartTouchpadTimer();
        GetBacklight()->RestoreBrightness();
    }

    void StartWifiConfigMode() override
    {
        ESP_LOGW(TAG, "WiFi setup required; opening StackChan App setup");
        GetHAL().requestAppSetup();
    }

    virtual std::string GetUuid() override
    {
        if (uuid_.empty() && (companion::CurrentAudioOwner() == companion::AudioOwner::Stock ||
            (companion::CurrentAudioOwner() == companion::AudioOwner::Native && stock_services::IsActive()))) {
            Settings identity("board", true);
            uuid_ = GenerateUuid();
            identity.SetString("uuid", uuid_); // Stable original activation identity after explicit Start.
        }
        return uuid_;
    }

    virtual AudioCodec* GetAudioCodec() override
    {
        static CoreS3AudioCodec audio_codec(i2c_bus_, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
                                            AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN, AUDIO_CODEC_AW88298_ADDR,
                                            AUDIO_CODEC_ES7210_ADDR, AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override
    {
        return display_;
    }

    virtual Camera* GetCamera() override
    {
        #ifdef STACKCHAN_COMPANION_BOOT
        if (!camera_.load() && (stock_services::AllowsCamera() || companion::CurrentAudioOwner()==companion::AudioOwner::Stock)) InitializeCamera();
#endif
        return camera_.load();
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override
    {
        std::lock_guard<std::mutex> lock(power_state_mutex_);
        if (!power_state_.status_valid || !power_state_.battery_level_valid) {
            return false;
        }

        level       = power_state_.battery_level;
        charging    = power_state_.charging;
        discharging = power_state_.discharging;
        return true;
    }

    virtual void SetPowerSaveLevel(PowerSaveLevel level) override
    {
        if (level != PowerSaveLevel::LOW_POWER) {
            if(power_save_timer_)power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveLevel(level);
    }

    virtual Backlight* GetBacklight() override
    {
        static CustomBacklight backlight(pmic_);
        return &backlight;
    }

    i2c_master_bus_handle_t GetI2cBus()
    {
        return i2c_bus_;
    }
};

DECLARE_BOARD(M5StackCoreS3Board);

i2c_master_bus_handle_t hal_bridge::board_get_i2c_bus()
{
    auto& board = (M5StackCoreS3Board&)Board::GetInstance();
    return board.GetI2cBus();
}

StackChanCamera* hal_bridge::board_get_camera()
{
    auto& board = Board::GetInstance();
    auto camera = (StackChanCamera*)board.GetCamera();
    return camera;
}

int hal_bridge::board_get_battery_level()
{
    auto& board      = Board::GetInstance();
    int level        = 0;
    bool charging    = false;
    bool discharging = false;
    if (board.GetBatteryLevel(level, charging, discharging)) {
        return level;
    } else {
        return 100;
    }
}

bool hal_bridge::board_is_battery_charging()
{
    auto& board      = Board::GetInstance();
    int level        = 0;
    bool charging    = false;
    bool discharging = false;
    if (board.GetBatteryLevel(level, charging, discharging)) {
        return charging;
    } else {
        return false;
    }
}

void hal_bridge::board_set_backlight_brightness(uint8_t brightness, bool permanent)
{
    auto& board    = Board::GetInstance();
    auto backlight = board.GetBacklight();
    if (backlight) {
        backlight->SetBrightness(brightness, false);
        if (permanent) {
            Settings settings("display", true);
            settings.SetInt("brightness", brightness);
        }
    }
}

uint8_t hal_bridge::board_get_backlight_brightness()
{
    auto& board    = Board::GetInstance();
    auto backlight = board.GetBacklight();
    if (backlight) {
        return backlight->brightness();
    } else {
        return 0;
    }
}

void hal_bridge::board_set_speaker_volume(uint8_t volume, bool permanent)
{
    auto codec=Board::GetInstance().GetAudioCodec();
    if(!codec)return;
    auto scope=companion::CurrentSettingsScope();
    companion::SetSettingsScope(companion::SettingsScope::None);
    codec->SetOutputVolume(volume);
    companion::SetSettingsScope(scope);
    // Codec::SetOutputVolume persists through Settings only inside the explicit
    // Volume scope; permanent Confirm is the sole allowed persistent operation.
    if(permanent){Settings settings("audio",true);settings.SetInt("output_volume",volume);}
}
uint8_t hal_bridge::board_get_speaker_volume()
{
    auto codec=Board::GetInstance().GetAudioCodec();
    return codec ? codec->output_volume() : 70;
}

void hal_bridge::toggle_xiaozhi_chat_state()
{
    auto& app = Application::GetInstance();
    if (app.GetDeviceState() == kDeviceStateStarting) {
        // EnterWifiConfigMode();
        return;
    }
    app.ToggleChatState();
}

companion::desk::CameraReadiness hal_bridge::board_camera_readiness() {
    return static_cast<M5StackCoreS3Board&>(Board::GetInstance()).CameraReadiness();
}
companion::desk::CameraSessionResult hal_bridge::board_camera_acquire(uint64_t epoch,bool opt_in,companion::desk::CameraBinding& lease) {
    return static_cast<M5StackCoreS3Board&>(Board::GetInstance()).AcquireCamera(epoch,opt_in,lease);
}
companion::desk::CameraSessionResult hal_bridge::board_camera_start(companion::desk::CameraBinding lease) {
    return static_cast<M5StackCoreS3Board&>(Board::GetInstance()).StartCamera(lease);
}
companion::desk::CameraSessionResult hal_bridge::board_camera_stop(companion::desk::CameraBinding lease) {
    return static_cast<M5StackCoreS3Board&>(Board::GetInstance()).StopCamera(lease);
}
bool hal_bridge::board_camera_current(companion::desk::CameraBinding lease) {
    return static_cast<M5StackCoreS3Board&>(Board::GetInstance()).CurrentCamera(lease);
}
void hal_bridge::board_camera_revoke(companion::desk::CameraBinding lease) {
    static_cast<M5StackCoreS3Board&>(Board::GetInstance()).RevokeCamera(lease);
}

bool hal_bridge::board_camera_capture(companion::desk::CameraBinding lease) {
    return static_cast<M5StackCoreS3Board&>(Board::GetInstance()).CaptureCamera(lease);
}
bool hal_bridge::board_camera_copy_bgra(companion::desk::CameraBinding lease,uint8_t* out,size_t capacity,uint64_t& sequence,uint64_t& captured_ns) {
    return static_cast<M5StackCoreS3Board&>(Board::GetInstance()).CopyCameraBGRA(lease,out,capacity,sequence,captured_ns);
}
bool hal_bridge::board_camera_copy_lv01(companion::desk::CameraBinding lease,uint8_t* out,size_t capacity,size_t& written) {
    return static_cast<M5StackCoreS3Board&>(Board::GetInstance()).CopyCameraLV01(lease,out,capacity,written);
}

companion::desk::BoardCameraStopReport hal_bridge::board_camera_stop_checked(companion::desk::CameraBinding lease) {
    return static_cast<M5StackCoreS3Board&>(Board::GetInstance()).StopCameraChecked(lease);
}

uint64_t hal_bridge::board_camera_authority_id(companion::desk::CameraBinding lease) {
    return static_cast<M5StackCoreS3Board&>(Board::GetInstance()).CameraAuthorityId(lease);
}

void hal_bridge::board_camera_configure_timing(std::shared_ptr<companion::desk::QualifiedCaptureTimingAuthority> qualified) {
    static_cast<M5StackCoreS3Board&>(Board::GetInstance()).ConfigureCameraTiming(std::move(qualified));
}
uint32_t hal_bridge::board_camera_driver_revision() {return esp_video_capture_clock_contract_revision();}

bool hal_bridge::board_camera_hold_restart(companion::desk::CameraBinding lease) {return static_cast<M5StackCoreS3Board&>(Board::GetInstance()).HoldCameraRestart(lease);}
bool hal_bridge::board_camera_release_restart(const companion::desk::BoardCameraStopReport& report) {return static_cast<M5StackCoreS3Board&>(Board::GetInstance()).ReleaseCameraRestart(report);}
