// Companion variant of pinned Xiaozhi MIT settings implementation: explicit feature-scoped persistent settings.
#include "settings.h"

#include <esp_log.h>
#include <nvs_flash.h>
#include <companion/audio_owner.hpp>
#include <companion/settings_policy.hpp>
#include <apps/network_setup/persistence_gate.h>
#include <map>
#include <mutex>

namespace {
std::mutex overlay_mutex;
std::map<std::string, std::string> session_strings;
std::map<std::string, int32_t> session_ints;
std::string overlay_key(const std::string& ns, const std::string& key) { return ns + ":" + key; }
bool persistent_namespace(const std::string& ns) {
 return companion::SettingsNamespaceAllowed(ns) || (ns=="app_config"&&network_setup::CredentialPersistenceAllowed());
}
bool persistent_key(const std::string& ns,const std::string& key) {
 return companion::SettingsWriteAllowed(ns,key)||(ns=="app_config"&&key=="is_configed"&&network_setup::CredentialPersistenceAllowed());
}
bool protocol_namespace(const std::string& ns) { return ns == "mqtt" || ns == "websocket"; }
bool protocol_write(const std::string& ns) {
    return protocol_namespace(ns) && companion::CurrentAudioOwner() == companion::AudioOwner::Stock && companion::CurrentSettingsScope()!=companion::SettingsScope::StockRuntime;
}
}

#define TAG "Settings"

Settings::Settings(const std::string& ns, bool read_write) : ns_(ns), read_write_(read_write && persistent_namespace(ns)) {
    nvs_open(ns.c_str(), read_write_ ? NVS_READWRITE : NVS_READONLY, &nvs_handle_);
}

Settings::~Settings() {
    if (nvs_handle_ != 0) {
        if (read_write_ && dirty_) {
            ESP_ERROR_CHECK(nvs_commit(nvs_handle_));
        }
        nvs_close(nvs_handle_);
    }
}

std::string Settings::GetString(const std::string& key, const std::string& default_value) {
    if (protocol_namespace(ns_) && companion::CurrentSettingsScope()!=companion::SettingsScope::StockRuntime) {
        std::lock_guard<std::mutex> lock(overlay_mutex);
        auto found = session_strings.find(overlay_key(ns_, key));
        if (found != session_strings.end()) return found->second;
    }
    if (nvs_handle_ == 0) {
        return default_value;
    }

    size_t length = 0;
    if (nvs_get_str(nvs_handle_, key.c_str(), nullptr, &length) != ESP_OK) {
        return default_value;
    }

    std::string value;
    value.resize(length);
    ESP_ERROR_CHECK(nvs_get_str(nvs_handle_, key.c_str(), value.data(), &length));
    while (!value.empty() && value.back() == '\0') {
        value.pop_back();
    }
    return value;
}

void Settings::SetString(const std::string& key, const std::string& value) {
    if (protocol_write(ns_)) {
        std::lock_guard<std::mutex> lock(overlay_mutex);
        session_strings[overlay_key(ns_, key)] = value;
        return;
    }
    if (read_write_ && persistent_key(ns_,key)) {
        ESP_ERROR_CHECK(nvs_set_str(nvs_handle_, key.c_str(), value.c_str()));
        dirty_ = true;
    } else {
        ESP_LOGW(TAG, "Namespace %s is not open for writing", ns_.c_str());
    }
}

int32_t Settings::GetInt(const std::string& key, int32_t default_value) {
    if (protocol_namespace(ns_) && companion::CurrentSettingsScope()!=companion::SettingsScope::StockRuntime) {
        std::lock_guard<std::mutex> lock(overlay_mutex);
        auto found = session_ints.find(overlay_key(ns_, key));
        if (found != session_ints.end()) return found->second;
    }
    if (nvs_handle_ == 0) {
        return default_value;
    }

    int32_t value;
    if (nvs_get_i32(nvs_handle_, key.c_str(), &value) != ESP_OK) {
        return default_value;
    }
    return value;
}

void Settings::SetInt(const std::string& key, int32_t value) {
    if (protocol_write(ns_)) {
        std::lock_guard<std::mutex> lock(overlay_mutex);
        session_ints[overlay_key(ns_, key)] = value;
        return;
    }
    if (read_write_ && persistent_key(ns_,key)) {
        ESP_ERROR_CHECK(nvs_set_i32(nvs_handle_, key.c_str(), value));
        dirty_ = true;
    } else {
        ESP_LOGW(TAG, "Namespace %s is not open for writing", ns_.c_str());
    }
}

bool Settings::GetBool(const std::string& key, bool default_value) {
    if (nvs_handle_ == 0) {
        return default_value;
    }

    uint8_t value;
    if (nvs_get_u8(nvs_handle_, key.c_str(), &value) != ESP_OK) {
        return default_value;
    }
    return value != 0;
}

void Settings::SetBool(const std::string& key, bool value) {
    if (read_write_ && persistent_key(ns_,key)) {
        ESP_ERROR_CHECK(nvs_set_u8(nvs_handle_, key.c_str(), value ? 1 : 0));
        dirty_ = true;
    } else {
        ESP_LOGW(TAG, "Namespace %s is not open for writing", ns_.c_str());
    }
}

void Settings::EraseKey(const std::string& key) {
    if (read_write_ && persistent_key(ns_,key)) {
        auto ret = nvs_erase_key(nvs_handle_, key.c_str());
        if (ret != ESP_ERR_NVS_NOT_FOUND) {
            ESP_ERROR_CHECK(ret);
            dirty_ = true;
        }
    } else {
        ESP_LOGW(TAG, "Namespace %s is not open for writing", ns_.c_str());
    }
}

void Settings::EraseAll() {
    if (read_write_ && ns_=="account" && companion::CurrentSettingsScope()==companion::SettingsScope::Account) {
        ESP_ERROR_CHECK(nvs_erase_all(nvs_handle_));
        dirty_ = true;
    } else {
        ESP_LOGW(TAG, "Namespace %s is not open for writing", ns_.c_str());
    }
}
