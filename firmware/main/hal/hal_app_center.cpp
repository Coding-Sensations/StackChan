/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include "utils/ota/ota.h"
#include "utils/secret_logic/secret_logic.h"
#include <apps/stock_services/firmware_install_policy.h>
#include <mooncake_log.h>
#include <cJSON.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_timer.h>
#include <array>
#include <atomic>
#include <memory>
#include <cstring>

namespace {
constexpr size_t kBodyLimit = 24 * 1024;
constexpr size_t kEntryLimit = 32;
const std::string_view kTag = "HAL-AppCenter";

std::string field(cJSON* item, std::initializer_list<const char*> keys, size_t limit, bool url = false)
{
    for (auto key : keys) {
        auto value = cJSON_GetObjectItemCaseSensitive(item, key);
        if (!cJSON_IsString(value) || !value->valuestring) continue;
        const char* text = value->valuestring;
        const size_t size = strnlen(text, limit + 1);
        if (url) return size <= limit ? std::string(text, size) : std::string();
        std::string result(text, size > limit ? limit : size);
        for (char& c : result) if (static_cast<unsigned char>(c) < 32 || c == 127) c = ' ';
        // Avoid cutting a UTF-8 codepoint at the response limit.
        if (size > limit && !result.empty()) {
            size_t pos = result.size();
            while (pos && (static_cast<unsigned char>(result[pos - 1]) & 0xc0) == 0x80) --pos;
            if (pos && static_cast<unsigned char>(result[pos - 1]) >= 0xc0) result.resize(pos - 1);
        }
        return result;
    }
    return {};
}
bool boundedDepth(const std::string& body)
{
    int depth = 0;
    bool quoted = false, escaped = false;
    for (char c : body) {
        if (quoted) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
        } else if (c == '"') quoted = true;
        else if (c == '{' || c == '[') { if (++depth > 32) return false; }
        else if (c == '}' || c == ']') { if (--depth < 0) return false; }
    }
    return !quoted && depth == 0;
}
std::function<void(int)> on_progress;
std::atomic<bool> installing{false};
void ota_callback(int progress) { if (on_progress) on_progress(progress); }
}

app_center::AppInfoList_t Hal::fetchAppList()
{
    app_center::AppInfoList_t result;
    const std::string url = secret_logic::get_server_url() + "/stackChan/apps";
    if (!stock_services::IsFirmwareImageUrl(url)) return result;
    esp_http_client_config_t config{};
    config.url = url.c_str();
    config.timeout_ms = 4000;
    config.disable_auto_redirect = true;
    config.buffer_size = 1024;
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
    config.crt_bundle_attach = esp_crt_bundle_attach;
#endif
    auto client = esp_http_client_init(&config);
    if (!client) return result;
    bool valid = esp_http_client_open(client, 0) == ESP_OK;
    if (valid) {
        const int64_t length = esp_http_client_fetch_headers(client);
        valid = length >= 0 && length <= static_cast<int64_t>(kBodyLimit) &&
                esp_http_client_get_status_code(client) == 200;
    }
    std::string body;
    std::array<char, 1024> buffer{};
    const int64_t deadline = esp_timer_get_time() + 12000000;
    while (valid && esp_timer_get_time() < deadline) {
        const int count = esp_http_client_read(client, buffer.data(), buffer.size());
        if (count < 0) { valid = false; break; }
        if (!count) { valid = esp_http_client_is_complete_data_received(client); break; }
        if (body.size() + count > kBodyLimit) { valid = false; break; }
        body.append(buffer.data(), count);
    }
    valid = valid && esp_http_client_is_complete_data_received(client);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (!valid || !boundedDepth(body)) {
        mclog::tagError(kTag, "catalog unavailable or response exceeds limits");
        return result;
    }
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(
        cJSON_ParseWithLengthOpts(body.c_str(), body.size() + 1, nullptr, true), cJSON_Delete);
    if (!root) return result;
    cJSON* list = root.get();
    if (cJSON_IsObject(list)) {
        auto code = cJSON_GetObjectItemCaseSensitive(list, "code");
        if (cJSON_IsNumber(code) && code->valueint != 0) return result;
        list = cJSON_GetObjectItemCaseSensitive(list, "data");
    }
    if (!cJSON_IsArray(list)) return result;
    const int count = cJSON_GetArraySize(list);
    for (int index = 0; index < count && index < static_cast<int>(kEntryLimit); ++index) {
        auto item = cJSON_GetArrayItem(list, index);
        if (!cJSON_IsObject(item)) continue;
        app_center::AppInfo_t info;
        info.name = field(item, {"appName", "name"}, 80);
        info.iconUrl = field(item, {"appIconUrl", "iconUrl", "appIcon", "icon"}, 512, true);
        info.description = field(item, {"description", "appDescription", "desc"}, 768);
        // Stock aliases describe an ESP firmware image, never an executable package/plugin.
        info.firmwareUrl = field(item, {"firmwareUrl", "downloadUrl", "otaUrl", "url"}, 512, true);
        if (!info.name.empty()) result.push_back(std::move(info));
    }
    return result;
}

void Hal::launchApp(std::string_view url, std::function<void(int)> onProgress)
{
    if (!stock_services::FirmwareInstallAllowed() || !stock_services::IsFirmwareImageUrl(url)) {
        if (onProgress) onProgress(-1);
        return;
    }
    bool expected = false;
    if (!installing.compare_exchange_strong(expected, true)) {
        if (onProgress) onProgress(-1);
        return;
    }
    const std::string image_url(url);  // Original helper requires a NUL-terminated string.
    on_progress = std::move(onProgress);
    start_ota_update(image_url.c_str(), ota_callback);
    // Successful OTA restarts. Any return is failure; relinquish all permission/callback state.
    stock_services::SetFirmwareInstallAllowed(false);
    auto failed = std::move(on_progress);
    on_progress = nullptr;
    installing.store(false);
    if (failed) failed(-1);
}
