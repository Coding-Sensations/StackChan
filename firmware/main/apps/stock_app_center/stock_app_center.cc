#include <companion/station_authority.hpp>
#include "stock_app_center.h"
#include <assets/assets.h>
#include <apps/common/common.h>
#include <companion/boot.h>
#include <hal/hal.h>
#include <cJSON.h>
#include <esp_app_desc.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdkconfig.h>
#include <lvgl.h>
#include <algorithm>
#include <atomic>
#include <array>
#include <cstring>
#include <utility>

namespace stock_app_center {
namespace {
constexpr size_t kBodyLimit = 24 * 1024;
constexpr size_t kEntryLimit = 32;
std::atomic<int> requested_app{-1};
std::atomic<bool> fetch_inflight{false};
struct CatalogEntry { std::string name, description, firmware_url, icon_url; };
struct FetchState {
    companion::network::StationBorrow network_use;
    std::atomic<bool> cancelled{false}, done{false};
    std::vector<CatalogEntry> entries;
    std::string message;
};

std::string SafeText(const char* value, size_t limit)
{
    if (!value) return {};
    std::string result;
    for (size_t i = 0; value[i] && i < limit; ++i) {
        const unsigned char c = value[i];
        result += c < 32 || c == 127 ? ' ' : value[i];
    }
    // Do not leave a partial UTF-8 character at the truncation boundary.
    if (value[result.size()] && !result.empty()) {
        size_t pos = result.size();
        while (pos && (static_cast<unsigned char>(result[pos - 1]) & 0xc0) == 0x80) --pos;
        if (pos && static_cast<unsigned char>(result[pos - 1]) >= 0xc0) result.resize(pos - 1);
    }
    return result;
}
std::string Field(cJSON* item, std::initializer_list<const char*> keys, size_t limit)
{
    for (auto key : keys) {
        auto value = cJSON_GetObjectItemCaseSensitive(item, key);
        if (cJSON_IsString(value) && value->valuestring) return SafeText(value->valuestring, limit);
    }
    return {};
}
void ParseCatalog(const std::string& body, FetchState& state)
{
    // Bound parser recursion as well as input size for untrusted JSON.
    int depth = 0;
    bool quoted = false, escaped = false;
    for (char c : body) {
        if (quoted) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
        } else if (c == '"') quoted = true;
        else if (c == '[' || c == '{') {
            if (++depth > 32) { state.message = "The catalog response could not be read."; return; }
        } else if (c == ']' || c == '}') --depth;
    }
    const char* end = nullptr;
    auto root = cJSON_ParseWithLengthOpts(body.c_str(), body.size() + 1, &end, true);
    if (!root) { state.message = "The catalog response could not be read."; return; }
    auto list = root;
    if (cJSON_IsObject(root)) {
        auto code = cJSON_GetObjectItemCaseSensitive(root, "code");
        if (cJSON_IsNumber(code) && code->valueint != 0) {
            state.message = "The catalog is unavailable right now.";
            cJSON_Delete(root);
            return;
        }
        list = cJSON_GetObjectItemCaseSensitive(root, "data");
    }
    if (!cJSON_IsArray(list)) state.message = "The catalog response could not be read.";
    else {
        const int count = cJSON_GetArraySize(list);
        for (int i = 0; i < count && i < static_cast<int>(kEntryLimit); ++i) {
            auto item = cJSON_GetArrayItem(list, i);
            if (!cJSON_IsObject(item)) continue;
            CatalogEntry entry;
            entry.name = Field(item, {"appName", "name"}, 80);
            entry.description = Field(item, {"description", "appDescription", "desc"}, 768);
            entry.firmware_url = Field(item, {"firmwareUrl", "downloadUrl", "otaUrl", "url"}, 512);
            entry.icon_url = Field(item, {"appIconUrl", "iconUrl", "appIcon", "icon"}, 512);
            if (!entry.name.empty()) state.entries.push_back(std::move(entry));
        }
        state.message = state.entries.empty() ? "No firmware is listed right now." :
            count > static_cast<int>(kEntryLimit) ? "Showing the first 32 catalog entries." : "Firmware catalog";
    }
    cJSON_Delete(root);
}

void RunFetch(const std::shared_ptr<FetchState>& state)
{
    // Network initialization belongs to the shell thread. Only atomic connection
    // status is polled here while the asynchronous IP event arrives.
    const int64_t wifi_deadline = esp_timer_get_time() + 15000000;
    while (!state->cancelled && !companion::IsWifiConnected() && esp_timer_get_time() < wifi_deadline)
        vTaskDelay(pdMS_TO_TICKS(50));
    if (state->cancelled) return;
    if (!companion::IsWifiConnected()) {
        state->message = "Saved Wi-Fi did not connect. Please try again.";
        return;
    }
#ifdef CONFIG_STACKCHAN_SERVER_URL
    std::string url = CONFIG_STACKCHAN_SERVER_URL;
#else
    std::string url;
#endif
    while (!url.empty() && url.back() == '/') url.pop_back();
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) {
        state->message = "No online catalog is configured.";
        return;
    }
    url += "/stackChan/apps";
    esp_http_client_config_t config{};
    config.url = url.c_str();
    config.timeout_ms = 4000;
    config.disable_auto_redirect = true;
    config.buffer_size = 1024;
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
    config.crt_bundle_attach = esp_crt_bundle_attach;
#endif
    auto client = esp_http_client_init(&config);
    bool valid = client && !state->cancelled && esp_http_client_open(client, 0) == ESP_OK;
    if (valid) {
        const int64_t declared = esp_http_client_fetch_headers(client);
        valid = declared >= 0 && declared <= static_cast<int64_t>(kBodyLimit) &&
            esp_http_client_get_status_code(client) == 200;
    }
    std::string body;
    const int64_t deadline = esp_timer_get_time() + 12000000;
    std::array<char, 1024> buffer;
    while (valid && !state->cancelled && esp_timer_get_time() < deadline) {
        int read = esp_http_client_read(client, buffer.data(), buffer.size());
        if (read < 0) { valid = false; break; }
        if (!read) { valid = esp_http_client_is_complete_data_received(client); break; }
        if (body.size() + read > kBodyLimit) { valid = false; break; }
        body.append(buffer.data(), read);
    }
    if (client) {
        valid = valid && esp_http_client_is_complete_data_received(client);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
    if (!state->cancelled) {
        if (valid) ParseCatalog(body, *state);
        else state->message = "Could not fetch the catalog. Please try again.";
    }
}
void FetchCatalog(void* context)
{
    auto state = *static_cast<std::shared_ptr<FetchState>*>(context);
    delete static_cast<std::shared_ptr<FetchState>*>(context);
    RunFetch(state);
    state->network_use.Reset(); // HTTP client is closed; permit checked station stop
    state->done.store(true, std::memory_order_release);
    // Release C++ ownership before FreeRTOS deletes the task stack.
    state.reset();
    fetch_inflight.store(false, std::memory_order_release);
    vTaskDelete(nullptr);
}

class Center final : public mooncake::AppAbility {
    enum class Screen { LocalList, LocalDetail, OnlineList, OnlineDetail, Fetching, About };
    struct Callback { Center* owner; int action; };
    std::vector<InstalledApp> installed_;
    std::vector<CatalogEntry> catalog_;
    std::vector<std::unique_ptr<Callback>> callbacks_;
    std::shared_ptr<FetchState> fetch_;
    std::atomic<int> action_{0};
    lv_obj_t* root_ = nullptr;
    lv_obj_t* content_ = nullptr;
    Screen screen_ = Screen::LocalList;
    size_t selected_ = 0;
    int pending_app_ = -1;
    std::string catalog_message_ = "Browse downloadable firmware from the online catalog.";

