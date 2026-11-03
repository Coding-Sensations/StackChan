/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include "view/view.h"
#include "view/slideshow_policy.hpp"
#include <apps/app_setup/workers/workers.h>
#include <mooncake.h>
#include <mooncake_templates.h>
#include <cstdint>
#include <optional>
#include "category_routes.hpp"
#include <memory>

class AppLauncher : public mooncake::templates::AppLauncherBase {
public:
    void onLauncherCreate() override;
    void onLauncherOpen() override;
    void onLauncherRunning() override;
    void onLauncherClose() override;
    void onLauncherDestroy() override;

private:
    std::unique_ptr<view::LauncherView> _view;
    std::unique_ptr<view::Screensaver> _screensaver;
    std::unique_ptr<setup_workers::WorkerBase> _startup_worker;
    uint32_t _screensaver_timecount = 0;
    bool _startup_checked           = false;
    view::saver::Slideshow _slideshow;
    lv_obj_t* _launcher_screen = nullptr;

    std::optional<launcher_categories::Group> _category;
    int _pending_navigation=-1;
    void handle_category_navigation();
    void create_launcher_view(int preferredAppID=launcher_categories::Token(launcher_categories::Group::Companions));
    void screensaver_update();
};
