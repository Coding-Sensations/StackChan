#include "stock_services.h"
#include <apps/runtime_lifecycle/session.h>
#include <apps/runtime_lifecycle/cleanup.h>
#include <apps/stock_shell/navigation.h>
#include <esp_timer.h>
#include <apps/stock_shell/menu_policy.hpp>
#include "firmware_install_policy.h"
#include "firmware_install_ui.h"
#include <board.h>
#include <audio_codec.h>
#include <application.h>
#include <apps/apps.h>
#include <apps/app_setup/workers/workers.h>
#include <apps/network_setup/persistence_gate.h>
#include <assets/assets.h>
#include <companion/audio_owner.hpp>
#include <companion/boot.h>
#include <companion/settings_policy.hpp>
#include <hal/hal.h>
#include <stackchan/stackchan.h>
#include <esp_app_desc.h>
#include <esp_attr.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <lvgl.h>
#include <ota.h>
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

namespace stock_services {
namespace {
constexpr int kFeatureCount = static_cast<int>(Feature::Onboarding) + 1;
constexpr uint32_t kPreviewMagic = 0x4E535450;
struct PreviewCookie { uint32_t magic, feature, inverse; };
RTC_NOINIT_ATTR PreviewCookie preview_cookie;
std::atomic<int> requested{-1}, authorized{-1}, active{-1};

bool Valid(int feature) { return feature >= 0 && feature < kFeatureCount; }
const char* Name(Feature feature)
{
    switch (feature) {
        case Feature::Avatar: return "AVATAR";
        case Feature::EspNow: return "ESPNOW.REMOTE";
        case Feature::EzData: return "EZDATA";
        case Feature::Dance: return "DANCE";
        case Feature::Servo: return "Servo calibration";
        case Feature::Microphone: return "Microphone test";
        case Feature::Rgb: return "RGB strip test";
        case Feature::Account: return "Account";
        case Feature::FirmwareCheck: return "Firmware updates";
        case Feature::AppCenter: return "APP.CENTER downloads";
        case Feature::Onboarding: return "Welcome setup";
    }
    return "Native service";
}
const char* Effects(Feature feature)
{
    switch (feature) {
        case Feature::Avatar: return "Starts Wi-Fi and the vendor avatar service. Camera/video, remote messages, calls and motor control are enabled.";
        case Feature::EspNow: return "Starts the ESP-NOW radio and original sender/receiver controls. Motor and laser commands are enabled.";
        case Feature::EzData: return "Starts Wi-Fi and vendor EzData pairing/data transfer. Remote motor controls are enabled.";
        case Feature::Dance: return "Starts BLE control for the avatar, motors and RGB strip.";
        case Feature::Servo: return "Powers the motors and opens the original home-position calibration. Confirmed calibration is saved.";
        case Feature::Microphone: return "Opens the original record/playback test. Record Test enables the microphone and speaker.";
        case Feature::Rgb: return "Opens the original RGB strip test. Motors remain off.";
        case Feature::Account: return "Connects Wi-Fi and reads the vendor account/device status. Unbind and reset require the original confirmations.";
        case Feature::FirmwareCheck: return "Checks vendor firmware updates. Installing requires a separate confirmation and replaces this custom firmware.";
        case Feature::AppCenter: return "Opens the original vendor app catalog. Installing a listed app replaces this custom firmware after a separate confirmation.";
        case Feature::Onboarding: return "Runs the original Welcome, Skip, servo test and phone setup wizard. Start enables motors for the test.";
    }
    return "";
}
bool NeedsMotion(Feature feature)
{
    return feature == Feature::Avatar || feature == Feature::EspNow ||
        feature == Feature::EzData || feature == Feature::Dance || feature == Feature::Servo || feature == Feature::Onboarding;
}
bool NeedsFreshRuntime(){return !runtime_lifecycle::VendorNetworkAvailable();}
void ReturnHome(){runtime_lifecycle::RequestHome();}
lv_obj_t* Label(lv_obj_t* parent, const std::string& text, int x, int y, int width)
{
    auto label = lv_label_create(parent);
    lv_label_set_text(label, text.c_str());
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    return label;
}

class Preview {
    Feature feature_;
    lv_obj_t* root_ = nullptr;
    lv_obj_t* message_ = nullptr;
    std::atomic<int> action_{0};
    bool waiting_network_=false;
    void Button(const char* text, int x, int width, int action)
    {
        auto button = lv_button_create(root_);
        lv_obj_set_pos(button, x, 192);
        lv_obj_set_size(button, width, 44);
        lv_obj_set_user_data(button, reinterpret_cast<void*>(static_cast<intptr_t>(action)));
        auto label = lv_label_create(button);
        lv_label_set_text(label, text);
        lv_obj_center(label);
        lv_obj_add_event_cb(button, [](lv_event_t* event) {
            auto self = static_cast<Preview*>(lv_event_get_user_data(event));
            auto target = static_cast<lv_obj_t*>(lv_event_get_target(event));
            self->action_ = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(target)));
        }, LV_EVENT_CLICKED, this);
    }
public:
    explicit Preview(Feature feature, bool activate=false) : feature_(feature)
    {
        LvglLockGuard lock;
        root_ = lv_obj_create(lv_screen_active());
        lv_obj_set_size(root_, 320, 240);
        lv_obj_set_pos(root_, 0, 0);
        lv_obj_set_style_pad_all(root_, 0, 0);
        lv_obj_set_style_radius(root_, 0, 0);
        lv_obj_set_style_bg_color(root_, lv_color_hex(0x10232d), 0);
        lv_obj_set_style_text_color(root_, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_text_font(root_, &lv_font_montserrat_14, 0);
        lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
        Label(root_, std::string(Name(feature)) + (activate ? " - opening" : " - stopped"), 8, 6, 304);
        auto body = lv_obj_create(root_);
        lv_obj_set_pos(body, 4, 30);
        lv_obj_set_size(body, 312, 112);
        lv_obj_set_style_pad_all(body, 0, 0);
        lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(body, 0, 0);
        lv_obj_set_scroll_dir(body, LV_DIR_VER);
        Label(body, Effects(feature), 4, 2, 296);
        bool restart = NeedsFreshRuntime();
        message_ = Label(root_, restart ? "Network owner still active. Return Home and stop Mira first." :
            "Stop / Home returns to the launcher. Companion remains available after return.", 8, 146, 304);
        Button("Back", 8, 88, 1);
        Button(activate ? "Retry" : (restart ? "Retry" : "Start"), 104, 208, 2);
        if(activate)action_=2; // Same checked request as the former second Start.
    }
    ~Preview()
    {
        LvglLockGuard lock;
        if (root_) lv_obj_delete(root_);
    }
    // 0 keep open, 1 Back, 2 native Start. Called only on the shell thread.
    int Poll()
    {
        int action = action_.exchange(0);
        if(!action&&waiting_network_)action=2;
        if(action==1)waiting_network_=false;
        if (action != 2) return action;
        if (companion::CurrentAudioOwner() != companion::AudioOwner::None) {
            LvglLockGuard lock;
            lv_label_set_text(message_, "Audio is still stopping. Retry Start.");
            return 0;
        }
        if (!runtime_lifecycle::PrepareVendorNetwork()) {waiting_network_=true;LvglLockGuard lock;lv_label_set_text(message_,"Stopping previous Wi-Fi; waiting for cleanup. Back cancels opening.");return 0;}
        waiting_network_=false;
        return 2;
    }
};

mooncake::AppAbility::AppInfo_t AppInfo(Feature feature)
{
    switch (feature) {
        case Feature::Avatar: { AppAvatar source; return source.getAppInfo(); }
        case Feature::EspNow: { AppEspnowControl source; return source.getAppInfo(); }
        case Feature::EzData: { AppEzdata source; return source.getAppInfo(); }
        case Feature::Dance: { AppDance source; return source.getAppInfo(); }
        case Feature::AppCenter: { AppAppCenter source; return source.getAppInfo(); }
        default: {
            AppSetup source;
            auto info = source.getAppInfo();
            info.name = Name(feature);
            return info;
        }
    }
}
class PreviewApp final : public mooncake::AppAbility {
    Feature feature_;
    std::unique_ptr<Preview> preview_;
    bool start_ = false;
public:
    explicit PreviewApp(Feature feature) : feature_(feature) { setAppInfo() = AppInfo(feature); }
    void onOpen() override { start_ = false; preview_ = std::make_unique<Preview>(feature_,feature_==Feature::Avatar||feature_==Feature::Dance); }
    void onRunning() override
    {
        int action = preview_ ? preview_->Poll() : 0;
        if (action == 2) start_ = true;
        if (action == 1 || action == 2) close();
    }
    void onClose() override
    {
        preview_.reset();
        if (std::exchange(start_, false)) requested.store(static_cast<int>(feature_));
    }
};
class PreviewWorker final : public setup_workers::WorkerBase {
    Feature feature_;
    Preview preview_;
public:
    explicit PreviewWorker(Feature feature) : feature_(feature), preview_(feature,feature==Feature::Rgb) {}
    void update() override
    {
        int action = preview_.Poll();
        if (action == 2) requested.store(static_cast<int>(feature_));
        if (action == 1 || action == 2) _is_done = true;
    }
};

class SetupTransferWorker final : public setup_workers::WorkerBase {
    Feature feature_;
public:
    explicit SetupTransferWorker(Feature feature):feature_(feature){}
    void update() override {
        if(_is_done)return;
        if(!runtime_lifecycle::PrepareVendorNetwork())return;
        requested.store(static_cast<int>(feature_));_is_done=true;
    }
};

// Query original update metadata, then offer a separate local confirmation
// before any firmware download or image write.
class FirmwareCheckWorker final : public setup_workers::WorkerBase {
    lv_obj_t* root_ = nullptr;
    lv_obj_t* result_ = nullptr;
    std::atomic<bool> back_{false};
    std::string firmware_url_, firmware_version_;
public:
    FirmwareCheckWorker()
    {
        root_ = lv_obj_create(lv_screen_active());
        lv_obj_set_size(root_, 320, 240);
        lv_obj_set_pos(root_, 0, 0);
        lv_obj_set_style_pad_all(root_, 0, 0);
        lv_obj_set_style_bg_color(root_, lv_color_hex(0xEDF4FF), 0);
        lv_obj_set_style_text_font(root_, &lv_font_montserrat_16, 0);
        lv_obj_set_scroll_dir(root_, LV_DIR_VER);
        result_ = Label(root_, "Checking firmware versions...", 10, 52, 294);
        auto button = lv_button_create(root_);
        lv_obj_set_pos(button, 10, 182);
        lv_obj_set_size(button, 294, 48);
        auto label = lv_label_create(button);
        lv_label_set_text(label, "Back");
        lv_obj_center(label);
        lv_obj_add_event_cb(button, [](lv_event_t* event) {
            static_cast<FirmwareCheckWorker*>(lv_event_get_user_data(event))->back_ = true;
        }, LV_EVENT_CLICKED, this);
        // Called by WorkerHost while holding exactly the normal worker LVGL
        // lock. As in stock workers, release it during network/HTTP work so the
        // persistent top-layer Stop/Home remains operable throughout.
        GetHAL().lvglUnlock();
        GetHAL().startNetwork([this](std::string_view message) {
            LvglLockGuard lock;
            lv_label_set_text(result_, std::string(message).c_str());
        });
        Ota ota;
        auto error = ota.CheckVersion();
        std::string result = "Current: " + std::string(esp_app_get_description()->version) + "\n";
        if (error != ESP_OK) result += "Could not check for updates. Try again later.";
        else if (ota.HasNewVersion()) result += "Available: " + ota.GetFirmwareVersion().substr(0, 128) +
            "\nInstalling vendor firmware replaces this custom build.";
        else result += "No newer vendor firmware is listed.";
        if (error == ESP_OK && ota.HasNewVersion() && IsFirmwareImageUrl(ota.GetFirmwareUrl())) {
            firmware_url_ = ota.GetFirmwareUrl(); firmware_version_ = ota.GetFirmwareVersion();
        }
        result += "\nInstallation requires confirmation.";
        GetHAL().lvglLock();
        lv_label_set_text(result_, result.c_str());
        lv_obj_update_layout(result_);
        if (!firmware_url_.empty()) {
            auto install = lv_button_create(root_); lv_obj_set_size(install,294,48);
            lv_obj_set_pos(install,10,lv_obj_get_y(result_)+lv_obj_get_height(result_)+14);
            auto label = lv_label_create(install);lv_label_set_text(label,"Review installation");lv_obj_center(label);
            lv_obj_add_event_cb(install,[](lv_event_t* event){
                auto self=static_cast<FirmwareCheckWorker*>(lv_event_get_user_data(event));
                OfferFirmwareInstall(self->firmware_url_,self->firmware_version_);
            },LV_EVENT_CLICKED,this);
            lv_obj_set_y(button,lv_obj_get_y(install)+62);
        } else lv_obj_set_y(button, std::max(182, static_cast<int>(lv_obj_get_y(result_) + lv_obj_get_height(result_) + 14)));
    }
    ~FirmwareCheckWorker() override { if (root_) lv_obj_delete(root_); }
    void update() override { if (back_) _is_done = true; }
};
std::unique_ptr<setup_workers::WorkerBase> ActualWorker(Feature feature)
{
    switch (feature) {
        case Feature::Servo: return std::make_unique<setup_workers::ZeroCalibrationWorker>();
        case Feature::Microphone: return std::make_unique<setup_workers::MicTestWorker>();
        case Feature::Rgb: return std::make_unique<setup_workers::RgbTestWorker>();
        case Feature::Account: return std::make_unique<setup_workers::AccountWorker>();
        case Feature::FirmwareCheck: return std::make_unique<FirmwareCheckWorker>();
        case Feature::Onboarding: return std::make_unique<setup_workers::StartupWorker>();
        default: return nullptr;
    }
}
class WorkerHost final : public mooncake::AppAbility {
    Feature feature_;
    std::unique_ptr<setup_workers::WorkerBase> worker_;
public:
    explicit WorkerHost(Feature feature) : feature_(feature) { setAppInfo() = AppInfo(feature); }
    void onOpen() override
    {
        LvglLockGuard lock;
        worker_ = ActualWorker(feature_);
    }
    void onRunning() override
    {
        LvglLockGuard lock;
        if (worker_) worker_->update();
        GetStackChan().update();
        if (!worker_ || worker_->isDone()) { ReturnHome();close(); }
    }
    void onClose() override {if(runtime_lifecycle::DeferNativeClose())return;LvglLockGuard lock;worker_.reset();}
};
std::unique_ptr<mooncake::AppAbility> ActualApp(Feature feature)
{
    switch (feature) {
        case Feature::Avatar: return std::make_unique<AppAvatar>();
        case Feature::EspNow: return std::make_unique<AppEspnowControl>();
        case Feature::EzData: return std::make_unique<AppEzdata>();
        case Feature::Dance: return std::make_unique<AppDance>();
        case Feature::AppCenter: return std::make_unique<AppAppCenter>();
        default: return std::make_unique<WorkerHost>(feature);
    }
}
lv_obj_t* SessionControls(Feature feature)
{
    LvglLockGuard lock;
    auto bar = lv_obj_create(lv_layer_top());
    lv_obj_set_size(bar, 320, 48);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x10232d), 0);
    lv_obj_set_style_text_color(bar, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(bar, &lv_font_montserrat_14, 0);
    auto status = Label(bar, std::string(Name(feature)) + " - starting", 5, 8, 218);
    lv_label_set_long_mode(status, LV_LABEL_LONG_DOT);
    auto stop = lv_button_create(bar);
    lv_obj_set_pos(stop, 226, 1);
    lv_obj_set_size(stop, 92, 44);
    auto label = lv_label_create(stop);
    lv_label_set_text(label, stock_shell::IsSetupFeature(feature)?"Stop / Back":"Stop / Home");
    lv_obj_center(label);
    lv_obj_add_event_cb(stop, [](lv_event_t*) { ReturnHome(); }, LV_EVENT_CLICKED, nullptr);
    return status;
}
}

