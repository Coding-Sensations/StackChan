#ifndef STACKCHAN_BUILD_MUSE
#define STACKCHAN_BUILD_MUSE 1
#endif
#include "provider_companion_app.h"
#include <apps/common/home_indicator/home_indicator.h>
#include <apps/common/tool_visual.hpp>
#include <assets/assets.h>
#include <atomic>
#include <companion/provider_room_runtime.h>
#if STACKCHAN_BUILD_MUSE
#include <companion/muse_room_transport.h>
#include <companion/muse_direct_device.h>
#include <companion/muse_direct_live_view.h>
#include <companion/muse_voice_task.h>
#include <companion/muse_direct_local_ui.hpp>
#include <companion/muse_direct_local_port.h>
#endif
#include <companion/provider_room_view.hpp>
#include <companion/companion_adapter_contract.hpp>
#include <esp_timer.h>
#include <hal/hal.h>

namespace stock_tools {
namespace {
class ProviderSetupApp final : public mooncake::AppAbility {
  const companion_modes::Descriptor &provider_;
  lv_obj_t *root_ = nullptr, *settings_ = nullptr, *connect_ = nullptr, *back_button_ = nullptr;
  companion::room::View room_;
  std::atomic<bool> back_{false}, toggle_{false}, connect_requested_{false};
  bool showing_settings_ = false;
#if STACKCHAN_BUILD_MUSE
  lv_obj_t *direct_pair_ = nullptr, *direct_confirm_ = nullptr, *direct_connect_ = nullptr;
#if STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
  lv_obj_t* direct_voice_=nullptr;
  std::atomic<bool> voice_requested_{false};
#endif
  std::unique_ptr<companion::muse::LocalSetupUi> local_setup_;
  std::atomic<bool> native_requested_{false};
  std::atomic<bool> pair_requested_{false};
  std::atomic<uint32_t> displayed_ticket_{0}, press_requested_{0};

#endif
public:
  explicit ProviderSetupApp(const companion_modes::Descriptor &provider)
      : provider_(provider) {
    setAppInfo().name = provider_.label;
    static auto icon = assets::get_image("icon_ai_agent.bin");
    setAppInfo().icon = &icon;
  }
  void onOpen() override {
    companion::room::SelectProvider(provider_.provider);
    LvglLockGuard lock;
    back_ = toggle_ = connect_requested_ = false;
    showing_settings_ = false;
#if STACKCHAN_BUILD_MUSE
    native_requested_=false;
#if STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
    voice_requested_=false;
#endif
    pair_requested_ = false;displayed_ticket_=press_requested_=0;
    if(provider_.provider==companion_modes::Provider::Muse)companion::muse::DeviceProviderVisible(true);
#endif
    root_ = lv_obj_create(lv_screen_active());
    stock_ui::Page(root_);
    lv_obj_set_size(root_, lv_obj_get_width(lv_screen_active()),
                    lv_obj_get_height(lv_screen_active()));
    // LVGL sizes are deferred; resolve the root before room/controls read it.
    lv_obj_update_layout(root_);
    room_.Open(root_, provider_.provider);
    room_.SetSubmit([provider = provider_.provider](const std::string &choice,
                                                    const std::string &ticket) {
      return companion::room::QueueDecision(
          provider, choice, ticket, uint64_t(esp_timer_get_time() / 1000));
    });
    room_.Render({}, 0);
    auto settings = stock_ui::Button(root_, "Settings",
                                     lv_obj_get_width(root_) - 88, 8, 80, 36);
    lv_obj_add_event_cb(
        settings,
        [](lv_event_t *e) {
          static_cast<ProviderSetupApp *>(lv_event_get_user_data(e))->toggle_ =
              true;
        },
        LV_EVENT_CLICKED, this);
    auto privacy = stock_ui::Text(root_, "Audio/camera status unverified", 8,
                                  lv_obj_get_height(root_) - 24,
                                  lv_obj_get_width(root_) - 16, true);
    lv_label_set_long_mode(privacy, LV_LABEL_LONG_DOT);
    settings_ = stock_ui::Card(root_, 8, 44, 304, 168);
    const auto readiness = companion::adapter::Describe(provider_.provider);
    auto text = stock_ui::Text(settings_, readiness.setup, 8, 8, 288);
    lv_obj_set_height(text, 100);
    lv_label_set_long_mode(text, LV_LABEL_LONG_WRAP);
    auto back = stock_ui::Button(settings_, "Back", 8, 120, 288, 36);
    lv_obj_add_event_cb(
        back,
        [](lv_event_t *e) {
          static_cast<ProviderSetupApp *>(lv_event_get_user_data(e))->toggle_ =
              true;
        },
        LV_EVENT_CLICKED, this);
    back_button_ = back;
#if STACKCHAN_BUILD_MUSE
    if (provider_.provider == companion_modes::Provider::Muse) {
      connect_ = stock_ui::Button(settings_, "Connect Muse", 8, 120, 140, 36);
      lv_obj_add_event_cb(connect_, [](lv_event_t *e) {
        static_cast<ProviderSetupApp *>(lv_event_get_user_data(e))->connect_requested_ = true;
      }, LV_EVENT_CLICKED, this);
      lv_obj_add_flag(connect_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_height(text, 30);
      auto local = stock_ui::Button(settings_, "Private Muse setup", 8, 42, 288, 32);
      lv_obj_add_event_cb(local, [](lv_event_t* e) {
        auto* self=static_cast<ProviderSetupApp*>(lv_event_get_user_data(e));
        if(!self->local_setup_)self->local_setup_=std::make_unique<companion::muse::LocalSetupUi>(companion::muse::DeviceLocalPort());
        self->local_setup_->Open(self->root_);
      }, LV_EVENT_CLICKED, this);
      direct_pair_ = stock_ui::Button(settings_, "Pair Muse device", 8, 80, 288, 36);
      lv_obj_add_event_cb(direct_pair_, [](lv_event_t *e) {
        static_cast<ProviderSetupApp *>(lv_event_get_user_data(e))->pair_requested_ = true;
      }, LV_EVENT_CLICKED, this);
      direct_confirm_ = stock_ui::Button(settings_, "Confirm Muse pairing", 8, 80, 288, 36);
      lv_obj_add_event_cb(direct_confirm_, [](lv_event_t *e) {
        auto* self=static_cast<ProviderSetupApp *>(lv_event_get_user_data(e));
        self->press_requested_=self->displayed_ticket_.load();
      }, LV_EVENT_CLICKED, this);
#if STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
      direct_voice_ = stock_ui::Button(settings_, "Start prepared voice", 8, 80, 288, 36);
      lv_obj_add_event_cb(direct_voice_, [](lv_event_t* e) {
        static_cast<ProviderSetupApp*>(lv_event_get_user_data(e))->voice_requested_=true;
      }, LV_EVENT_CLICKED, this);
      lv_obj_add_flag(direct_voice_, LV_OBJ_FLAG_HIDDEN);
#endif
      direct_connect_ = stock_ui::Button(settings_, "Connect direct Muse", 8, 80, 288, 36);
      lv_obj_add_event_cb(direct_connect_, [](lv_event_t *e) {
        static_cast<ProviderSetupApp *>(lv_event_get_user_data(e))->native_requested_ = true;
      }, LV_EVENT_CLICKED, this);
      lv_obj_add_flag(direct_connect_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(direct_pair_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(direct_confirm_, LV_OBJ_FLAG_HIDDEN);
    }
#endif
    lv_obj_add_flag(settings_, LV_OBJ_FLAG_HIDDEN);
    view::create_home_indicator([this] { back_ = true; }, 1000, 200,
                                lv_layer_top(), false);
  }
  void onRunning() override {
    if (back_.exchange(false)) {
      close();
      return;
    }
#if STACKCHAN_BUILD_MUSE
    if (provider_.provider == companion_modes::Provider::Muse) {
#if STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
      companion::muse::PollNativeVoiceTaskCleanup();
      if(voice_requested_.exchange(false)){
        if(companion::muse::NativeVoiceTaskPrepared())companion::muse::StartPreparedNativeVoiceTask();
        else companion::muse::QueueNativeVoiceReview();
        showing_settings_=false;
      }
#endif
      if(native_requested_.exchange(false)&&companion::muse::StartPreparedNativeSession())showing_settings_=false;
      if(pair_requested_.exchange(false))companion::muse::StartPreparedDevicePairing();
      if(auto captured=press_requested_.exchange(0))companion::muse::QueueDevicePhysicalPress(captured);
      displayed_ticket_=companion::muse::DevicePhysicalTicket();
      if(displayed_ticket_)showing_settings_=true;
      if (connect_requested_.exchange(false))
        companion::room::ConnectPreparedMuseRoom();
      companion::room::PollMuseRoom();
    }
#endif
    auto now = uint64_t(esp_timer_get_time() / 1000);
    auto snapshot = companion::room::ViewProvider(provider_.provider, now);
#if STACKCHAN_BUILD_MUSE
    auto native=provider_.provider==companion_modes::Provider::Muse?companion::muse::ViewNativeSession():companion::muse::SessionSnapshot{};
    const bool native_prepared=provider_.provider==companion_modes::Provider::Muse&&companion::muse::NativeSessionPrepared();
    if(native.stage!=companion::muse::SessionStage::Off){
      snapshot={};
      if(native.stage==companion::muse::SessionStage::Ready||native.stage==companion::muse::SessionStage::ReplyEnded||(native.stage==companion::muse::SessionStage::Closing&&native.response_complete)){snapshot.verified=true;snapshot.scene=(native.reply_ended||native.response_complete)?"muse_reply_ended":native.communicating?"muse_text_pending":"muse_text_ready";snapshot.caption=native.caption;snapshot.communication=native.communicating?companion::room::Communication::TextPending:companion::room::Communication::Idle;}
    }
#endif
    LvglLockGuard lock;
#if STACKCHAN_BUILD_MUSE
    if(local_setup_)local_setup_->Poll();
    if (connect_) {
      const bool prepared = native.stage==companion::muse::SessionStage::Off&&!native_prepared&&companion::room::MuseRoomPrepared();
      if (prepared) lv_obj_remove_flag(connect_, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(connect_, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_pos(back_button_, prepared ? 156 : 8, 120);
      lv_obj_set_size(back_button_, prepared ? 140 : 288, 36);
    }
#if STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
    if(direct_voice_){
      const auto voice=companion::muse::ViewNativeVoiceTask();
      const bool prepared=voice.stage==companion::muse::VoiceTaskStage::Prepared;
      const bool review=voice.stage==companion::muse::VoiceTaskStage::Reviewing||voice.stage==companion::muse::VoiceTaskStage::DirectionsPrepared;
      if(prepared||review){lv_label_set_text(lv_obj_get_child(direct_voice_,0),prepared?"Start prepared voice":"Show voice review");lv_obj_remove_flag(direct_voice_,LV_OBJ_FLAG_HIDDEN);}
      else lv_obj_add_flag(direct_voice_,LV_OBJ_FLAG_HIDDEN);
    }
#endif
    if(direct_connect_){if(native_prepared)lv_obj_remove_flag(direct_connect_,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(direct_connect_,LV_OBJ_FLAG_HIDDEN);}
    if(direct_pair_){
      if(companion::muse::DevicePairingPrepared()&&!displayed_ticket_)lv_obj_remove_flag(direct_pair_,LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(direct_pair_,LV_OBJ_FLAG_HIDDEN);
      if(displayed_ticket_)lv_obj_remove_flag(direct_confirm_,LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(direct_confirm_,LV_OBJ_FLAG_HIDDEN);
    }
#endif
    if (toggle_.exchange(false))
      showing_settings_ = !showing_settings_;
    if (snapshot.verified && snapshot.scene == "decision_sign")
      showing_settings_ = false;
    if (showing_settings_)
      lv_obj_remove_flag(settings_, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_add_flag(settings_, LV_OBJ_FLAG_HIDDEN);
    room_.Visible(!showing_settings_);
    room_.Render(snapshot, now);
  }
  void onClose() override {
#if STACKCHAN_BUILD_MUSE
    if (provider_.provider == companion_modes::Provider::Muse){
      companion::muse::DeviceProviderVisible(false);
      companion::room::StopMuseRoom();
    }
#endif
    companion::room::DisconnectProvider(provider_.provider);
    LvglLockGuard lock;
    view::destroy_home_indicator();
#if STACKCHAN_BUILD_MUSE
    if(local_setup_){local_setup_->Close();local_setup_.reset();}
#endif
    room_.Detach();
    if (root_)
      lv_obj_delete(root_);
    root_ = settings_ = connect_ = back_button_ = nullptr;
#if STACKCHAN_BUILD_MUSE
    direct_pair_ = direct_confirm_ = direct_connect_ = nullptr;
#if STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
    direct_voice_=nullptr;
#endif
    native_requested_=false;
#if STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
    voice_requested_=false;
#endif
    pair_requested_=false;displayed_ticket_=press_requested_=0;
#endif
    back_ = toggle_ = connect_requested_ = false;
  }
};
} // namespace
std::unique_ptr<mooncake::AppAbility>
MakeProviderCompanionApp(companion_modes::Provider provider) {
#if !STACKCHAN_BUILD_MUSE
  if(provider==companion_modes::Provider::Muse)return {};
#endif
  const auto *item = companion_modes::Find(provider);
  if (!item || item->route != companion_modes::Route::AdapterRequired)
    return {};
  return std::make_unique<ProviderSetupApp>(*item);
}
} // namespace stock_tools
