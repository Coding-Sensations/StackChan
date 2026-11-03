// SPDX-License-Identifier: MIT
// Intentional feature-entry adapter for the pinned official Stock AI.AGENT lifecycle.
#include "stock_agent.h"
#include <apps/runtime_lifecycle/session.h>
#include <apps/runtime_lifecycle/cleanup.h>
#include <esp_timer.h>
#include <apps/stock_shell/navigation.h>
#include <apps/common/home_indicator/home_indicator.h>
#include <apps/network_setup/persistence_gate.h>
#include <wifi_manager.h>
#include <apps/app_ai_agent/app_ai_agent.h>
#include <companion/boot.h>
#include <companion/audio_owner.hpp>
#include <companion/settings_policy.hpp>
#include <hal/hal.h>
#include <hal/board/hal_bridge.h>
#include <stackchan/stackchan.h>
#include <application.h>
#include <board.h>
#include <audio_codec.h>
#include <esp_attr.h>
#include <esp_system.h>
#include <lvgl.h>
#include <atomic>

namespace stock_agent {
namespace {
std::atomic<bool> start_requested{false},slow_stop{false};
companion::AudioLease runtime_audio_lease;
std::atomic<bool> start_authorized{false};
RTC_NOINIT_ATTR uint32_t preview_cookie;
constexpr uint32_t preview_magic = 0x53544F50; // STOP: preview, never resume

void ReturnHome() {
    runtime_lifecycle::RequestHome();
    companion::RevokeAudio(runtime_audio_lease); // stop authorizing capture immediately
}

lv_obj_t* Label(lv_obj_t* parent, const char* value, int x, int y, int width) {
    auto text = lv_label_create(parent);
    lv_label_set_text(text, value);
    lv_obj_set_pos(text, x, y);
    lv_obj_set_width(text, width);
    return text;
}

class StockAgentApp : public mooncake::AppAbility {
    bool waiting_network_=false;
    lv_obj_t* panel_ = nullptr;
    lv_obj_t* notice_ = nullptr;
    std::atomic<int> action_{0};
    void Button(const char* text, int x, int y, int width, int action) {
        auto button = lv_button_create(panel_);
        lv_obj_set_pos(button, x, y);
        lv_obj_set_size(button, width, 44);
        auto label = lv_label_create(button);
        lv_label_set_text(label, text);
        lv_obj_center(label);
        // Store action on the button, and object lifetime in the app callback.
        lv_obj_set_user_data(button, reinterpret_cast<void*>(static_cast<intptr_t>(action)));
        lv_obj_add_event_cb(button, [](lv_event_t* e) {
            auto self = static_cast<StockAgentApp*>(lv_event_get_user_data(e));
            auto button = static_cast<lv_obj_t*>(lv_event_get_target(e));
            self->action_ = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(button)));
        }, LV_EVENT_CLICKED, this);
    }
    void ShowNotice(const char* text) {
        LvglLockGuard lock;
        if(panel_){lv_label_set_text(notice_,text);return;}
        panel_ = lv_obj_create(lv_screen_active());
        lv_obj_set_size(panel_, 320, 240);
        lv_obj_set_pos(panel_, 0, 0);
        lv_obj_set_style_pad_all(panel_, 0, 0);
        lv_obj_remove_flag(panel_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(panel_, lv_color_hex(0x10232d), 0);
        lv_obj_set_style_text_color(panel_, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(panel_, &lv_font_montserrat_16, 0);
        Label(panel_, "Stackchan", 10, 10, 300);
        Label(panel_, "Uses cloud AI, microphone/wake word, camera and motors.\nSwipe up from the bottom for Home.", 10, 43, 300);
        notice_ = Label(panel_, text, 10, 128, 300);
        Button("Back", 10, 192, 90, 1);
        if(!GetHAL().isAppConfiged()){Button("Settings",112,192,198,3);return;}
        Button("Retry",112,192,96,2);
        Button("Settings",214,192,96,3);
    }
public:
    StockAgentApp() {
        AppAiAgent stock;
        setAppInfo() = stock.getAppInfo();
        setAppInfo().name = "StackChan";
    }
    void onOpen() override {
        waiting_network_=false;action_ = GetHAL().isAppConfiged() ? 2 : 0;
        // The tile's existing launch checks run in onRunning. Do not paint a
        // setup/retry screen that will be destroyed on the successful handoff.
        if(!GetHAL().isAppConfiged())ShowNotice("Setup required: Settings > Setup & login.\nComplete Wi-Fi and account setup there.");
    }
    void onRunning() override {
        auto action = action_.exchange(0);
        if(!action&&waiting_network_)action=2;
        if (action == 1) {waiting_network_=false;close();return;}
        if (action == 3) {waiting_network_=false;stock_shell::RequestSettings();close();return;}
        if (action != 2) return;
        if (!GetHAL().isAppConfiged()) {
            waiting_network_=false;
            ShowNotice("Setup required: Settings > Setup & login.\nComplete Wi-Fi and account setup there.");
            return;
        }
        if (companion::CurrentAudioOwner() != companion::AudioOwner::None) {
            ShowNotice("Audio is still stopping.\nRetry when stopped, or return Home.");
            return;
        }
        if (!runtime_lifecycle::PrepareVendorNetwork()) {
            waiting_network_=true;
            ShowNotice("Stopping previous Wi-Fi.\nWaiting for confirmed cleanup.\nBack cancels opening Stackchan.");
            return;
        }
        waiting_network_=false;start_requested = true;
    }
    void onClose() override {
        action_ = 0;
        LvglLockGuard lock;
        if (panel_) lv_obj_delete(panel_);
        panel_ = nullptr;
        notice_ = nullptr;
    }
};

const char* SessionText() {
    if(runtime_lifecycle::HomeRequested())return slow_stop.load()?"Stop pending; ownership retained":"Stopping Stackchan";
    // The original HAL background task owns avatar/motion/reminder updates.
    auto state = Application::GetInstance().GetDeviceState();
    const char* text = "Stackchan - starting";
    switch (state) {
        case kDeviceStateWifiConfiguring: text = "Stackchan - Wi-Fi setup"; break;
        case kDeviceStateActivating: text = "Stackchan - activation"; break;
        case kDeviceStateIdle: text = "Stackchan - wake word on"; break;
        case kDeviceStateConnecting: text = "Stackchan - connecting"; break;
        case kDeviceStateListening: text = "Stackchan - listening"; break;
        case kDeviceStateSpeaking: text = "Stackchan - speaking"; break;
        case kDeviceStateAudioTesting: text = "Stackchan - audio test"; break;
        case kDeviceStateFatalError: text = "Stackchan - error"; break;
        default: break;
    }
    return text;
}
void SessionControls() {
    LvglLockGuard lock;
    // Reuse the firmware's bottom-swipe Home overlay, hidden until requested.
    // The session loop owns it even during activation and checked shutdown.
    view::create_home_indicator(ReturnHome, 0x81DBBD, 0x134233,
                                lv_layer_top(), false, SessionText);
}
}
std::unique_ptr<mooncake::AppAbility> MakeApp() { return std::make_unique<StockAgentApp>(); }
bool ConsumeStartRequest() {
    bool requested = start_requested.exchange(false);
    if (requested) start_authorized = true;
    return requested;
}
bool ConsumePreviewRequest() {
    bool requested = esp_reset_reason() == ESP_RST_SW && preview_cookie == preview_magic;
    preview_cookie = 0;
    return requested;
}
bool AutoStartRequested() {
    // Legacy boot_ai remains stored, but only intentional feature entry may
    // activate AI.AGENT in this variant. Passive boot/reconnect is not consent.
    return false;
}
void AuthorizeAutoStart() {
    if (AutoStartRequested()) start_authorized = true;
}
void RunStockAgent() {
    if(!start_authorized.exchange(false)||!runtime_lifecycle::VendorNetworkAvailable())return;
    if(!companion::AcquireAudio(companion::AudioOwner::Stock,companion::AudioPurpose::StockRuntime,1,runtime_audio_lease))return;
    if(!runtime_lifecycle::Begin()){companion::ReleaseAudio(runtime_audio_lease,true);return;}
    runtime_lifecycle::ResetCleanupJob();slow_stop=false;
    companion::SetSettingsScope(companion::SettingsScope::StockRuntime);
    Board::GetInstance().GetUuid();hal_bridge::set_xiaozhi_mode(true);SessionControls();
    if(!Application::GetInstance().GetAudioService().BindAudioLease(runtime_audio_lease))ReturnHome();
    else {
        runtime_lifecycle::ApplicationStarted();
        if(xTaskCreate([](void*){
            GetHAL().startXiaozhi();
            runtime_lifecycle::ApplicationExited();vTaskDelete(nullptr);
        },"stock_session",8192,nullptr,5,nullptr)!=pdPASS){runtime_lifecycle::ApplicationExited();ReturnHome();}
    }
    while(true){
        GetHAL().feedTheDog();
        {LvglLockGuard lock;view::update_home_indicator();}
        auto& session=runtime_lifecycle::Session();
        if(session.Poll(uint64_t(esp_timer_get_time()/1000),[]{companion::RevokeAudio(runtime_audio_lease);runtime_lifecycle::QuietHardware();},
            []{return runtime_lifecycle::ApplicationJoined()&&runtime_lifecycle::UpdateJoined()&&runtime_lifecycle::PollSourcesStopped(nullptr);},
            []{runtime_lifecycle::QuietHardware();return Application::GetInstance().GetAudioService().Stop(10);},
            []{return companion::ReleaseAudio(runtime_audio_lease,true);} ))break;
        slow_stop=session.Pending(uint64_t(esp_timer_get_time()/1000));
        GetHAL().delay(20);
    }
    runtime_lifecycle::ResetSessionUi();companion::SetSettingsScope(companion::SettingsScope::None);
}
}
