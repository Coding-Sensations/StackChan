/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <mooncake.h>
#include <smooth_ui_toolkit.hpp>
#include <uitk/short_namespace.hpp>
#include <games/dvd_screensaver/dvd_screensaver.hpp>
#include <smooth_lvgl.hpp>
#include <functional>
#include <vector>
#include <memory>
#include <optional>
#include "screensaver_styles.hpp"

namespace view {

/**
 * @brief
 *
 */
class LauncherView {
public:
    ~LauncherView();

    enum State_t {
        STATE_STARTUP,
        STATE_NORMAL,
    };

    std::function<void(int appID)> onAppClicked;

    void init(std::vector<mooncake::AppProps_t> appPorps,std::optional<int> preferredAppID=std::nullopt);
    void update();

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::vector<std::unique_ptr<uitk::lvgl_cpp::Container>> _icon_panels;
    std::vector<std::unique_ptr<uitk::lvgl_cpp::Image>> _icon_images;
    std::vector<std::unique_ptr<uitk::lvgl_cpp::Container>> _lr_indicator_panels;
    std::vector<std::unique_ptr<uitk::lvgl_cpp::Image>> _lr_indicators_images;

    std::unique_ptr<uitk::AnimateVector2> _startup_anim;

    int _clicked_app_id = -1;
    State_t _state      = STATE_STARTUP;

    void handle_state_startup();
    void handle_state_normal();
};

/**
 * @brief
 *
 */
class Screensaver : public uitk::games::dvd_screensaver::DvdScreensaver {
public:
    explicit Screensaver(saver::Style style=saver::Style::Dvd) : _style(style) {}
    ~Screensaver();

    void onInit() override;
    void onBuildLevel() override;
    void onRender(float dt) override;
    void onLogoCollide(int logoGroupId) override;
    bool isActiveScreen() const;

private:
    std::unique_ptr<uitk::lvgl_cpp::ScreenActive> _prev_screen;
    std::unique_ptr<uitk::lvgl_cpp::Screen> _screen;
    std::unique_ptr<uitk::lvgl_cpp::Container> _logo;
    std::unique_ptr<uitk::lvgl_cpp::Container> _left_eye;
    std::unique_ptr<uitk::lvgl_cpp::Container> _right_eye;
    std::unique_ptr<uitk::lvgl_cpp::Container> _mouth;
    int _color_index = 0;
    saver::Style _style = saver::Style::Dvd;
    uint32_t _animation_ms = 0;
    lv_obj_t* _effect_canvas = nullptr;
    lv_obj_t* _effect_error = nullptr;
    uint16_t* _effect_pixels = nullptr;
    bool _effect_failed = false;
    void showEffectError();
};

}  // namespace view
