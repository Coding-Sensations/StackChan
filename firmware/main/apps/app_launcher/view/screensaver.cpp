/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "view.h"
#include <cstdint>
#include <memory>
#include <esp_heap_caps.h>
#include <cstring>

using namespace view;
using namespace uitk;
using namespace uitk::lvgl_cpp;
using namespace uitk::games;
using namespace uitk::games::dvd_screensaver;

static const Vector2 _screen_size               = {320, 240};
static const Vector2 _logo_size                 = {64, 48};
static const int _logo_id                       = 666;
static const std::vector<uint32_t> _logo_colors = {
    0xffffff, 0xfffa01, 0xff8300, 0x00feff, 0xff2600, 0xbe00ff, 0x0026ff, 0xff008b,
};
static const uint32_t _bg_color = 0x000000;

Screensaver::~Screensaver()
{
    if (_prev_screen && isActiveScreen() && lv_obj_is_valid(_prev_screen->get())) _prev_screen->load();
    // Canvas must release its borrowed pixels before the PSRAM buffer is freed.
    if (_effect_canvas) { lv_obj_delete(_effect_canvas); _effect_canvas=nullptr; }
    if (_effect_pixels) { heap_caps_free(_effect_pixels); _effect_pixels=nullptr; }
}

bool Screensaver::isActiveScreen() const
{
    return _screen && lv_screen_active()==_screen->get();
}

void Screensaver::showEffectError()
{
    _effect_failed=true;
    if(_effect_canvas) lv_obj_add_flag(_effect_canvas,LV_OBJ_FLAG_HIDDEN);
    if(!_effect_error) {
        _effect_error=lv_label_create(_screen->get());
        lv_obj_set_width(_effect_error,280);
        lv_obj_set_style_text_color(_effect_error,lv_color_hex(0xffffff),0);
        lv_obj_center(_effect_error);
    }
    lv_label_set_text(_effect_error,"Effect unavailable\nTouch to return to launcher");
}

void Screensaver::onInit()
{
    _prev_screen = std::make_unique<ScreenActive>();

    _screen = std::make_unique<Screen>();
    _screen->setBgColor(lv_color_hex(_bg_color));
    _screen->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _screen->setPadding(0, 0, 0, 0);
    _screen->load();

    if (_style != saver::Style::Dvd) {
        const uint16_t index=uint16_t(static_cast<unsigned>(_style)-1);
        if(!effect_catalog::Usable(index)) { showEffectError(); return; }
        constexpr size_t bytes=320u*240u*sizeof(uint16_t);
        _effect_pixels=static_cast<uint16_t*>(heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if(!_effect_pixels) { showEffectError(); return; }
        std::memset(_effect_pixels,0,bytes);
        _effect_canvas=lv_canvas_create(_screen->get());
        lv_canvas_set_buffer(_effect_canvas,_effect_pixels,320,240,LV_COLOR_FORMAT_RGB565);
        lv_obj_set_pos(_effect_canvas,0,0);
        lv_obj_remove_flag(_effect_canvas,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        effect_catalog::Reset(index);
        onRender(0);
        return;
    }

    _logo = std::make_unique<Container>(_screen->get());
    _logo->setSize(_logo_size.width, _logo_size.height);
    _logo->setBgColor(lv_color_hex(_logo_colors[0]));
    _logo->align(LV_ALIGN_TOP_LEFT, 2333, 2333);
    _logo->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _logo->setPadding(0, 0, 0, 0);
    _logo->setBorderWidth(0);
    _logo->setRadius(0);

    _left_eye = std::make_unique<Container>(_logo->get());
    _left_eye->align(LV_ALIGN_CENTER, -15, -3);
    _left_eye->setBgColor(lv_color_hex(_bg_color));
    _left_eye->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _left_eye->setRadius(LV_RADIUS_CIRCLE);
    _left_eye->setBorderWidth(0);
    _left_eye->setSize(6, 6);

    _right_eye = std::make_unique<Container>(_logo->get());
    _right_eye->align(LV_ALIGN_CENTER, 15, -3);
    _right_eye->setBgColor(lv_color_hex(_bg_color));
    _right_eye->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _right_eye->setRadius(LV_RADIUS_CIRCLE);
    _right_eye->setBorderWidth(0);
    _right_eye->setSize(6, 6);

    _mouth = std::make_unique<Container>(_logo->get());
    _mouth->align(LV_ALIGN_CENTER, 0, 5);
    _mouth->setBgColor(lv_color_hex(_bg_color));
    _mouth->removeFlag(LV_OBJ_FLAG_SCROLLABLE);
    _mouth->setBorderWidth(0);
    _mouth->setSize(18, 2);
    _mouth->setRadius(0);
}

void Screensaver::onBuildLevel()
{
    if (_style != saver::Style::Dvd) return;
    addScreenFrameAsWall(_screen_size);

    auto& random      = Random::getInstance();
    Vector2 direction = {random.getFloat(0.3, 0.7), random.getFloat(0.3, 0.7)};
    direction         = direction.normalized();

    addLogo(_logo_id, {_screen_size.width / 2, _screen_size.height / 2}, _logo_size, direction, 110);
}

void Screensaver::onRender(float dt)
{
    if(!isActiveScreen())return;
    if (_style != saver::Style::Dvd) {
        if(_effect_failed || !_effect_canvas || !_effect_pixels) return;
        // Clamp stalled frame delta; only visual time is advanced.
        if (dt > 0 && dt < 1) _animation_ms += uint32_t(dt * 1000);
        const uint16_t index=uint16_t(static_cast<unsigned>(_style)-1);
        // Playback needs a complete frame. A wall-clock preview deadline can
        // expire while clearing PSRAM and must not disable a usable effect.
        // The renderer's finite work budget and clipped primitives still apply.
        if(!effect_catalog::Render(index,_animation_ms,_effect_pixels,320,240)) {
            showEffectError(); return;
        }
        lv_obj_invalidate(_effect_canvas);
        return;
    }
    getWorld().forEachObject([&](GameObject* obj) {
        if (obj->groupId == _logo_id) {
            auto p = obj->get<Transform>()->position;
            _logo->setPos((int)p.x - _logo_size.width / 2, (int)p.y - _logo_size.height / 2);
        }
    });
}

void Screensaver::onLogoCollide(int logoGroupId)
{
    _color_index++;
    if (_color_index >= _logo_colors.size()) {
        _color_index = 0;
    }

    _logo->setBgColor(lv_color_hex(_logo_colors[_color_index]));
}
