/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <hal/hal.h>
#include <smooth_lvgl.hpp>
#include <uitk/short_namespace.hpp>
#include <assets/assets.h>
#include <mooncake_log.h>
#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <apps/stock_services/firmware_install_policy.h>

namespace view {

/**
 * @brief
 *
 */
class AppListPage {
public:
    AppListPage(const app_center::AppInfoList_t& app_list)

    {
        _panel = std::make_unique<uitk::lvgl_cpp::Container>(lv_screen_active());
        _panel->setSize(320, 240);
        _panel->setAlign(LV_ALIGN_CENTER);
        _panel->setBgColor(lv_color_hex(0xFFFAD6));
        _panel->setFlexFlow(LV_FLEX_FLOW_COLUMN);
        _panel->setFlexAlign(LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        _panel->setPadding(52, 72, 0, 0);
        _panel->setPadRow(18);
        _panel->setRadius(0);
        _panel->setBorderWidth(0);
        _panel->setScrollbarMode(LV_SCROLLBAR_MODE_ACTIVE);

        _bg_image = assets::get_image("app_center_bg.png");
        if (_bg_image.data_size != 0) {
            lv_obj_set_style_bg_image_src(_panel->get(), &_bg_image, LV_PART_MAIN);
        }

        _title_panel = std::make_unique<uitk::lvgl_cpp::Container>(lv_screen_active());
        _title_panel->setSize(320, 28);
        _title_panel->align(LV_ALIGN_TOP_MID, 0, 0);
        _title_panel->setBgColor(lv_color_hex(0xF4A354));
        _title_panel->setPadding(0, 0, 0, 0);
        _title_panel->setRadius(0);
        _title_panel->setBorderWidth(0);
        _title_panel->setScrollbarMode(LV_SCROLLBAR_MODE_ACTIVE);

        _title = std::make_unique<uitk::lvgl_cpp::Label>(*_title_panel);
        _title->setText("App Center");
        _title->setTextFont(&lv_font_montserrat_16);
        _title->setTextColor(lv_color_hex(0xFFF8C7));
        _title->align(LV_ALIGN_CENTER, 0, 0);

        if (app_list.empty()) {
            _empty = std::make_unique<uitk::lvgl_cpp::Label>(*_panel);
            _empty->setText("No catalog entries. Check Wi-Fi and refresh.");
            _empty->setWidth(282);
            _empty->setTextFont(&lv_font_montserrat_16);
            _empty->setLongMode(LV_LABEL_LONG_WRAP);
        }
        auto refresh = std::make_unique<uitk::lvgl_cpp::Button>(*_panel);
        refresh->setSize(282, 48);
        refresh->label().setText("Refresh catalog");
        refresh->onClick().connect([this]() { _refresh = true; });
        _buttons.push_back(std::move(refresh));

        int index = 0;
        for (const auto& app : app_list) {
            auto btn = std::make_unique<uitk::lvgl_cpp::Button>(*_panel);
            btn->setSize(282, 52);
            btn->setRadius(18);
            btn->setBorderWidth(0);
            btn->setShadowWidth(0);
            btn->setBgColor(lv_color_hex(0xFFDF9A));

            btn->label().setText(app.name);
            btn->label().setTextFont(&lv_font_montserrat_20);
            btn->label().setTextColor(lv_color_hex(0x47330A));
            btn->label().setLongMode(LV_LABEL_LONG_SCROLL_CIRCULAR);
            btn->label().setWidth(220);
            btn->label().setTextAlign(LV_TEXT_ALIGN_CENTER);
            btn->label().setAlign(LV_ALIGN_CENTER);

            btn->onClick().connect([this, index]() { _clicked_index = index; });

            _buttons.push_back(std::move(btn));
            index++;
        }
    }

    bool checkRefresh() { bool value = _refresh; _refresh = false; return value; }

    int isSelected()
    {
        int temp       = _clicked_index;
        _clicked_index = -1;
        return temp;
    }

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Container> _title_panel;
    std::unique_ptr<uitk::lvgl_cpp::Label> _title;
    std::vector<std::unique_ptr<uitk::lvgl_cpp::Button>> _buttons;
    lv_image_dsc_t _bg_image;

    std::unique_ptr<uitk::lvgl_cpp::Label> _empty;
    bool _refresh = false;
    int _clicked_index = -1;
};

/**
 * @brief
 *
 */
class AppDetailPage {
public:
    AppDetailPage(const app_center::AppInfo_t& app_info)
    {
        _panel = std::make_unique<uitk::lvgl_cpp::Container>(lv_screen_active());
        _panel->setBgColor(lv_color_hex(0xFFFAD6));
        _panel->align(LV_ALIGN_CENTER, 0, 0);
        _panel->setBorderWidth(0);
        _panel->setSize(320, 240);
        _panel->setRadius(0);
        _panel->setFlexFlow(LV_FLEX_FLOW_COLUMN);
        _panel->setFlexAlign(LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        _panel->setPadding(48, 72, 0, 0);
        _panel->setPadRow(18);
        _panel->setScrollbarMode(LV_SCROLLBAR_MODE_ACTIVE);

        _bg_image = assets::get_image("app_center_bg.png");
        if (_bg_image.data_size != 0) {
            lv_obj_set_style_bg_image_src(_panel->get(), &_bg_image, LV_PART_MAIN);
        }

        _title = std::make_unique<uitk::lvgl_cpp::Label>(*_panel);
        _title->setText(app_info.name);
        _title->setTextFont(&lv_font_montserrat_20);
        _title->setTextColor(lv_color_hex(0x47330A));
        _title->setTextAlign(LV_TEXT_ALIGN_CENTER);
        _title->setLongMode(LV_LABEL_LONG_SCROLL_CIRCULAR);
        _title->setWidth(220);
        _title->setHeight(28);

        _desc_label = std::make_unique<uitk::lvgl_cpp::Label>(*_panel);
        _desc_label->setText(std::string("Replacement firmware\n\n") + app_info.description +
                            (stock_services::IsFirmwareImageUrl(app_info.firmwareUrl) ? "" : "\n\nNo valid firmware URL."));
        _desc_label->setTextFont(&lv_font_montserrat_20);
        _desc_label->setTextColor(lv_color_hex(0x47330A));
        _desc_label->setTextAlign(LV_TEXT_ALIGN_LEFT);
        _desc_label->setWidth(260);
        _desc_label->setLongMode(LV_LABEL_LONG_WRAP);

        _btn_container = std::make_unique<uitk::lvgl_cpp::Container>(*_panel);
        _btn_container->setSize(LV_PCT(100), 80);
        _btn_container->setBgOpa(LV_OPA_TRANSP);
        _btn_container->setBorderWidth(0);
        _btn_container->setPadding(0, 0, 0, 0);
        _btn_container->setPadColumn(28);
        _btn_container->setFlexFlow(LV_FLEX_FLOW_ROW);
        _btn_container->setFlexAlign(LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        _btn_back = std::make_unique<uitk::lvgl_cpp::Button>(*_btn_container);
        _btn_back->setSize(112, 48);
        _btn_back->setRadius(18);
        _btn_back->setBgColor(lv_color_hex(0xFFDF9A));
        _btn_back->setBorderWidth(0);
        _btn_back->setShadowWidth(0);
        _btn_back->label().setText("Back");
        _btn_back->label().setTextFont(&lv_font_montserrat_20);
        _btn_back->label().setTextColor(lv_color_hex(0x47330A));
        _btn_back->onClick().connect([this]() { _is_back = true; });

        _btn_launch = std::make_unique<uitk::lvgl_cpp::Button>(*_btn_container);
        _btn_launch->setSize(112, 48);
        _btn_launch->setRadius(18);
        _btn_launch->setBgColor(lv_color_hex(0xFFAC6D));
        _btn_launch->setBorderWidth(0);
        _btn_launch->setShadowWidth(0);
        _btn_launch->label().setText("Install");
        _btn_launch->label().setTextFont(&lv_font_montserrat_20);
        _btn_launch->label().setTextColor(lv_color_hex(0x47330A));
        if (stock_services::IsFirmwareImageUrl(app_info.firmwareUrl))
            _btn_launch->onClick().connect([this]() { _is_launch = true; });
        else lv_obj_add_state(_btn_launch->get(), LV_STATE_DISABLED);
    }

    bool checkBack()
    {
        if (_is_back) {
            _is_back = false;
            return true;
        }
        return false;
    }

    bool checkLaunch()
    {
        if (_is_launch) {
            _is_launch = false;
            return true;
        }
        return false;
    }

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Label> _title;
    std::unique_ptr<uitk::lvgl_cpp::Label> _desc_label;
    std::unique_ptr<uitk::lvgl_cpp::Container> _btn_container;
    std::unique_ptr<uitk::lvgl_cpp::Button> _btn_back;
    std::unique_ptr<uitk::lvgl_cpp::Button> _btn_launch;
    lv_image_dsc_t _bg_image;

    bool _is_back   = false;
    bool _is_launch = false;
};

/**
 * @brief
 *
 */
class AppConfirmPage {
public:
    explicit AppConfirmPage(const app_center::AppInfo_t& app)
    {
        _panel = lv_obj_create(lv_screen_active());
        lv_obj_set_size(_panel, 320, 240);
        lv_obj_center(_panel);
        lv_obj_set_style_bg_color(_panel, lv_color_hex(0xFFFAD6), 0);
        lv_obj_set_style_pad_top(_panel, 48, 0);
        lv_obj_set_style_pad_bottom(_panel, 40, 0);
        lv_obj_set_flex_flow(_panel, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(_panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t* title = lv_label_create(_panel);
        lv_obj_set_width(title, 280);
        lv_label_set_text(title, app.name.c_str());
        auto source=lv_label_create(_panel);lv_obj_set_size(source,280,24);
        lv_label_set_long_mode(source,LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_label_set_text(source,("Source: "+app.firmwareUrl.substr(0,app.firmwareUrl.find('?'))).c_str());
        lv_obj_t* message = lv_label_create(_panel);
        lv_obj_set_width(message, 280);
        std::string warning = "Install replacement firmware?\n\n"
            "This replaces the current Companion and Tools firmware. Companion, Tools and Games may be removed. "
            "Restore this custom build over USB to get them back.\n\n";
        if (app.firmwareUrl.rfind("http://", 0) == 0)
            warning += "Unencrypted download (HTTP).\n\n";
        warning += "The device restarts after installation.";
        lv_label_set_text(message, warning.c_str());
        addButton("Cancel", false);
        addButton("Install and restart", true);
    }
    ~AppConfirmPage() { lv_obj_delete(_panel); }
    bool checkCancel() { bool value = _cancel; _cancel = false; return value; }
    bool checkConfirm() { bool value = _confirm; _confirm = false; return value; }
private:
    void addButton(const char* text, bool confirm)
    {
        auto button = lv_button_create(_panel);
        lv_obj_set_size(button, 280, 48);
        auto label = lv_label_create(button);
        lv_label_set_text(label, text);
        lv_obj_center(label);
        lv_obj_add_event_cb(button, [](lv_event_t* event) {
            auto self = static_cast<AppConfirmPage*>(lv_event_get_user_data(event));
            if (lv_event_get_target_obj(event) == self->_confirm_button) self->_confirm = true;
            else self->_cancel = true;
        }, LV_EVENT_CLICKED, this);
        if (confirm) _confirm_button = button;
    }
    lv_obj_t* _panel = nullptr;
    lv_obj_t* _confirm_button = nullptr;
    bool _cancel = false, _confirm = false;
};

class AppInstallPage {
public:
    AppInstallPage(const app_center::AppInfo_t& app_info)
    {
        _panel = std::make_unique<uitk::lvgl_cpp::Container>(lv_screen_active());
        _panel->setBgColor(lv_color_hex(0xFFFAD6));
        _panel->align(LV_ALIGN_CENTER, 0, 0);
        _panel->setBorderWidth(0);
        _panel->setSize(320, 240);
        _panel->setRadius(0);
        _panel->setPadding(0, 0, 0, 0);

        _bg_image = assets::get_image("app_center_bg.png");
        if (_bg_image.data_size != 0) {
            lv_obj_set_style_bg_image_src(_panel->get(), &_bg_image, LV_PART_MAIN);
        }

        _title = std::make_unique<uitk::lvgl_cpp::Label>(*_panel);
        _title->setText(app_info.name);
        _title->setTextFont(&lv_font_montserrat_20);
        _title->setTextColor(lv_color_hex(0x47330A));
        _title->setTextAlign(LV_TEXT_ALIGN_CENTER);
        _title->align(LV_ALIGN_TOP_MID, 0, 48);
        _title->setLongMode(LV_LABEL_LONG_SCROLL_CIRCULAR);
        _title->setWidth(220);
        _title->setHeight(28);

        _tips = std::make_unique<uitk::lvgl_cpp::Label>(*_panel);
        _tips->setText("Downloading...");
        _tips->setTextFont(&lv_font_montserrat_16);
        _tips->setTextColor(lv_color_hex(0xA36135));
        _tips->setTextAlign(LV_TEXT_ALIGN_CENTER);
        _tips->setLongMode(LV_LABEL_LONG_SCROLL_CIRCULAR);
        _tips->align(LV_ALIGN_CENTER, 0, -40);

        _progress_bar = std::make_unique<uitk::lvgl_cpp::Bar>(*_panel);
        _progress_bar->setSize(260, 92);
        _progress_bar->setRadius(18);
        _progress_bar->setRadius(0, LV_PART_INDICATOR);
        _progress_bar->align(LV_ALIGN_CENTER, 0, 23);
        _progress_bar->setBgColor(lv_color_hex(0xFFDF9A));
        _progress_bar->setBgColor(lv_color_hex(0xFF9E5D), LV_PART_INDICATOR);
        _progress_bar->setBgOpa(LV_OPA_COVER);
        _progress_bar->setRange(0, 100);
        _progress_bar->setValue(0);

        _progress = std::make_unique<uitk::lvgl_cpp::Label>(*_panel);
        _progress->setTextFont(&lv_font_montserrat_24);
        _progress->setTextColor(lv_color_hex(0x47330A));
        _progress->align(LV_ALIGN_CENTER, 0, 23);
        _progress->setText("");
    }

    bool checkBack() { bool value = _back; _back = false; return value; }
    bool checkRetry() { bool value = _retry; _retry = false; return value; }
    void setFailed()
    {
        _tips->setText("Install failed. Try again.");
        lv_obj_add_flag(_progress_bar->get(), LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_progress->get(), LV_OBJ_FLAG_HIDDEN);
        if (!_btn_back) {
            _btn_back = std::make_unique<uitk::lvgl_cpp::Button>(*_panel);
            _btn_back->setSize(112, 48);
            _btn_back->align(LV_ALIGN_BOTTOM_LEFT, 20, -30);
            _btn_back->label().setText("Back");
            _btn_back->onClick().connect([this]() { _back = true; });
            _btn_retry = std::make_unique<uitk::lvgl_cpp::Button>(*_panel);
            _btn_retry->setSize(112, 48);
            _btn_retry->align(LV_ALIGN_BOTTOM_RIGHT, -20, -30);
            _btn_retry->label().setText("Retry");
            _btn_retry->onClick().connect([this]() { _retry = true; });
        }
    }
    void setProgress(int percent)
    {
        if (percent < 0) { setFailed(); return; }
        if (percent > 100) percent = 100;
        _progress->setText(fmt::format("{}%", percent));
        _progress_bar->setValue(percent);
    }

private:
    std::unique_ptr<uitk::lvgl_cpp::Container> _panel;
    std::unique_ptr<uitk::lvgl_cpp::Label> _title;
    std::unique_ptr<uitk::lvgl_cpp::Label> _tips;
    std::unique_ptr<uitk::lvgl_cpp::Label> _progress;
    std::unique_ptr<uitk::lvgl_cpp::Bar> _progress_bar;
    std::unique_ptr<uitk::lvgl_cpp::Button> _btn_back, _btn_retry;
    bool _back = false, _retry = false;
    lv_image_dsc_t _bg_image;
};

}  // namespace view