    void Text(lv_obj_t* parent, const std::string& text, int y, int width = 286)
    {
        auto label = lv_label_create(parent);
        lv_label_set_text(label, text.c_str());
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_set_pos(label, 8, y);
        lv_obj_set_width(label, width);
    }
    void Button(lv_obj_t* parent, const std::string& text, int x, int y, int width, int height, int action)
    {
        auto button = lv_button_create(parent);
        lv_obj_set_pos(button, x, y);
        lv_obj_set_size(button, width, height);
        lv_obj_set_style_bg_color(button, lv_color_hex(0xFFDF9A), 0);
        lv_obj_set_style_text_color(button, lv_color_hex(0x47330A), 0);
        auto label = lv_label_create(button);
        lv_label_set_text(label, text.c_str());
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_set_width(label, width - 14);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(label);
        auto callback = std::make_unique<Callback>(Callback{this, action});
        lv_obj_add_event_cb(button, [](lv_event_t* event) {
            auto callback = static_cast<Callback*>(lv_event_get_user_data(event));
            callback->owner->action_.store(callback->action);
        }, LV_EVENT_CLICKED, callback.get());
        callbacks_.push_back(std::move(callback));
    }
    void Render()
    {
        LvglLockGuard lock;
        if (!root_) return;
        lv_obj_clean(root_);
        callbacks_.clear();
        Text(root_, "App Center", 8, 220);
        Button(root_, "Back", 250, 2, 66, 30, 1);
        Button(root_, "On device", 8, 38, 98, 32, 2);
        Button(root_, "Online", 111, 38, 98, 32, 3);
        Button(root_, "About", 214, 38, 98, 32, 4);
        content_ = lv_obj_create(root_);
        lv_obj_set_pos(content_, 4, 76);
        lv_obj_set_size(content_, 312, 160);
        lv_obj_set_style_pad_all(content_, 0, 0);
        lv_obj_set_style_border_width(content_, 0, 0);
        lv_obj_set_style_bg_color(content_, lv_color_hex(0xFFFAD6), 0);
        lv_obj_set_scroll_dir(content_, LV_DIR_VER);
        if (screen_ == Screen::LocalList) {
            if (installed_.empty()) Text(content_, "No local apps are registered.", 8);
            for (size_t i = 0; i < installed_.size(); ++i)
                Button(content_, installed_[i].name, 8, i * 46 + 4, 286, 40, 100 + i);
        } else if (screen_ == Screen::LocalDetail && selected_ < installed_.size()) {
            Text(content_, installed_[selected_].name + "\n" + installed_[selected_].description, 6);
            auto label = lv_obj_get_child(content_, 0);
            lv_obj_update_layout(label);
            Button(content_, "Open", 8, lv_obj_get_height(label) + 18, 286, 40, 5);
        } else if (screen_ == Screen::Fetching) {
            Text(content_, "Fetching catalog...\nBack cancels this request.", 10);
        } else if (screen_ == Screen::OnlineList) {
            Text(content_, catalog_message_, 4);
            auto label = lv_obj_get_child(content_, 0);
            lv_obj_update_layout(label);
            int y = lv_obj_get_height(label) + 16;
            Button(content_, "Fetch catalog", 8, y, 286, 36, 6);
            y += 46;
            for (size_t i = 0; i < catalog_.size(); ++i)
                Button(content_, catalog_[i].name, 8, y + i * 46, 286, 40, 200 + i);
        } else if (screen_ == Screen::OnlineDetail && selected_ < catalog_.size()) {
            const auto& entry = catalog_[selected_];
            Text(content_, entry.name + "\nFirmware\n\n" + entry.description +
                "\n\nInstalling replaces this firmware and may remove Companion and the built-in apps."
                "\n\nFirmware installation is not available here." +
                (entry.firmware_url.empty() ? "\nNo firmware download is listed." : ""), 6);
        } else if (screen_ == Screen::About) {
            auto info = esp_app_get_description();
            Text(content_, std::string("Current firmware\n") + info->project_name + " " + info->version +
                "\n\nOn-device apps open within this firmware. Online entries are replacement firmware."
                "\n\nTo restore this custom build, flash its saved firmware and assets over USB.", 6);
        }
    }
    void CancelFetch()
    {
        if (fetch_) fetch_->cancelled = true;
    }
    void StartFetch()
    {
        // Process-lifetime gate also covers app destruction/reopening while a
        // cancelled worker finishes its current bounded network operation.
        bool expected = false;
        if (!fetch_inflight.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            catalog_message_ = "The previous request is finishing. Try again shortly.";
            screen_ = Screen::OnlineList; Render(); return;
        }
        // Explicit fetch action, shell thread only: no worker may initialize Wi-Fi.
        if (!companion::EnsureSavedNetwork()) {
            fetch_inflight.store(false, std::memory_order_release);
            catalog_message_ = "Saved Wi-Fi is unavailable.";
            screen_ = Screen::OnlineList; Render(); return;
        }
        fetch_ = std::make_shared<FetchState>();
        fetch_->network_use=companion::network::Authority().BorrowSaved();
        if(!fetch_->network_use){fetch_.reset();fetch_inflight=false;catalog_message_="Wi-Fi is stopping. Try again after Home.";screen_=Screen::OnlineList;Render();return;}
        auto context = new std::shared_ptr<FetchState>(fetch_);
        if (xTaskCreate(FetchCatalog, "app_catalog", 8192, context, 3, nullptr) != pdPASS) {
            delete context;
            fetch_.reset();
            fetch_inflight.store(false, std::memory_order_release);
            catalog_message_ = "Could not start the catalog request. Try again.";
            screen_ = Screen::OnlineList;
        } else screen_ = Screen::Fetching;
        Render();
    }
public:
    explicit Center(std::vector<InstalledApp> installed) : installed_(std::move(installed))
    {
        installed_.erase(std::remove_if(installed_.begin(), installed_.end(), [](const InstalledApp& app) {
            return app.app_id < 0 || app.name.empty();
        }), installed_.end());
        if (installed_.size() > 64) installed_.resize(64);
        setAppInfo().name = "APP.CENTER";
        static auto icon = assets::get_image("icon_app_center.bin");
        static uint32_t theme = 0xF4A354;
        setAppInfo().icon = &icon;
        setAppInfo().userData = &theme;
    }
    ~Center() override { CancelFetch(); }
    void onOpen() override
    {
        screen_ = Screen::LocalList;
        pending_app_ = -1;
        action_ = 0;
        {
            LvglLockGuard lock;
            root_ = lv_obj_create(lv_screen_active());
            lv_obj_set_pos(root_, 0, 0);
            lv_obj_set_size(root_, 320, 240);
            lv_obj_set_style_pad_all(root_, 0, 0);
            lv_obj_set_style_radius(root_, 0, 0);
            lv_obj_set_style_border_width(root_, 0, 0);
            lv_obj_set_style_bg_color(root_, lv_color_hex(0xF4A354), 0);
            lv_obj_set_style_text_color(root_, lv_color_hex(0x47330A), 0);
            lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
        }
        Render();
    }
    void onRunning() override
    {
        const int action = action_.exchange(0);
        if (action == 1) {
            CancelFetch();
            if (screen_ == Screen::LocalList) { close(); return; }
            screen_ = screen_ == Screen::OnlineDetail || screen_ == Screen::Fetching ?
                Screen::OnlineList : Screen::LocalList;
            Render();
        } else if (action >= 2 && action <= 4) {
            CancelFetch();
            screen_ = action == 2 ? Screen::LocalList : action == 3 ? Screen::OnlineList : Screen::About;
            Render();
        } else if (action == 5 && screen_ == Screen::LocalDetail && selected_ < installed_.size()) {
            pending_app_ = installed_[selected_].app_id;
            close(); return;
        } else if (action == 6 && screen_ == Screen::OnlineList) StartFetch();
        else if (action >= 100 && action < 100 + static_cast<int>(installed_.size()) && screen_ == Screen::LocalList) {
            selected_ = action - 100; screen_ = Screen::LocalDetail; Render();
        } else if (action >= 200 && action < 200 + static_cast<int>(catalog_.size()) && screen_ == Screen::OnlineList) {
            selected_ = action - 200; screen_ = Screen::OnlineDetail; Render();
        }
        if (fetch_ && fetch_->done.load(std::memory_order_acquire)) {
            if (!fetch_->cancelled) {
                catalog_ = std::move(fetch_->entries);
                catalog_message_ = std::move(fetch_->message);
                if (screen_ == Screen::Fetching) { screen_ = Screen::OnlineList; Render(); }
            }
            fetch_.reset();
        }
    }
    void onClose() override
    {
        CancelFetch();
        {
            LvglLockGuard lock;
            if (root_) lv_obj_delete(root_);
            root_ = content_ = nullptr;
            callbacks_.clear();
            action_ = 0;
        }
        if (pending_app_ >= 0) requested_app.store(std::exchange(pending_app_, -1));
    }
};
}
std::unique_ptr<mooncake::AppAbility> MakeApp(std::vector<InstalledApp> installed)
{
    return std::make_unique<Center>(std::move(installed));
}
int ConsumeRequestedAppId() { return requested_app.exchange(-1); }
}
