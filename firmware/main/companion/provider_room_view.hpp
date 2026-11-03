#ifndef STACKCHAN_BUILD_MUSE
#define STACKCHAN_BUILD_MUSE 1
#endif
#pragma once
#include "mira_character_image.hpp"
#include "companion_presence.hpp"
#include "companion_portraits.hpp"
#include "provider_room_state.hpp"
#include <apps/stock_shell/provider_catalog.hpp>
#include <functional>
#include <lvgl.h>
namespace companion::room {
// Original lightweight room primitives. No donor art, audio, hardware actions
// or worker thread; owner serializes calls under the existing LVGL lock.
class View {
  lv_obj_t *root_ = nullptr, *wall_ = nullptr, *avatar_ = nullptr,
           *status_ = nullptr, *activity_ = nullptr, *screen_ = nullptr,
           *lamp_ = nullptr, *sign_ = nullptr, *prompt_ = nullptr,
           *summary_ = nullptr, *approve_ = nullptr, *deny_ = nullptr;
  lv_obj_t *caption_ = nullptr;
  companion_modes::Provider provider_ = companion_modes::Provider::Mira;
  Snapshot snapshot_;
  #if STACKCHAN_LOCAL_PORTRAITS
  desk::LocalPortrait portrait_{};
  #endif
  lv_obj_t* face_=nullptr;Presence presence_;uint8_t face_amount_=0;
  std::string pressed_, spent_, last_scene_;
  uint64_t completion_started_ = 0;
  bool visible_ = false;
  int side_ = 0;
  unsigned wall_color_ = 0;
  std::function<bool(const std::string &, const std::string &)> submit_;
  int Scale(int n) const { return n * side_ / 216; }
  lv_obj_t *Box(lv_obj_t *parent, int x, int y, int w, int h, unsigned color,
                int radius = 0) {
    auto p = lv_obj_create(parent);
    lv_obj_remove_style_all(p);
    lv_obj_set_pos(p, Scale(x), Scale(y));
    lv_obj_set_size(p, Scale(w), Scale(h));
    lv_obj_remove_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(p, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(p, 255, 0);
    lv_obj_set_style_radius(p, radius, 0);
    return p;
  }
  lv_obj_t *Text(lv_obj_t *parent, const char *text, int x, int y, int w,
                 int h = 20) {
    auto p = lv_label_create(parent);
    lv_obj_set_pos(p, Scale(x), Scale(y));
    lv_obj_set_size(p, Scale(w), Scale(h));
    lv_obj_set_style_text_font(p, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(p, lv_color_hex(0xffffff), 0);
    lv_label_set_long_mode(p, LV_LABEL_LONG_DOT);
    lv_label_set_text(p, text);
    return p;
  }
  const lv_image_dsc_t* Portrait()const {
    if(provider_==companion_modes::Provider::Mira)return desk::MiraCharacterImage();
    #if STACKCHAN_LOCAL_PORTRAITS
    return portrait_.image;
    #else
    return nullptr;
    #endif
  }
  static void Show(lv_obj_t *p, bool yes) {
    if (!p)
      return;
    if (yes)
      lv_obj_remove_flag(p, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_add_flag(p, LV_OBJ_FLAG_HIDDEN);
  }
  static void Decision(lv_event_t *event) {
    auto *self = static_cast<View *>(lv_event_get_user_data(event));
    // A queued event from a retired surface must not acquire a new ticket.
    auto* target=lv_event_get_target(event);
    if(target!=self->approve_&&target!=self->deny_)return;
    if (!self->visible_ || !self->snapshot_.verified ||
        self->snapshot_.scene != "decision_sign")
      return;
    if (lv_event_get_code(event) == LV_EVENT_PRESSED) {
      self->pressed_ = self->snapshot_.ticket;
      return;
    }
    if (lv_event_get_code(event) != LV_EVENT_CLICKED)
      return;
    auto ticket = std::move(self->pressed_);
    self->pressed_.clear();
    if (ticket.empty() || ticket != self->snapshot_.ticket ||
        ticket == self->spent_ || !self->submit_)
      return;
    self->spent_ = ticket;
    lv_obj_add_state(self->approve_, LV_STATE_DISABLED);
    lv_obj_add_state(self->deny_, LV_STATE_DISABLED);
    bool sent = self->submit_(
        lv_event_get_target(event) == self->approve_ ? "approve" : "deny",
        ticket);
    lv_label_set_text(self->summary_,
                      sent ? "Decision queued; waiting for source receipt."
                           : "Decision not sent. Check the source app.");
  }

public:
  void SetSubmit(
      std::function<bool(const std::string &, const std::string &)> submit) {
    submit_ = std::move(submit);
  }
  void Detach() {
    root_ = wall_ = avatar_ = status_ = activity_ = screen_ = lamp_ = sign_ =
        prompt_ = summary_ = approve_ = deny_ = nullptr;
    #if STACKCHAN_LOCAL_PORTRAITS
    portrait_={};
    #endif
    caption_ = nullptr;face_=nullptr;presence_.Reset();face_amount_=0;
    visible_ = false;
    pressed_.clear();
    spent_.clear();
    snapshot_ = {};
    submit_ = {};
  }
  lv_obj_t *Root() const { return root_; }
  void Open(lv_obj_t *parent, companion_modes::Provider provider) {
    if (int(provider) < 0 || int(provider) >= 6 || !companion_modes::Find(provider))
      return;
    provider_ = provider;
    #if STACKCHAN_LOCAL_PORTRAITS
    portrait_=desk::LocalPortraitEntry(provider);
    #endif
    side_ = lv_obj_get_height(parent) - 24;
    if (side_ > lv_obj_get_width(parent))
      side_ = lv_obj_get_width(parent);
    if (side_ < 160)
      return;
    root_ = lv_obj_create(parent);
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, side_, side_);
    lv_obj_set_pos(root_, (lv_obj_get_width(parent) - side_) / 2, 0);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    const unsigned walls[] = {0x173344, 0x3c2947, 0x3c3321,
                              0x2b2154, 0x3b302b, 0x152a3e};
    unsigned wall = walls[int(provider_)];
    wall_color_ = wall;
    wall_ = Box(root_, 0, 0, 216, 216, wall);
    Box(root_, 0, 144, 216, 72, wall + 0x121212);
    Box(root_, 10, 32, 52, 54, 0x68a0b7, 4);
    Box(root_, 33, 32, 3, 54, 0x243e4c);
    Box(root_, 10, 57, 52, 3, 0x243e4c);
    Box(root_, 139, 51, 64, 48, 0x152228, 3);
    screen_ = Box(root_, 144, 56, 54, 36, 0x498877, 2);
    Box(root_, 166, 99, 8, 9, 0x243746);
    Box(root_, 122, 110, 84, 9, 0x96724b, 2);
    Box(root_, 130, 119, 6, 41, 0x71523d);
    Box(root_, 193, 119, 6, 41, 0x71523d);
    Box(root_, 9, 163, 66, 27, 0x5a6f81, 4);
    Box(root_, 9, 157, 22, 11, 0xddd6c3, 3);
    Box(root_, 11, 190, 5, 12, 0x6d4a32);
    Box(root_, 65, 190, 5, 12, 0x6d4a32);
    Box(root_, 184, 164, 17, 23, 0xaa7255, 3);
    Box(root_, 190, 136, 5, 30, 0x487a54);
    Box(root_, 178, 142, 16, 10, 0x71a86f, 7);
    Box(root_, 193, 132, 15, 10, 0x80b87a, 7);
    lamp_ = Box(root_, 100, 44, 19, 12, 0xf4d98a, 8);
    Box(root_, 108, 0, 2, 44, 0x8c9ba0);
    // Props differentiate worlds, while provider-specific full sprite packs
    // remain replaceable. Only Mira uses its exact established image,
    // unchanged.
    if (provider_ == companion_modes::Provider::Hermes ||
        provider_ == companion_modes::Provider::Claude) {
      for (int n = 0; n < 5; ++n)
        Box(root_, 77 + n * 7, 74, 5, 21, 0xb88655 + n * 0x050602);
      Box(root_, 72, 96, 44, 4, 0x9a754a);
    }
#if STACKCHAN_BUILD_MUSE
    if (provider_ == companion_modes::Provider::Muse) {
      Text(root_, "music", 76, 74, 50);
      Box(root_, 84, 102, 24, 24, 0x8f70a0, 12);
    }
#endif
    if (provider_ == companion_modes::Provider::Codex) {
      Text(root_, ">_", 150, 65, 40);
      Box(root_, 123, 123, 78, 3, 0x5da5a6);
    }
    avatar_ = lv_obj_create(root_);
    lv_obj_remove_style_all(avatar_);
    lv_obj_set_size(avatar_, Scale(86), Scale(66));
    lv_obj_set_pos(avatar_, Scale(58), Scale(95));
    lv_obj_remove_flag(avatar_, LV_OBJ_FLAG_SCROLLABLE);
    if (provider_ == companion_modes::Provider::Mira) {
      auto image = lv_image_create(avatar_);
      lv_image_set_src(image, desk::MiraCharacterImage());
      lv_image_set_pivot(image, 160, 120);
      lv_image_set_scale(image, Scale(80) * 256 / 320);
      lv_obj_center(image);
    } else if (const auto* portrait=Portrait()) {
      auto image=lv_image_create(avatar_);lv_image_set_src(image,portrait);
      lv_image_set_pivot(image,portrait->header.w/2,portrait->header.h/2);
      lv_image_set_scale(image,Scale(64)*256/portrait->header.h);lv_image_set_antialias(image,false);lv_obj_center(image);
    } else if(provider_==companion_modes::Provider::Hermes||provider_==companion_modes::Provider::Muse||provider_==companion_modes::Provider::Claude||provider_==companion_modes::Provider::Codex){
      Text(avatar_,"Portrait not configured",0,12,86,50);
    } else {
      unsigned colors[] = {0xafd6e5, 0xe8e9dd, 0xe5c184,
                           0xc8a2ec, 0xe8b19a, 0x93d8d2};
      Box(avatar_, 12, 4, 58, 53, colors[int(provider_)], 18);
      Box(avatar_, 25, 21, 6, 9, 0x172833, 3);
      Box(avatar_, 49, 21, 6, 9, 0x172833, 3);
      Box(avatar_, 34, 39, 11, 3, 0x355463, 2);
    }
    Text(root_, companion_modes::Find(provider_)->label, 8, 5, 130, 18);
    activity_ = Text(root_, "", 8, 188, 200);
    status_ = Text(root_, "Not connected", 8, 24, 140, 18);
    lv_obj_set_style_bg_color(status_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(status_, 190, 0);
    sign_ = Text(root_, "", 80, 154, 124, 30);
    if (provider_ == companion_modes::Provider::Muse) {
      caption_ = Text(root_, "", 8, 112, 200, 70);
      lv_label_set_long_mode(caption_, LV_LABEL_LONG_WRAP);
      lv_obj_set_style_bg_color(caption_, lv_color_hex(0x17242f), 0);
      lv_obj_set_style_bg_opa(caption_, 240, 0);
      Show(caption_, false);
    }
    prompt_ = Box(root_, 0, 0, 216, 216, 0x17242f, 4);
    Text(prompt_, "Decision from source", 8, 8, 200);
    summary_ = Text(prompt_, "", 8, 40, 200, 116);
    lv_label_set_long_mode(summary_, LV_LABEL_LONG_WRAP);
    approve_ = lv_button_create(prompt_);
    lv_obj_set_pos(approve_, Scale(8), Scale(168));
    lv_obj_set_size(approve_, Scale(96), Scale(40));
    auto yes = lv_label_create(approve_);
    lv_label_set_text(yes, "Approve");
    lv_obj_center(yes);
    deny_ = lv_button_create(prompt_);
    lv_obj_set_pos(deny_, Scale(112), Scale(168));
    lv_obj_set_size(deny_, Scale(96), Scale(40));
    auto no = lv_label_create(deny_);
    lv_label_set_text(no, "Deny");
    lv_obj_center(no);
    for (auto button : {approve_, deny_}) {
      lv_obj_add_event_cb(button, Decision, LV_EVENT_PRESSED, this);
      lv_obj_add_event_cb(button, Decision, LV_EVENT_CLICKED, this);
    }
    Show(prompt_, false);
    // Sibling viewport fills the actual display width, not the square room.
    face_=lv_obj_create(parent);lv_obj_remove_style_all(face_);
    lv_obj_set_size(face_,lv_obj_get_width(parent),lv_obj_get_height(parent)-24);
    lv_obj_remove_flag(face_,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(face_,lv_color_hex(0x10141c),0);lv_obj_set_style_bg_opa(face_,255,0);
    if(const auto* source=Portrait()){
      auto portrait=lv_image_create(face_);lv_image_set_src(portrait,source);
      // Explicit local face area, or the complete image when unspecified.
      // Identity/geometry was validated once at Open; rendering never re-queries
      // an external adapter or assumes coordinates for a particular mascot.
      #if STACKCHAN_LOCAL_PORTRAITS
      const auto& area=portrait_.face;
      #else
      const desk::FaceArea area{};
      #endif
      int x=0,y=0,w=source->header.w,h=source->header.h;
      if(area.width&&area.height){x=area.x;y=area.y;w=area.width;h=area.height;}
      const int px=x+w/2,py=y+h/2;
      const int sx=lv_obj_get_width(parent)*256/w;
      const int sy=(lv_obj_get_height(parent)-24)*256/h;
      const int scale=sx<sy?sx:sy;
      lv_image_set_pivot(portrait,px,py);lv_image_set_scale(portrait,scale);
      lv_image_set_antialias(portrait,false);
      lv_obj_set_pos(portrait,lv_obj_get_width(parent)/2-px,(lv_obj_get_height(parent)-24)/2-py);
    }else{
      std::string missing=std::string(companion_modes::Find(provider_)->label)+" portrait not configured";
      auto label=Text(face_,missing.c_str(),8,80,200,50);lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);lv_obj_align(label,LV_ALIGN_CENTER,0,0);
    }
    std::string title=std::string(companion_modes::Find(provider_)->label)+" conversation";
    Text(face_,title.c_str(),8,5,200);
    presence_.Reset();face_amount_=0;Show(face_,false);
    visible_ = true;
  }
  void Visible(bool show) {
    visible_ = show;
    Show(root_, show);
    if(!show){presence_.Reset();face_amount_=0;}Show(face_,show&&face_amount_);
    if (!show)
      pressed_.clear();
  }
  void Render(const Snapshot &value, uint64_t now) {
    if (!root_)
      return;
    if (value.ticket != snapshot_.ticket)
      pressed_.clear();
    snapshot_ = value;
    const bool valid = value.verified;
    const std::string state = valid ? value.scene : "unknown";
    if (state == "completion" && last_scene_ != "completion")
      completion_started_ = now;
    last_scene_ = state;
    const bool request = visible_ && valid && state == "decision_sign" &&
                         !value.ticket.empty() && !value.summary.empty() &&
                         bool(submit_);
    Show(prompt_, request);
    // Ready/connected and background desk work never imply communication.
    const bool communicating=value.communication!=Communication::Idle;
    face_amount_=presence_.Update(now,valid||value.local_communication_current,communicating,!visible_||state=="decision_sign");
    Show(face_,visible_&&face_amount_);if(face_)lv_obj_set_style_opa(face_,face_amount_,0);
#if STACKCHAN_BUILD_MUSE
    const bool has_caption = visible_ && valid && !request &&
        provider_ == companion_modes::Provider::Muse && !value.caption.empty();
    Show(caption_, has_caption);
    if (caption_)
      lv_label_set_text(caption_, has_caption ? value.caption.c_str() : "");
#endif
    if (request) {
      if (spent_ != value.ticket) {
        lv_obj_remove_state(approve_, LV_STATE_DISABLED);
        lv_obj_remove_state(deny_, LV_STATE_DISABLED);
        lv_label_set_text(summary_, value.summary.c_str());
      }
      return;
    }
    lv_obj_set_style_bg_color(wall_, lv_color_hex(wall_color_), 0);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x498877), 0);
    const char *caption = "Activity unavailable";
    int x = 58, y = 95;
#if STACKCHAN_BUILD_MUSE
    const bool muse_ready=provider_==companion_modes::Provider::Muse&&state=="muse_text_ready";
    const bool muse_reply=provider_==companion_modes::Provider::Muse&&state=="muse_reply_ended";
    if(communicating)caption="Waiting for reply";
    else if(muse_ready)caption="Ready for text";
    else if(muse_reply)caption="Reply ended";
    else
#endif
    if (state == "desk_work") {
      caption = "Working";
      x = 112;
      y = 103;
      lv_obj_set_style_bg_color(
          screen_, lv_color_hex((now / 180) % 2 ? 0x6dbcb0 : 0x498877), 0);
    } else if (state == "decision_sign")
      caption = "Decision waiting in source app";
    else if (state == "waiting_receipt")
      caption = "Waiting for source receipt";
    else if (state == "error") {
      caption = "Needs attention";
      x = 79;
      y = 102;
    } else if (state == "completion") {
      caption = "Completed";
      bool dancing =
          now >= completion_started_ && now - completion_started_ < 2000;
      x = 66 + (dancing ? int((now / 120) % 3) * 6 : 0);
      y = 93 - (dancing ? int((now / 120) % 2) * 5 : 0);
    } else if (state == "idle_decoration") {
      unsigned hobby = unsigned(now / 8000) % 3;
      caption = hobby == 0   ? "Resting"
                : hobby == 1 ? "Reading (decoration)"
                             : "Tending plants (decoration)";
      x = hobby == 2 ? 126 : 42;
      y = hobby == 0 ? 128 : 100;
      lv_obj_set_style_bg_color(
          wall_,
          lv_color_hex(hobby == 0 ? ((wall_color_ & 0xfefefe) >> 1)
                                  : wall_color_),
          0);
    }
    lv_obj_set_pos(avatar_, Scale(x), Scale(y));
    lv_label_set_text(activity_, caption);
#if STACKCHAN_BUILD_MUSE
    if(muse_reply)lv_label_set_text(status_,"Reply received");
    else if(muse_ready)lv_label_set_text(status_,"Muse connected");
    else
#endif
    lv_label_set_text(status_,valid ? (state == "unknown" ? "Activity unknown" : "Source connected") : "Not connected");
    lv_label_set_text(sign_, state == "error"        ? "Help needed"
                             : state == "completion" ? "Done!"
                                                     : "");
  }
};
} // namespace companion::room
