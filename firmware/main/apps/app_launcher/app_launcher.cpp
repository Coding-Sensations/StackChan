/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_launcher.h"
#include "view/screensaver_library.hpp"
#include <hal/hal.h>
#include <hal/hal_toggles.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <stackchan/stackchan.h>
#include <cstdint>
#include <settings.h>
#include <esp_random.h>
#ifdef STACKCHAN_COMPANION_BOOT
#include <companion/settings_policy.hpp>
#endif

using namespace mooncake;

namespace {

hal_toggles::ToggleStates load_toggle_states()
{
    auto states = hal_toggles::default_states();
    hal_toggles::load_states(states);
    return states;
}

bool is_app_visible(const mooncake::AppProps_t& props, const hal_toggles::ToggleStates& states)
{
    return hal_toggles::is_app_enabled(props.appID, states);
}

}  // namespace

void AppLauncher::onLauncherCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");

    // 打开自己
    open();
}

void AppLauncher::onLauncherOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    LvglLockGuard lock;
    // Returning from an app means Home, independent of its former category.
    _category.reset();

#ifndef STACKCHAN_COMPANION_BOOT
    if (!_startup_checked && GetHAL().consumeAppSetupRequest()) {
        mclog::tagInfo(getAppInfo().name, "resume App setup after network/binding failure");
        _startup_worker = std::make_unique<setup_workers::WifiSetupWorker>();
    } else
#endif
    if (
#ifndef STACKCHAN_COMPANION_BOOT
        !_startup_checked && !GetHAL().isAppConfiged()
#else
        false // Companion boot never opens account/network setup automatically.
#endif
    ) {
        mclog::tagInfo(getAppInfo().name, "app not configured, start startup worker");
        _startup_worker = std::make_unique<setup_workers::StartupWorker>();
    } else {
        create_launcher_view();
    }
}

void AppLauncher::onLauncherRunning()
{
    LvglLockGuard lock;

    if (_startup_worker) {
        _startup_worker->update();
        if (_startup_worker->isDone()) {
            _startup_worker.reset();
            _startup_checked = true;
            create_launcher_view();
        }
    } else {
        _view->update();
        handle_category_navigation();
        screensaver_update();
    }

    GetStackChan().update();
}

void AppLauncher::onLauncherClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    LvglLockGuard lock;

    _screensaver.reset();
    _view.reset();
    _launcher_screen=nullptr;
    _pending_navigation=-1;
    _screensaver_timecount=0;
}

void AppLauncher::onLauncherDestroy()
{
    mclog::tagInfo(getAppInfo().name, "on close");
}

void AppLauncher::create_launcher_view(int preferredAppID)
{
    _launcher_screen=lv_screen_active();
    const auto installed = getAppProps();
    const auto states    = load_toggle_states();
    std::vector<mooncake::AppProps_t> shown;
    if (_category) {
        for (const auto& props : installed) {
            if (is_app_visible(props, states) && launcher_categories::Assign(props.info.name) == *_category) shown.push_back(props);
        }
        if (!installed.empty()) {
            auto back      = installed.front();
            back.appID     = launcher_categories::Back;
            back.info.name = "Back to categories";
            shown.push_back(back);
        }
    } else {
        for (size_t index = 0; index < launcher_categories::Titles.size(); ++index) {
            const auto group = static_cast<launcher_categories::Group>(index);
            for (const auto& props : installed) {
                if (is_app_visible(props, states) && launcher_categories::Assign(props.info.name) == group) {
                    auto category      = props;
                    category.appID     = launcher_categories::Token(group);
                    category.info.name = std::string(launcher_categories::Titles[index]);
                    shown.push_back(category);
                    break;
                }
            }
        }
    }
    // Retire callbacks/helpers before replacing their shared runtime.
    _view.reset();
    _view = std::make_unique<view::LauncherView>();
    const int focusAppID=_category&&!shown.empty()?shown.front().appID:preferredAppID;
    _view->init(std::move(shown),focusAppID);
    _view->onAppClicked = [&](int appID) {_pending_navigation=appID;};
}

void AppLauncher::handle_category_navigation()
{
    // Called after LauncherView::update returns; never destroy its view in a callback.
    const int action = _pending_navigation;
    _pending_navigation = -1;
    if (action == -1) return;
    if (action == launcher_categories::Back) {
        const auto focus = _category ? launcher_categories::Token(*_category)
                                     : launcher_categories::Token(launcher_categories::Group::Companions);
        _category.reset();
        create_launcher_view(focus);
        return;
    }
    const auto states = load_toggle_states();
    if (launcher_categories::IsGroup(action)) {
        const auto group = launcher_categories::Decode(action);
        int only         = -1;
        unsigned count   = 0;
        for (const auto& props : getAppProps()) {
            if (is_app_visible(props, states) && launcher_categories::Assign(props.info.name) == group) {
                only = props.appID;
                ++count;
            }
        }
        if (count == 1) {
            openApp(only);
            return;
        }
        _category = group;
        create_launcher_view();
        return;
    }
    bool installed = false;
    for (const auto& props : getAppProps()) {
        if (is_app_visible(props, states) && props.appID == action &&
            (!_category || launcher_categories::Assign(props.info.name) == *_category)) {
            installed = true;
            break;
        }
    }
    if (installed) openApp(action);
}

void AppLauncher::screensaver_update()
{
    // The stock launcher alone drives idle lifecycle. No independent timer/task.
    if (_screensaver && !_screensaver->isActiveScreen()) _screensaver.reset();
    const bool launcher_active=_view && !_screensaver && lv_screen_active()==_launcher_screen;
    bool media_owned=false;
#ifdef STACKCHAN_COMPANION_BOOT
    media_owned=companion::CurrentAudioOwner()!=companion::AudioOwner::None;
#endif
    const uint32_t idle=lv_display_get_inactive_time(nullptr);
    if (_screensaver && (idle<30000 || media_owned || _going_to_open_app_id>=0)) {
        _screensaver.reset(); // current saver restores its own previous screen
        return;              // first touch is consumed on the saver screen
    }
    const uint32_t now=GetHAL().millis();
    if (!_screensaver) {
        if(!view::saver::MayEnterIdle(idle,launcher_active,_going_to_open_app_id>=0,media_owned))return;
        Settings settings("screensaver");
        auto selection=view::saver::Library::fromPayload(settings.GetString("playlist")).effective();
        auto style=static_cast<view::saver::Style>(_slideshow.start(selection,now));
        _screensaver=std::make_unique<view::Screensaver>(style);
        _screensaver->init();
        _screensaver_timecount=now;
    } else if (_slideshow.advance(now,[]{return esp_random();})) {
        // Replace within the same LVGL lock; no overlay or extra UI task.
        _screensaver.reset();
        _screensaver=std::make_unique<view::Screensaver>(static_cast<view::saver::Style>(_slideshow.current()));
        _screensaver->init();
        _screensaver_timecount=now;
    }
    if (_screensaver && uint32_t(now-_screensaver_timecount)>30) {
        _screensaver_timecount=now;
        _screensaver->update();
    }
}
