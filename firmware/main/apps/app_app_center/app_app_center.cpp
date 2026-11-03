#include <apps/runtime_lifecycle/session.h>
/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_app_center.h"
#include <hal/hal.h>
#include <mooncake.h>
#include <mooncake_log.h>
#include <assets/assets.h>
#include <apps/common/common.h>
#include <memory>

using namespace mooncake;
using namespace smooth_ui_toolkit::lvgl_cpp;

AppAppCenter::AppAppCenter()
{
    // 配置 App 名
    setAppInfo().name = "APP.CENTER";
    // 配置 App 图标
    static auto icon  = assets::get_image("icon_app_center.bin");
    setAppInfo().icon = (void*)&icon;
    // 配置 App 主题颜色
    static uint32_t theme_color = 0xF4A354;
    setAppInfo().userData       = (void*)&theme_color;
}

void AppAppCenter::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppAppCenter::reloadCatalog()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    // Create loading page
    std::unique_ptr<view::LoadingPage> loading_page;
    {
        LvglLockGuard lock;
        loading_page = std::make_unique<view::LoadingPage>(0xF4A354, 0x332609);
    }

    // Start network
    GetHAL().startNetwork([&](std::string_view msg) {
        LvglLockGuard lock;
        loading_page->setMessage(msg);
    });

    // Fetch app list
    {
        LvglLockGuard lock;
        loading_page->setMessage("Fetching app list...");
    }
    _app_list = GetHAL().fetchAppList();

    LvglLockGuard lock;

    // Destroy loading page
    loading_page.reset();

    _app_list_page = std::make_unique<view::AppListPage>(_app_list);

}

void AppAppCenter::onOpen()
{
    stock_services::SetFirmwareInstallAllowed(false);
    reloadCatalog();
    LvglLockGuard lock;
    view::create_home_indicator([this]() { close(); }, 0xFFDF9A, 0x47330A);
    view::create_status_bar(0xFFDF9A, 0x47330A);
}

void AppAppCenter::onRunning()
{
    if (_refresh_requested) {
        _refresh_requested = false;
        reloadCatalog();
    }
    if (_launch_requested) {
        // Clear before starting: a failed installation must never retry automatically.
        _launch_requested = false;
        if (_selected_index >= 0 && _selected_index < static_cast<int>(_app_list.size())) {
            stock_services::FirmwareInstallScope permission;
            GetHAL().launchApp(_app_list[_selected_index].firmwareUrl, [this](int percent) {
                LvglLockGuard lock;
                if (_app_install_page) _app_install_page->setProgress(percent);
            });
        }
        // The original OTA implementation restarts on success. Any return is failure.
        LvglLockGuard lock;
        if (_app_install_page) _app_install_page->setFailed();
    }

    LvglLockGuard lock;
    if (_app_list_page) {
        if (_app_list_page->checkRefresh()) {
            _app_list_page.reset();
            _refresh_requested = true;
        } else {
            int selected = _app_list_page->isSelected();
            if (selected >= 0 && selected < static_cast<int>(_app_list.size())) {
                _selected_index = selected;
                _app_list_page.reset();
                _app_detail_page = std::make_unique<view::AppDetailPage>(_app_list[_selected_index]);
            }
        }
    }
    if (_app_detail_page) {
        if (_app_detail_page->checkBack()) {
            _app_detail_page.reset();
            _app_list_page = std::make_unique<view::AppListPage>(_app_list);
        } else if (_app_detail_page->checkLaunch()) {
            _app_detail_page.reset();
            _app_confirm_page = std::make_unique<view::AppConfirmPage>(_app_list[_selected_index]);
        }
    }
    if (_app_confirm_page) {
        if (_app_confirm_page->checkCancel()) {
            _app_confirm_page.reset();
            _app_detail_page = std::make_unique<view::AppDetailPage>(_app_list[_selected_index]);
        } else if (_app_confirm_page->checkConfirm()) {
            _app_confirm_page.reset();
            _app_install_page = std::make_unique<view::AppInstallPage>(_app_list[_selected_index]);
            _launch_requested = true;
        }
    }
    if (_app_install_page) {
        if (_app_install_page->checkBack()) {
            _app_install_page.reset();
            _app_detail_page = std::make_unique<view::AppDetailPage>(_app_list[_selected_index]);
        } else if (_app_install_page->checkRetry()) {
            _app_install_page.reset();
            _app_confirm_page = std::make_unique<view::AppConfirmPage>(_app_list[_selected_index]);
        }
    }
    view::update_home_indicator();
    view::update_status_bar();
}

void AppAppCenter::onClose()
{
#ifdef STACKCHAN_COMPANION_BOOT
    if(runtime_lifecycle::DeferNativeClose())return;
#endif
    mclog::tagInfo(getAppInfo().name, "on close");

    LvglLockGuard lock;

    stock_services::SetFirmwareInstallAllowed(false);
    _app_confirm_page.reset();
    _refresh_requested = false;
    _app_list.clear();
    _app_list_page.reset();
    _app_detail_page.reset();
    _app_install_page.reset();
    _selected_index   = -1;
    _launch_requested = false;

    view::destroy_home_indicator();
    view::destroy_status_bar();

    GetHAL().requestWarmReboot(3);
}
