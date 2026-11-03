#!/usr/bin/env python3
"""Generate reviewed variants from exact pinned vendor sources; no fetched file edits."""
import argparse
import hashlib
from pathlib import Path

PINNED = {
    'dns_server.cc': '8e6f03dc39f7386a149e227c756a53e3f715454b1d024c7ce38b75cdf00045d1',
    'protocols/mqtt_protocol.cc': 'f555738d12d6ef8e7b43dbe2cb45d6135de3d393b4f6374c99388457d34d6555',
    'protocols/websocket_protocol.cc': 'f8454cdd600ce024d24207405125f6e5730084a2805be413ccf69fbdcfd55a97',
    'device_state_machine.cc': 'c1167dc89493d275de3a9b267eb67bff171404613192e5daca6dc3eb65443ba5',
    'application.cc': '7c9e9bb1b720c405ea52e4991edc555c0fb90887e68524d30aa69dd34bd92f0f',
    'ota.cc': 'd5340cb20b3b4651656c619a138b6f8b0907b86b3a6541f3376f22bb5dd459d8',
    'boards/common/wifi_board.cc': 'df4afcfdd13bae8ebdf0eb6b8d31f7a334aefb01b660f95bf7d4ababecbf789c',
    'wifi_manager.cc': '3346377f3595774d8a8d07b259f4860a9aabaf323e39fba6c1cac598e44d5d20',
    'wifi_configuration_ap.cc': '05e2d1fe46ce57de433078f287accfc06df771c326a23b9eac894d6657a465c3',
    'ssid_manager.cc': 'e3441e0fc84012b539bd08b680cdfbfdaca5eb55af55423400268d0a18d5daca',
}

def pinned(path, name):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != PINNED[name]:
        raise SystemExit(f'Unreviewed vendor source: {name}; re-review adapter before updating pin')
    return data.decode()

def replace_once(source, before, after):
    if source.count(before) != 1:
        raise SystemExit('Vendor adapter anchor did not match exactly once')
    return source.replace(before, after, 1)

def replace_region(source, begin, end, replacement):
    start = source.index(begin)
    finish = source.index(end, start)
    return source[:start] + replacement + '\n\n' + source[finish:]

