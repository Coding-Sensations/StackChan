// SPDX-License-Identifier: MIT
// Explicit, bounded network setup using official StackChan World / browser flows.
#include "network_setup.h"
#include <apps/runtime_lifecycle/session.h>
#include <apps/runtime_lifecycle/cleanup.h>
#include <apps/app_setup/workers/workers.h>
#include <apps/stock_services/policy.h>
#include <application.h>
#include <settings.h>
#include <assets/assets.h>
#include <companion/audio_owner.hpp>
#include <companion/boot.h>
#include <hal/hal.h>
#include <stackchan/stackchan.h>
#include <wifi_manager.h>
#include <esp_attr.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <lvgl.h>
#include <atomic>
#include <cstdio>
#include <cstdint>

namespace network_setup {
namespace {
constexpr uint64_t setup_timeout_ms = 5 * 60 * 1000;
constexpr uint32_t preview_magic = 0x57494650; // WIFP: stopped preview, no consent
RTC_NOINIT_ATTR uint32_t preview_cookie;
std::atomic<bool> persistence_allowed{false};
enum class PersistenceScope { Standalone, NativeOnboarding, StockWifi };
std::atomic<PersistenceScope> persistence_scope{PersistenceScope::Standalone};
std::atomic<uint32_t> persistence_deadline{0};
std::atomic<bool> network_started{false};
std::atomic<int> save_result{0}; // 0 pending, 1 committed, -1 rejected
std::atomic<bool> browser_exit{false};
uint64_t NowMs() { return static_cast<uint64_t>(esp_timer_get_time()) / 1000; }
bool NativeOnboardingOwnsScope() {
    return companion::CurrentAudioOwner() == companion::AudioOwner::Native &&
        stock_services::IsActive() &&
        stock_services::CurrentFeature() == stock_services::Feature::Onboarding;
}
bool StockWifiOwnsScope() {
    return companion::CurrentAudioOwner() == companion::AudioOwner::Stock &&
        Application::GetInstance().GetDeviceState() == kDeviceStateWifiConfiguring;
}
void OpenPersistenceScope(PersistenceScope scope) {
    persistence_deadline = static_cast<uint32_t>(NowMs() + setup_timeout_ms);
    save_result = 0;
    network_started = true;
    persistence_scope = scope;
    persistence_allowed = true;
}

void ReturnHome(){persistence_allowed=false;preview_cookie=0;runtime_lifecycle::RequestHome();}
lv_obj_t* Label(lv_obj_t* parent, const char* text, int x, int y, int width) {
    auto label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_label_set_text(label, text);
    return label;
}

class Controller {
    enum class Mode { Preview, PhoneApp, Browser };
    Mode mode_ = Mode::Preview;
    Mode waiting_mode_=Mode::Preview;
    std::atomic<int> action_{0};
    lv_obj_t* panel_ = nullptr;
    lv_obj_t* info_ = nullptr;
    lv_obj_t* top_ = nullptr;
    lv_obj_t* session_label_ = nullptr;
    std::unique_ptr<setup_workers::WorkerBase> phone_;
    uint64_t deadline_ = 0;
    bool done_ = false;
    bool started_ = false;
    void DeletePanel() {
        if (panel_) lv_obj_delete(panel_);
        panel_ = info_ = nullptr;
    }
    void Panel() {
        panel_ = lv_obj_create(lv_screen_active());
        lv_obj_set_size(panel_, 320, 240);
        lv_obj_set_pos(panel_, 0, 0);
        lv_obj_set_style_pad_all(panel_, 0, 0);
        lv_obj_remove_flag(panel_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(panel_, lv_color_hex(0x10232d), 0);
        lv_obj_set_style_text_color(panel_, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(panel_, &lv_font_montserrat_16, 0);
    }
    void Button(lv_obj_t* parent, const char* text, int x, int y, int width, int id) {
        auto button = lv_button_create(parent);
        lv_obj_set_pos(button, x, y);
        lv_obj_set_size(button, width, 44);
        lv_obj_set_user_data(button, reinterpret_cast<void*>(static_cast<intptr_t>(id)));
        auto label = lv_label_create(button);
        lv_label_set_text(label, text);
        lv_obj_center(label);
        lv_obj_add_event_cb(button, [](lv_event_t* e) {
            auto self = static_cast<Controller*>(lv_event_get_user_data(e));
            auto button = static_cast<lv_obj_t*>(lv_event_get_target(e));
            self->action_ = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(button)));
        }, LV_EVENT_CLICKED, this);
    }
    void Preview() {
        Panel();
        Label(panel_, "Change Wi-Fi - stopped", 10, 12, 224);
        Button(panel_, "Back", 245, 2, 65, 1);
        bool owner_busy = !runtime_lifecycle::VendorNetworkAvailable();
        info_ = Label(panel_, owner_busy
            ? "Network owner is still active.\nStop the current companion, then retry."
            : "Choose a setup method.\nYour phone submits the new Wi-Fi.\nAI, audio and motion stay off.", 10, 57, 300);
        Button(panel_, "Start phone app", 10, 130, 300, 2);
        Button(panel_, "Start browser", 10, 182, 300, 3);
    }
    void SessionBar() {
        top_ = lv_obj_create(lv_layer_top());
        lv_obj_set_size(top_, 320, 48);
        lv_obj_align(top_, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_style_pad_all(top_, 0, 0);
        lv_obj_remove_flag(top_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(top_, lv_color_hex(0x10232d), 0);
        lv_obj_set_style_text_color(top_, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(top_, &lv_font_montserrat_16, 0);
        session_label_ = Label(top_, "Wi-Fi setup starting", 5, 12, 201);
        lv_label_set_long_mode(session_label_, LV_LABEL_LONG_DOT);
        Button(top_, "Stop / Home", 207, 0, 110, 1);
    }
    void Start(Mode requested) {
        if (companion::CurrentAudioOwner() != companion::AudioOwner::None) {
            lv_label_set_text(info_, "Audio is still stopping.\nRetry setup when it is off.");
            return;
        }
        if(!runtime_lifecycle::PrepareVendorNetwork()){
            waiting_mode_=requested;lv_label_set_text(info_,"Stopping previous Wi-Fi. Back cancels setup.");return;
        }
        waiting_mode_=Mode::Preview;
        if(!runtime_lifecycle::Begin()){
            lv_label_set_text(info_,"Network owner still active. Stop it first; no restart performed.");return;
        }
        runtime_lifecycle::ResetCleanupJob();
        network_started = true;
        OpenPersistenceScope(PersistenceScope::Standalone);
        save_result = 0;
        browser_exit = false;
        deadline_ = NowMs() + setup_timeout_ms;
        started_ = true;
        mode_ = requested;
        DeletePanel();
        SessionBar();
        if (mode_ == Mode::PhoneApp) {
            // The actual official StackChan World app onboarding, including its
            // Next action and BLE handshake/setWifi messages, is retained.
            phone_ = std::make_unique<setup_workers::WifiSetupWorker>();
            return;
        }
        Panel();
        info_ = Label(panel_, "Starting phone browser setup...", 10, 56, 300);
        auto& wifi = WifiManager::GetInstance();
        WifiManagerConfig config;
        config.ssid_prefix = "StackChan-Setup";
        config.language = "en-US";
        if (!wifi.Initialize(config)) {
            persistence_allowed = false;
            lv_label_set_text(info_, "Wi-Fi could not start.\nExisting settings are preserved.\nStop / Home to return.");
            return;
        }
        wifi.SetEventCallback([](WifiEvent event, const std::string&) {
            if (event == WifiEvent::ConfigModeExit) browser_exit = true;
        });
        wifi.StartConfigAp(); // no station auto-connect, Board, cloud or audio
        if (!wifi.IsConfigMode()) {
            persistence_allowed = false;
            lv_label_set_text(info_, "Hotspot could not start.\nStop / Home to return.");
            return;
        }
        auto ssid = wifi.GetApSsid();
        auto url = wifi.GetApWebUrl();
        char text[256];
        std::snprintf(text, sizeof(text), "On your phone, join:\n%s\nThen open:\n%s\nSelect or enter Wi-Fi and submit.\nOnly your Wi-Fi choice is saved.", ssid.c_str(), url.c_str());
        lv_label_set_text(info_, text);
    }
public:
    Controller() {
        LvglLockGuard lock;
        Preview();
    }
    ~Controller() {
        // Closing an active worker/app ends BLE/AP and any outstanding request
        // through reset. A stopped preview can return without any radio action.
        if (started_) ReturnHome();
        LvglLockGuard lock;
        DeletePanel();
        if (top_) lv_obj_delete(top_);
    }
    bool Done() const { return done_; }
    void Tick() {
        if(runtime_lifecycle::HomeRequested())return;
        LvglLockGuard lock;
        int action = action_.exchange(0);
        if(!action&&mode_==Mode::Preview&&waiting_mode_!=Mode::Preview)action=waiting_mode_==Mode::PhoneApp?2:3;
        if (action == 1) {
            waiting_mode_=Mode::Preview;
            if (started_) {ReturnHome();return;}
            done_ = true;
            return;
        }
        if (mode_ == Mode::Preview) {
            if (action == 2) Start(Mode::PhoneApp);
            if (action == 3) Start(Mode::Browser);
            return;
        }
        if (NowMs() >= deadline_) ReturnHome();
        if (browser_exit.exchange(false)) ReturnHome();
        if (phone_) {
            phone_->update();
            GetStackChan().update(); // avatar only; no motion is attached
            if (phone_->isDone() && CredentialSaveSucceeded()) ReturnHome();
        }
        auto result = save_result.load();
        char status[80];
        auto now = NowMs();
        auto seconds = static_cast<unsigned>((deadline_ > now ? deadline_ - now : 0) / 1000);
        std::snprintf(status, sizeof(status), "%s %s %us",
            mode_ == Mode::PhoneApp ? "Wi-Fi app" : "Wi-Fi browser",
            result > 0 ? "saved" : result < 0 ? "save failed" : "setup", seconds);
        lv_label_set_text(session_label_, status);
    }
};

class SetupWorker : public setup_workers::WorkerBase {
    Controller controller_;
public:
    void update() override {
        controller_.Tick();
        _is_done = controller_.Done();
    }
};
class SetupApp : public mooncake::AppAbility {
    std::unique_ptr<Controller> controller_;
public:
    SetupApp() {
        setAppInfo().name = "WI-FI SETUP";
        static auto icon = assets::get_image("icon_setup.bin");
        setAppInfo().icon = &icon;
    }
    void onOpen() override { controller_ = std::make_unique<Controller>(); }
    void onRunning() override {
        if (!controller_) return;
        controller_->Tick();
        if (controller_->Done()) close();
    }
    void onClose() override { controller_.reset(); }
};
}
std::unique_ptr<mooncake::AppAbility> MakeApp() { return std::make_unique<SetupApp>(); }
std::unique_ptr<setup_workers::WorkerBase> MakeWorker() { return std::make_unique<SetupWorker>(); }
bool ConsumePreviewRequest() {
    bool requested = esp_reset_reason() == ESP_RST_SW && preview_cookie == preview_magic;
    preview_cookie = 0;
    return requested;
}
bool BeginOriginalOnboardingSession() {
    if (!NativeOnboardingOwnsScope()) return false;
    OpenPersistenceScope(PersistenceScope::NativeOnboarding);
    return true;
}
void EndOriginalOnboardingSession() { persistence_allowed = false; }
bool BeginStockWifiProvisioningSession() {
    if (!StockWifiOwnsScope()) return false;
    OpenPersistenceScope(PersistenceScope::StockWifi);
    return true;
}
void EndStockWifiProvisioningSession() {
    if (persistence_scope.load() == PersistenceScope::StockWifi) persistence_allowed = false;
}
bool CredentialPersistenceAllowed() {
    if (!persistence_allowed.load()) return false;
    auto scope = persistence_scope.load();
    bool owns_scope = scope == PersistenceScope::NativeOnboarding ? NativeOnboardingOwnsScope() :
        scope == PersistenceScope::StockWifi ? StockWifiOwnsScope() :
        companion::CurrentAudioOwner() == companion::AudioOwner::None;
    if (!owns_scope) {
        persistence_allowed = false;
        return false;
    }
    if (static_cast<int32_t>(static_cast<uint32_t>(NowMs()) - persistence_deadline.load()) >= 0) {
        persistence_allowed = false;
        return false;
    }
    return true;
}
void NotifyCredentialsSaved(bool success) {
    save_result = success ? 1 : -1;
    // Completed explicit setup marks its completion flag. Failed
    // credential commits preserve the prior flag and never configure the device.
    if (success && CredentialPersistenceAllowed()) {
        Settings configured("app_config", true);
        configured.SetBool("is_configed", true);
    }
}
bool CredentialSaveSucceeded() { return save_result.load() > 0; }
bool NetworkWasStarted() { return network_started.load(); }
}