std::optional<Feature> CurrentFeature()
{
    int feature = active.load();
    return Valid(feature) ? std::optional<Feature>(static_cast<Feature>(feature)) : std::nullopt;
}
bool IsActive() { return CurrentFeature().has_value(); }
bool AllowsCamera() { return CurrentFeature() == Feature::Avatar; }
std::unique_ptr<mooncake::AppAbility> MakeApp(Feature feature)
{
    return Valid(static_cast<int>(feature)) ? std::make_unique<PreviewApp>(feature) : nullptr;
}
std::unique_ptr<setup_workers::WorkerBase> MakeWorker(Feature feature)
{
    return Valid(static_cast<int>(feature)) ? std::make_unique<PreviewWorker>(feature) : nullptr;
}
std::unique_ptr<setup_workers::WorkerBase> MakeSetupWorker(Feature feature)
{
    return stock_shell::IsSetupFeature(feature)?std::make_unique<SetupTransferWorker>(feature):nullptr;
}
std::optional<Feature> ConsumeRequest()
{
    int feature = requested.exchange(-1);
    if (!Valid(feature)) return std::nullopt;
    authorized.store(feature);
    return static_cast<Feature>(feature);
}
std::optional<Feature> ConsumePreviewRequest()
{
    auto cookie = preview_cookie;
    preview_cookie = {};
    if (esp_reset_reason() != ESP_RST_SW || cookie.magic != kPreviewMagic ||
        cookie.inverse != ~cookie.feature || !Valid(static_cast<int>(cookie.feature))) return std::nullopt;
    return static_cast<Feature>(cookie.feature);
}
static companion::AudioLease runtime_audio_lease;
void RunFeature(Feature feature)
{
    if(!Valid(static_cast<int>(feature))||authorized.exchange(-1)!=static_cast<int>(feature)||NeedsFreshRuntime())return;
    if(!companion::AcquireAudio(companion::AudioOwner::Native,companion::AudioPurpose::NativeRuntime,1,runtime_audio_lease))return;
    if(!runtime_lifecycle::Begin()){companion::ReleaseAudio(runtime_audio_lease,true);return;}
    runtime_lifecycle::ResetCleanupJob();preview_cookie={};active=static_cast<int>(feature);
    if(feature==Feature::AppCenter||feature==Feature::FirmwareCheck||feature==Feature::Avatar||feature==Feature::EzData||feature==Feature::Account)Board::GetInstance().GetUuid();
    auto status=SessionControls(feature);auto bar=lv_obj_get_parent(status);
    if(!Application::GetInstance().GetAudioService().BindAudioLease(runtime_audio_lease))ReturnHome();
    bool ready=!runtime_lifecycle::HomeRequested()&&GetHAL().prepareStockFeatureHardware(NeedsMotion(feature),feature==Feature::Avatar);
    if(!ready)ReturnHome();
    auto& shell=mooncake::GetMooncake();
    auto launcher_owned=std::make_unique<AppLauncher>();auto* launcher=launcher_owned.get();shell.installApp(std::move(launcher_owned));
    auto owned=ActualApp(feature);auto* app=owned.get();int id=shell.installApp(std::move(owned));
    if(ready&&!runtime_lifecycle::HomeRequested())launcher->openApp(id);
    bool ui_closed=false;
    while(true){
        GetHAL().feedTheDog();
        if(!runtime_lifecycle::HomeRequested())shell.update();
        auto& session=runtime_lifecycle::Session();
        if(session.Poll(uint64_t(esp_timer_get_time()/1000),[]{companion::RevokeAudio(runtime_audio_lease);runtime_lifecycle::QuietHardware();},
            [&]{return runtime_lifecycle::PollSourcesStopped(&shell);},
            []{runtime_lifecycle::QuietHardware();auto& audio=Application::GetInstance().GetAudioService();if(!audio.Stop(10))return false;auto* codec=Board::GetInstance().GetAudioCodec();if(!codec)return false;const bool input=codec->EnableInputChecked(false),output=codec->EnableOutputChecked(false);return input&&output;},
            [&]{
                if(!ui_closed){
                    runtime_lifecycle::FinalizeNativeUi(true);
                    // A native app may already be sleeping after Home. Its UI
                    // was deliberately retained by DeferNativeClose until now.
                    if(ready)app->onClose();
                    runtime_lifecycle::ResetSessionUi();
                    {LvglLockGuard lock;if(lv_obj_is_valid(bar))lv_obj_delete(bar);shell.uninstallAllApps();mooncake::DestroyMooncake();}
                    ui_closed=true;
                }
                return companion::ReleaseAudio(runtime_audio_lease,true);
            }))break;
        if(runtime_lifecycle::HomeRequested()&&!ui_closed){
            LvglLockGuard lock;lv_label_set_text(status,session.Pending(uint64_t(esp_timer_get_time()/1000))?"Stop pending; ownership retained":"Stopping native service");
        }
        GetHAL().delay(20);
    }
    active=-1;runtime_lifecycle::FinalizeNativeUi(false);stock_shell::SetSetupContext(false);
    companion::SetSettingsScope(companion::SettingsScope::None);
}
}