def generate(xiaozhi, wifi, output):
    output.mkdir(parents=True, exist_ok=True)
    state = pinned(xiaozhi / 'device_state_machine.cc', 'device_state_machine.cc')
    state = '#include <apps/runtime_lifecycle/session.h>\n' + state
    state = replace_once(state, '    DeviceState old_state = current_state_.load();',
        '    if(new_state==kDeviceStateStarting && runtime_lifecycle::ConsumeStateReset())current_state_=kDeviceStateUnknown;\n    DeviceState old_state = current_state_.load();')
    (output / 'stock_device_state_machine.cc').write_text(state)
    source=pinned(xiaozhi/'protocols/websocket_protocol.cc','protocols/websocket_protocol.cc')
    source=replace_once(source,'WebsocketProtocol::~WebsocketProtocol() {','WebsocketProtocol::~WebsocketProtocol() {\n    websocket_.reset(); // Join while callback state and event group are alive.')
    (output/'stock_websocket_protocol.cc').write_text(source)
    source=pinned(xiaozhi/'protocols/mqtt_protocol.cc','protocols/mqtt_protocol.cc')
    source=replace_once(source,'    if (reconnect_timer_ != nullptr) {',
        '    if(mqtt_)mqtt_->Disconnect();\n    mqtt_.reset();\n    udp_.reset();\n    if (reconnect_timer_ != nullptr) {')
    source=replace_once(source,'    udp_.reset();\n    mqtt_.reset();','    // Transport callbacks have joined before timer and event-group destruction.')
    (output/'stock_mqtt_protocol.cc').write_text(source)
    source = pinned(xiaozhi / 'application.cc', 'application.cc')
    source = '#include <apps/stock_services/firmware_install_policy.h>\n#include <apps/stock_services/firmware_install_ui.h>\n#include <hal/hal.h>\n' + source
    source = replace_once(source, '''        if (ota_->HasNewVersion()) {
            if (UpgradeFirmware(ota_->GetFirmwareUrl(), ota_->GetFirmwareVersion())) {
                return; // This line will never be reached after reboot
            }
            // If upgrade failed, continue to normal operation
        }''', '''        if (ota_->HasNewVersion()) {
            stock_services::OfferFirmwareInstall(ota_->GetFirmwareUrl(), ota_->GetFirmwareVersion());
        }''')
    source = replace_once(source,
        'bool Application::UpgradeFirmware(const std::string& url, const std::string& version) {',
        '''bool Application::UpgradeFirmware(const std::string& url, const std::string& version) {
    if (!stock_services::FirmwareInstallAllowed()) {
        stock_services::OfferFirmwareInstall(url, version);
        return false;
    }
    if (!stock_services::IsFirmwareImageUrl(url)) return false;''')
    source = replace_once(source, '    esp_restart();', '    GetHAL().reboot();')
    (output / 'stock_application.cc').write_text(source)

    source = pinned(xiaozhi / 'ota.cc', 'ota.cc')
    source = replace_once(source,
        '        ESP_LOGI(TAG, "Setup HTTP, User-Agent: %s, Serial-Number: %s", user_agent.c_str(), serial_number_.c_str());',
        '        // Serial identity is sent only after explicit Start and never logged.')
    source = replace_once(source,
        '        ESP_LOGE(TAG, "Failed to activate, code: %d, body: %s", status_code, http->ReadAll().c_str());',
        '        ESP_LOGE(TAG, "Failed to activate, code: %d", status_code);')
    source = '#include <apps/runtime_lifecycle/session.h>\n#include <apps/stock_services/firmware_install_policy.h>\n#include <apps/stock_services/firmware_install_ui.h>\n' + source
    source = replace_once(source,
        'bool Ota::Upgrade(const std::string& firmware_url, std::function<void(int progress, size_t speed)> callback) {',
        '''bool Ota::Upgrade(const std::string& firmware_url, std::function<void(int progress, size_t speed)> callback) {
    if (runtime_lifecycle::HomeRequested() || !stock_services::FirmwareInstallAllowed() ||
        !stock_services::IsFirmwareImageUrl(firmware_url) ||
        !stock_services::PrepareOriginalFirmwareWifi()) return false;
    if (runtime_lifecycle::HomeRequested() || !stock_services::FirmwareInstallAllowed()) return false;''')
    source = replace_once(source, '    if (content_length == 0) {',
        '    if (content_length < sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t) || content_length > update_partition->size) {')
    source = replace_once(source, '    auto http = network->CreateHttp(0);\n    if (!http->Open("GET", firmware_url)) {',
        '    auto http = network->CreateHttp(0);\n    http->SetTimeout(4000);\n    if (!http->Open("GET", firmware_url)) {')
    source = replace_once(source, '            heap_caps_free(buffer);\n            return false;\n        }\n\n        // Calculate speed',
        '            if (image_header_checked) esp_ota_abort(update_handle);\n            heap_caps_free(buffer);\n            return false;\n        }\n        if (total_read + static_cast<size_t>(ret) > content_length || (ret == 0 && total_read != content_length)) {\n            if (image_header_checked) esp_ota_abort(update_handle);\n            heap_caps_free(buffer);\n            return false;\n        }\n\n        // Calculate speed')
    source = replace_once(source, '    auto last_calc_time = esp_timer_get_time();\n    while (true) {',
        '    auto last_calc_time = esp_timer_get_time();\n    const auto install_deadline = last_calc_time + 180000000;\n    while (true) {\n        if (esp_timer_get_time() > install_deadline) {\n            if (image_header_checked) esp_ota_abort(update_handle);\n            heap_caps_free(buffer);\n            return false;\n        }')
    source = replace_once(source, '            image_header.append(buffer, buffer_offset);',
        '            image_header.assign(buffer, buffer_offset);')
    source = source.replace('    auto http = network->CreateHttp(0);', '    auto http = network->CreateHttp(0);\n    http->SetTimeout(4000);')
    source = replace_once(source, '    size_t buffer_offset = 0;  // Current data size in buffer', '''    auto cancelled = [] { return runtime_lifecycle::HomeRequested() || !stock_services::FirmwareInstallAllowed(); };
    auto abort_download = [&] {
        if (image_header_checked) esp_ota_abort(update_handle);
        http->Close();
        heap_caps_free(buffer);
    };
    size_t buffer_offset = 0;  // Current data size in buffer''')
    source = replace_once(source, '        if (esp_timer_get_time() > install_deadline) {\n            if (image_header_checked) esp_ota_abort(update_handle);\n            heap_caps_free(buffer);\n            return false;\n        }',
        '        if (cancelled() || esp_timer_get_time() > install_deadline) { abort_download(); return false; }')
    source = replace_once(source, '        if (ret < 0) {',
        '        if (cancelled()) { abort_download(); return false; }\n        if (ret < 0) {')
    source = replace_once(source, '        if (!image_header_checked) {',
        '        if (cancelled()) { abort_download(); return false; }\n        if (!image_header_checked) {')
    source = replace_once(source, '        if (buffer_offset == PAGE_SIZE || (is_last_chunk && buffer_offset > 0)) {',
        '        if (buffer_offset == PAGE_SIZE || (is_last_chunk && buffer_offset > 0)) {\n            if (cancelled()) { abort_download(); return false; }')
    source = replace_once(source, '    esp_err_t err = esp_ota_end(update_handle);',
        '    if (cancelled()) { if (image_header_checked) esp_ota_abort(update_handle); return false; }\n    esp_err_t err = esp_ota_end(update_handle);')
    source = replace_once(source, '''    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {''', '''    // Leave the running image selected through caller cleanup and delays.
    // Only the final serialized reboot commit can select this candidate.
    const bool staged = stock_services::FirmwareInstallAllowed() &&
        runtime_lifecycle::StageFirmwareInstall([update_partition] {
            return esp_ota_set_boot_partition(update_partition) == ESP_OK;
        });
    if (!staged) {''')
    (output / 'stock_ota.cc').write_text(source)

    source = pinned(xiaozhi / 'boards/common/wifi_board.cc', 'boards/common/wifi_board.cc')
    source = replace_once(source, '    wifi_manager.Initialize(config);', '''    if (!wifi_manager.Initialize(config)) {
        Application::GetInstance().Alert("Stock AI", "Wi-Fi failed. Stop / Home to return.", "triangle_exclamation");
        return;
    }''')
    source = '#include <apps/network_setup/persistence_gate.h>\n#include <apps/runtime_lifecycle/session.h>\n#include <mutex>\nstatic std::recursive_mutex runtime_network_callback_mutex;\n' + source
    source = replace_once(source, 'void WifiBoard::OnNetworkEvent(NetworkEvent event, const std::string& data) {', 'void WifiBoard::OnNetworkEvent(NetworkEvent event, const std::string& data) {\n    std::lock_guard<std::recursive_mutex> callback_lock(runtime_network_callback_mutex);\n    if(runtime_lifecycle::HomeRequested())return;')
    source = replace_once(source, 'void WifiBoard::SetNetworkEventCallback(NetworkEventCallback callback) {', 'void WifiBoard::SetNetworkEventCallback(NetworkEventCallback callback) {\n    std::lock_guard<std::recursive_mutex> callback_lock(runtime_network_callback_mutex);')
    source = replace_once(source,
        '    Application::GetInstance().SetDeviceState(kDeviceStateWifiConfiguring);',
        '    Application::GetInstance().SetDeviceState(kDeviceStateWifiConfiguring);\n    network_setup::BeginStockWifiProvisioningSession();')
    source = replace_once(source, '            case WifiEvent::ConfigModeExit:',
        '            case WifiEvent::ConfigModeExit:\n                network_setup::EndStockWifiProvisioningSession();')
    (output / 'stock_wifi_board.cc').write_text(source)

    source=pinned(wifi/'dns_server.cc','dns_server.cc')
    source='''#include <mutex>
#include <map>
// These receipts outlive DNS objects; publication releases this mutex before
// cleanup may erase the receipt and free the former callback owner.
static std::mutex home_dns_mutex;
static std::map<void*,bool> home_dns_receipts;
static void home_dns_arm(void* owner){std::lock_guard<std::mutex> lock(home_dns_mutex);home_dns_receipts[owner]=false;}
static void home_dns_publish(void* owner){std::lock_guard<std::mutex> lock(home_dns_mutex);auto item=home_dns_receipts.find(owner);if(item!=home_dns_receipts.end())item->second=true;}
static bool home_dns_joined(void* owner){std::lock_guard<std::mutex> lock(home_dns_mutex);auto item=home_dns_receipts.find(owner);if(item==home_dns_receipts.end())return true;if(!item->second)return false;home_dns_receipts.erase(item);return true;}
'''+source
    source=replace_once(source,'    if (running_) {\n        Stop();\n    }','    Stop();')
    source=replace_once(source,'    xTaskCreate([](void* arg) {','    home_dns_arm(this);\n    if(xTaskCreate([](void* arg) {')
    source=replace_once(source,'        dns_server->Run();','        dns_server->Run();\n        home_dns_publish(dns_server); // no owner access after publication')
    source=replace_once(source,'    }, "DnsServerTask", 4096, this, 5, &task_handle_);','    }, "DnsServerTask", 4096, this, 5, &task_handle_)!=pdPASS){running_=false;home_dns_publish(this);}')
    source=replace_once(source,'    if (!running_) {\n        return;\n    }','    // A passive exit or failed startup can still require joining.')
    source=replace_once(source,'''    if (task_handle_ != nullptr) {
        // Give the task some time to exit gracefully
        vTaskDelay(pdMS_TO_TICKS(100));
        task_handle_ = nullptr;
    }''','''    while(!home_dns_joined(this))vTaskDelay(pdMS_TO_TICKS(1));
    task_handle_=nullptr;''')
    source=replace_once(source,'    task_handle_ = nullptr;\n    ESP_LOGI(TAG, "DNS server task exiting");','    ESP_LOGI(TAG, "DNS server task exiting");')
    source=replace_once(source,'recvfrom(fd_, buffer, sizeof(buffer),','recvfrom(fd_, buffer, sizeof(buffer)-16,')
    source=replace_once(source,'        // Simple DNS response:', '        if(len<12)continue; // Header and appended answer must fit.\n        // Simple DNS response:')
    (output/'stock_dns_server.cc').write_text(source)

    source = pinned(wifi / 'wifi_manager.cc', 'wifi_manager.cc')
    source = '#include <companion/station_authority.hpp>\n' + source
    for signature in ['bool WifiManager::Initialize(const WifiManagerConfig& config) {','void WifiManager::StartStation() {','void WifiManager::StartConfigAp() {']:
        result='false' if signature.startswith('bool') else ''
        source=replace_once(source,signature,signature+'\n    if(!companion::network::Authority().CanAcquire(companion::network::Owner::Vendor) || companion::network::Authority().Current()!=companion::network::Owner::Vendor)return '+result+';')
    source = replace_once(source, '''    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }''', '    // Preserve pairing, calibration and Wi-Fi on every initialization error.')
    source = replace_once(source, '    ret = esp_wifi_init(&cfg);\n    if (ret != ESP_OK) {', '''    wifi_mode_t reused_mode{};
    ret = esp_wifi_init(&cfg);
    if (ret == ESP_ERR_WIFI_INIT_STATE && (esp_wifi_get_mode(&reused_mode)!=ESP_OK || reused_mode!=WIFI_MODE_NULL))return false;
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_INIT_STATE) {''')
    (output / 'stock_wifi_manager.cc').write_text(source)

    source = pinned(wifi / 'wifi_configuration_ap.cc', 'wifi_configuration_ap.cc')
    source = '#include <apps/network_setup/wifi_credentials.hpp>\n#include <apps/network_setup/advanced_config.hpp>\n#include <apps/network_setup/persistence_gate.h>\n' + source
    source = replace_once(source,
        '        .uri = "/submit",\n        .method = HTTP_POST,\n        .handler = [](httpd_req_t *req) -> esp_err_t {',
        '        .uri = "/submit",\n        .method = HTTP_POST,\n        .handler = [](httpd_req_t *req) -> esp_err_t {\n            if (!network_setup::CredentialPersistenceAllowed()) { httpd_resp_set_status(req, "403 Forbidden"); httpd_resp_set_type(req, "application/json"); httpd_resp_send(req, "{\\\"success\\\":false,\\\"error\\\":\\\"Setup session ended\\\"}", HTTPD_RESP_USE_STRLEN); return ESP_OK; }')
    source = replace_once(source, '            this_->Save(ssid_str, password_str);',
        '            this_->Save(ssid_str, password_str);\n            if (!network_setup::CredentialPersistenceAllowed() || !network_setup::CredentialSaveSucceeded()) { cJSON_Delete(json); httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Wi-Fi connected but could not be saved"); return ESP_OK; }')

    source = replace_once(source, '    strlcpy((char *)wifi_config.sta.ssid, ssid.c_str(), 32);\n    strlcpy((char *)wifi_config.sta.password, password.c_str(), 64);',
        '    if (!network_setup::Valid({ssid, password})) return false;\n    memcpy(wifi_config.sta.ssid, ssid.data(), ssid.size());\n    memcpy(wifi_config.sta.password, password.data(), password.size());')
    source = replace_once(source, '            if (!this_->ConnectToWifi(ssid_str, password_str)) {',
        '            if (!network_setup::Valid({ssid_str, password_str}) || !this_->ConnectToWifi(ssid_str, password_str)) {')
    source = replace_once(source, '            std::string password_str = "";',
        '            if (!cJSON_IsString(password_item) || !password_item->valuestring || strlen(password_item->valuestring) > 64) { cJSON_Delete(json); httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid password"); return ESP_OK; }\n            std::string password_str = "";')
    source = replace_once(source, '    ESP_LOGI(TAG, "Save SSID %s %d", ssid.c_str(), ssid.length());',
        '    // No credential or network-name logging.')

    begin = '    // Register the /advanced/submit URI'
    end = '    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &advanced_submit));'
    first = source.index(begin)
    last = source.index(end, first) + len(end)
    source = source[:first] + r'''    // Original advanced fields, fixed keys/types, only during explicit setup.
    httpd_uri_t advanced_submit = {
        .uri = "/advanced/submit",
        .method = HTTP_POST,
        .handler = [](httpd_req_t *req) -> esp_err_t {
            auto respond = [req](const char* status, const char* body) {
                httpd_resp_set_status(req, status);
                httpd_resp_set_type(req, "application/json");
                httpd_resp_set_hdr(req, "Connection", "close");
                httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
                return ESP_OK;
            };
            if (!network_setup::CredentialPersistenceAllowed())
                return respond("403 Forbidden", "{\"success\":false,\"error\":\"Setup session ended\"}");
            if (req->content_len < 2 || req->content_len > 1024)
                return respond("400 Bad Request", "{\"success\":false,\"error\":\"Invalid request size\"}");
            std::string body(req->content_len, '\0');
            size_t received = 0;
            while (received < body.size()) {
                int count = httpd_req_recv(req, body.data() + received, body.size() - received);
                if (count <= 0) return respond("400 Bad Request", "{\"success\":false,\"error\":\"Incomplete request\"}");
                received += count;
            }
            cJSON* json = cJSON_ParseWithLengthOpts(body.c_str(), body.size() + 1, nullptr, true);
            bool valid = cJSON_IsObject(json) && body.find("\\u0000") == std::string::npos && body.find('\0') == std::string::npos;
            auto* this_ = static_cast<WifiConfigurationAp*>(req->user_ctx);
            std::string url = this_->ota_url_;
            int power = this_->max_tx_power_;
            bool remember = this_->remember_bssid_, sleep = this_->sleep_mode_;
            unsigned seen = 0;
            for (auto* item = valid ? json->child : nullptr; item; item = item->next) {
                unsigned bit = 0;
                if (strcmp(item->string, "ota_url") == 0) {
                    bit = 1;
                    if (!cJSON_IsString(item) || !item->valuestring) valid = false;
                    else {
                        url = item->valuestring;
                        if (!network_setup::ValidActivationUrl(url)) valid = false;
                    }
                } else if (strcmp(item->string, "max_tx_power") == 0) {
                    bit = 2;
                    if (!cJSON_IsNumber(item) || !network_setup::ValidTxPower(item->valuedouble)) valid = false;
                    else power = item->valueint;
                } else if (strcmp(item->string, "remember_bssid") == 0) {
                    bit = 4;
                    if (!cJSON_IsBool(item)) valid = false;
                    else remember = cJSON_IsTrue(item);
                } else if (strcmp(item->string, "sleep_mode") == 0) {
                    bit = 8;
                    if (!cJSON_IsBool(item)) valid = false;
                    else sleep = cJSON_IsTrue(item);
                } else valid = false;
                if (seen & bit) valid = false;
                seen |= bit;
            }
            cJSON_Delete(json);
            if (!valid || !seen) return respond("400 Bad Request", "{\"success\":false,\"error\":\"Invalid settings\"}");
            if (!network_setup::CredentialPersistenceAllowed())
                return respond("403 Forbidden", "{\"success\":false,\"error\":\"Setup session ended\"}");
            int8_t old_power = this_->max_tx_power_;
            auto err = (seen & 2) ? esp_wifi_set_max_tx_power(power) : ESP_OK;
            nvs_handle_t handle = 0;
            if (err == ESP_OK) err = nvs_open("wifi", NVS_READWRITE, &handle);
            if (err == ESP_OK && (seen & 1)) err = nvs_set_str(handle, "ota_url", url.c_str());
            if (err == ESP_OK && (seen & 2)) err = nvs_set_i8(handle, "max_tx_power", power);
            if (err == ESP_OK && (seen & 4)) err = nvs_set_u8(handle, "remember_bssid", remember ? 1 : 0);
            if (err == ESP_OK && (seen & 8)) err = nvs_set_u8(handle, "sleep_mode", sleep ? 1 : 0);
            if (err == ESP_OK) err = nvs_commit(handle);
            if (handle) nvs_close(handle);
            if (err != ESP_OK) {
                if (seen & 2) {
                    auto restore = esp_wifi_set_max_tx_power(old_power);
                    if (restore != ESP_OK) ESP_LOGE(TAG, "Could not restore Wi-Fi power: %d", restore);
                }
                return respond("500 Internal Server Error", "{\"success\":false,\"error\":\"Could not save settings\"}");
            }
            this_->ota_url_ = url;
            this_->max_tx_power_ = power;
            this_->remember_bssid_ = remember;
            this_->sleep_mode_ = sleep;
            // Saving the activation endpoint never downloads firmware.
            return respond("200 OK", "{\"success\":true}");
        },
        .user_ctx = this
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server_, &advanced_submit));''' + source[last:]
    source = replace_once(source,
        '            ESP_LOGI(TAG, "SmartConfig SSID: %s, Password: %s", ssid, password);',
        '            // Never log Wi-Fi credentials.')
    source = replace_once(source, '            self->Save(ssid, password);',
        '            // SmartConfig credentials are not saved before a verified connection.')
    # Suppress network names in logs; local setup UI still receives the scan list.
    lines = source.splitlines(keepends=True)
    clean = []; index = 0
    while index < len(lines):
        line = lines[index]
        if 'ESP_LOG' in line and ('%s' in line) and ('SSID' in line or 'WiFi' in line):
            while ');' not in lines[index]: index += 1
            clean.append('    // Wi-Fi name omitted from logs.\n')
        else: clean.append(line)
        index += 1
    source = ''.join(clean)
    (output / 'stock_wifi_configuration_ap.cc').write_text(source)

    source = pinned(wifi / 'ssid_manager.cc', 'ssid_manager.cc')
    source = '#include <apps/network_setup/persistence_gate.h>\n#include <apps/network_setup/wifi_credentials.hpp>\n' + source
    source = replace_once(source, '    ssid_list_.clear();\n\n    // Load ssid', """    ssid_list_.clear();
    nvs_handle_t saved = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &saved) == ESP_OK) {
        size_t size = 0;
        if (nvs_get_blob(saved, "config_v1", nullptr, &size) == ESP_OK && size <= 1024) {
            std::vector<uint8_t> blob(size);
            std::vector<network_setup::Credential> credentials;
            if (nvs_get_blob(saved, "config_v1", blob.data(), &size) == ESP_OK &&
                network_setup::Decode(blob.data(), size, credentials)) {
                for (const auto& item : credentials) ssid_list_.push_back({item.ssid, item.password});
                nvs_close(saved);
                return;
            }
        }
        nvs_close(saved);
    }
    // Legacy keys remain untouched and serve as a fallback after a damaged blob.
    // Load ssid""")
    source = replace_once(source, '        ssid_list_.push_back({ssid, password});',
        '        if (network_setup::Valid({ssid, password})) ssid_list_.push_back({ssid, password});')
    source = replace_region(source, 'void SsidManager::SaveToNvs() {',
        'void SsidManager::AddSsid(const std::string& ssid, const std::string& password) {',
        """void SsidManager::SaveToNvs() {
    if (!network_setup::CredentialPersistenceAllowed()) return;
    std::vector<network_setup::Credential> credentials;
    for (const auto& item : ssid_list_) credentials.push_back({item.ssid, item.password});
    std::vector<uint8_t> blob;
    if (!network_setup::Encode(credentials, blob)) {
        network_setup::NotifyCredentialsSaved(false); return;
    }
    nvs_handle_t handle = 0;
    auto result = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (result == ESP_OK) {
        result = nvs_set_blob(handle, "config_v1", blob.data(), blob.size());
        if (result == ESP_OK) result = nvs_commit(handle);
        nvs_close(handle);
    }
    // One atomic NVS blob update; legacy Wi-Fi, calibration and account keys stay intact.
    network_setup::NotifyCredentialsSaved(result == ESP_OK);
}""")
    source = replace_region(source, 'void SsidManager::AddSsid(const std::string& ssid, const std::string& password) {',
        'void SsidManager::RemoveSsid(int index) {', """void SsidManager::AddSsid(const std::string& ssid, const std::string& password) {
    if (!network_setup::Valid({ssid, password})) { network_setup::NotifyCredentialsSaved(false); return; }
    for (auto iter = ssid_list_.begin(); iter != ssid_list_.end(); ++iter) {
        if (iter->ssid == ssid) { ssid_list_.erase(iter); break; }
    }
    if (ssid_list_.size() >= MAX_WIFI_SSID_COUNT) ssid_list_.pop_back();
    ssid_list_.insert(ssid_list_.begin(), {ssid, password});
    SaveToNvs();
}""")
    (output / 'stock_ssid_manager.cc').write_text(source)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--xiaozhi', type=Path, required=True)
    parser.add_argument('--wifi', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    generate(args.xiaozhi, args.wifi, args.output)
