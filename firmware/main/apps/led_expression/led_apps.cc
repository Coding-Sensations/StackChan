#include "led_apps.h"
#include "led_service_api.h"
#include <assets/assets.h>
#include <apps/common/tool_visual.hpp>
#include "led_visual_model.hpp"
#include <hal/hal.h>
#include <lvgl.h>
#include <mutex>
#include <string>
#include <vector>
namespace stock_tools {
namespace {
class LedApp final : public mooncake::AppAbility {
  struct Callback {
    LedApp *app;
    int action;
  };
  std::vector<std::unique_ptr<Callback>> callbacks_;
  std::mutex mutex_;
  int action_ = 0;
  lv_obj_t *root_ = nullptr, *label_ = nullptr;
  std::string shown_;
  unsigned palette_ = 0;
  bool details_ = false;
  std::array<lv_obj_t*, 12> pixels_{};
  static void Click(lv_event_t *e) {
    auto *c = static_cast<Callback *>(lv_event_get_user_data(e));
    std::lock_guard<std::mutex> lock(c->app->mutex_);
    if (!c->app->action_)
      c->app->action_ = c->action;
  }
  void Button(const char *text, int x, int y, int w, int action) {
    auto b = stock_ui::Button(root_, text, x, y, w, 40);
    auto c = std::make_unique<Callback>();
    c->app = this;
    c->action = action;
    lv_obj_add_event_cb(b, Click, LV_EVENT_CLICKED, c.get());
    callbacks_.push_back(std::move(c));
  }
  void Build() {
    LvglLockGuard lock;
    lv_obj_clean(root_); callbacks_.clear(); pixels_.fill(nullptr);
    stock_ui::Page(root_);
    Button("Back", 4, 4, 68, 1);
    stock_ui::Text(root_, "LED expressions", 82, 14, 150);
    Button(details_ ? "Preview" : "Details", 238, 4, 78, 9);
    auto card = stock_ui::Card(root_, 8, 50, 304, 100);
    if(details_) {
      lv_obj_add_flag(card, LV_OBJ_FLAG_SCROLLABLE);
      label_ = stock_ui::Text(card, "", 8, 6, 288, true);
    } else {
      for(unsigned i=0;i<12;++i) {
        auto p=lv_obj_create(card); pixels_[i]=p;
        auto g=led_visual::Pixel(i);
        lv_obj_set_pos(p,g.x,g.y);lv_obj_set_size(p,g.w,g.h);
        lv_obj_remove_flag(p,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(p,12,0);lv_obj_set_style_border_width(p,1,0);
        lv_obj_set_style_border_color(p,lv_color_hex(stock_ui::Muted),0);
      }
      label_=stock_ui::Text(card,"",8,70,288,true);
    }
    Button("Off",4,156,72,2);Button("25%",82,156,72,3);
    Button("50%",160,156,72,4);Button("100%",238,156,78,5);
    Button("Expressions",4,198,100,6);Button("Color",110,198,100,8);
    Button("Retry",216,198,100,7);shown_.clear();
  }
  void Update() {
    auto preview=led_expression::ReadCachedPreview();
    std::string text=led_visual::Status(preview);
    if(details_) text += "\nController cache only; no optical readback.\nStatus overrides color/expression; Off wins.\nMira custom LED scope is unavailable.\nI2C failure leaves output unknown; Retry is explicit.";
    LvglLockGuard lock;
    if(!details_) for(unsigned i=0;i<12;++i) {
      lv_obj_set_style_bg_color(pixels_[i],lv_color_hex(led_visual::PixelColor(preview,i)),0);
      lv_obj_set_style_border_color(pixels_[i],lv_color_hex(preview.result==led_expression::Result::IoFault?stock_ui::Warning:stock_ui::Muted),0);
    }
    if(text!=shown_) {shown_=text;lv_label_set_text(label_,text.c_str());}
  }

public:
  LedApp() {
    setAppInfo().name = "LED EXPRESSIONS";
    static auto icon = assets::get_image("icon_ai_agent.bin");
    setAppInfo().icon = &icon;
  }
  void onOpen() override {
    {
      std::lock_guard<std::mutex> l(mutex_);
      action_ = 0;
    }
    shown_.clear();
    {
      LvglLockGuard l;
      root_ = lv_obj_create(lv_screen_active());
      lv_obj_set_pos(root_, 0, 0);
      lv_obj_set_size(root_, 320, 240);
      lv_obj_set_style_pad_all(root_, 0, 0);
    }
    details_=false;Build();
    Update();
  }
  void onRunning() override {
    int a = 0;
    {
      std::lock_guard<std::mutex> l(mutex_);
      a = action_;
      action_ = 0;
    }
    if (a == 1) {
      close();
      return;
    }
    if (a == 9) { details_ = !details_; Build(); }
    if (a == 2)
      led_expression::SetOff(true);
    if (a >= 3 && a <= 5) {
      led_expression::SetBrightness(a == 3 ? 25 : a == 4 ? 50 : 100);
      led_expression::SetOff(false);
    }
    if (a == 6) {
      led_expression::ReleaseManualColor();
      led_expression::SetOff(false);
    }
    if (a == 7)
      led_expression::RetryOutput();
    if (a == 8) {
      const led_expression::Color colors[] = {
          {100, 80, 0}, {40, 80, 140}, {40, 120, 60}};
      led_expression::SetManualColor(colors[palette_++ % 3]);
      led_expression::SetOff(false);
    }
    Update();
  }
  void onClose() override {
    LvglLockGuard l;
    if (root_)
      lv_obj_delete(root_);
    root_ = label_ = nullptr;
    callbacks_.clear();
  }
};
} // namespace
std::unique_ptr<mooncake::AppAbility> MakeLedExpressionApp() {
  return std::make_unique<LedApp>();
}
} // namespace stock_tools
