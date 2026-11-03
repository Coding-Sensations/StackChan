// SPDX-License-Identifier: MIT
#include "wifi_station.h"
#include <companion/station_authority.hpp>
#include <companion/station_event_barrier.hpp>
#include <apps/network_setup/wifi_credentials.hpp>
#include <apps/network_setup/persistence_gate.h>
#include <cstring>
#include <esp_wifi.h>
#include <esp_wifi_default.h>
#include <esp_system.h>
#include <esp_timer.h>
#include "ssid_manager.h"

#define WIFI_EVENT_CONNECTED BIT0
#define MAX_RECONNECT_COUNT 1
namespace {
void Wipe(std::string& value) {
    volatile char* bytes = value.empty() ? nullptr : value.data();
    for (size_t i = 0; i < value.size(); ++i) bytes[i] = 0;
    value.clear();
}
void WipeConfig(wifi_config_t& config) {
    volatile unsigned char* bytes = reinterpret_cast<volatile unsigned char*>(&config);
    for (size_t i = 0; i < sizeof(config); ++i) bytes[i] = 0;
}
}
StackChanWifiStation::StackChanWifiStation() { event_group_ = xEventGroupCreate(); }
StackChanWifiStation::~StackChanWifiStation() {
    // The config-server owner retains this object until checked stop succeeds.
    configASSERT(StopChecked());
    if(event_group_)vEventGroupDelete(event_group_);
}
void StackChanWifiStation::WipePendingLocked() {
    Wipe(pending_password_);
    pending_ = associated_ = awaiting_disconnect_ = committing_ = false;
    is_connecting_ = false;
    reconnect_count_ = 0;
}
bool StackChanWifiStation::AddAuth(const std::string& ssid, const std::string& password, const WifiBandInfo& band) {
    network_setup::Credential credential{ssid, password};
    bool valid = network_setup::Valid(credential);
    Wipe(credential.password);
    Wipe(credential.ssid);
    if (!valid || !network_setup::CredentialPersistenceAllowed()||!callbacks_enabled_||!companion::network::Authority().Accepts(companion::network::Owner::Vendor,network_generation_)) return false;
    wifi_ap_record_t old_connection{};
    bool disconnect_first = esp_wifi_sta_get_ap_info(&old_connection) == ESP_OK;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!is_started_ || pending_ || committing_) return false;
        WipePendingLocked();
        ssid_ = ssid;
        pending_password_ = password;
        band_ = band;
        connection_started_at_ = esp_timer_get_time();
        pending_ = is_connecting_ = true;
        awaiting_disconnect_ = disconnect_first;
    }
    xEventGroupClearBits(event_group_, WIFI_EVENT_CONNECTED);
    if (on_connect_) on_connect_(ssid);
    if (disconnect_first) {
        // The old link must disconnect before a new request can qualify an IP,
        // including a different password submitted for the same SSID.
        if (esp_wifi_disconnect() != ESP_OK) { FailPending(); return false; }
        return true;
    }
    return ApplyPending();
}
bool StackChanWifiStation::ApplyPending() {
    if (!network_setup::CredentialPersistenceAllowed()||!callbacks_enabled_||!companion::network::Authority().Accepts(companion::network::Owner::Vendor,network_generation_)) { FailPending(); return false; }
    wifi_config_t config{};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!pending_ || awaiting_disconnect_) return false;
        std::memcpy(config.sta.ssid, ssid_.data(), ssid_.size());
        std::memcpy(config.sta.password, pending_password_.data(), pending_password_.size());
    }
    // Inspect all matching APs before choosing the strongest signal.
    config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    auto result = esp_wifi_set_config(WIFI_IF_STA, &config);
    WipeConfig(config);
    if (result != ESP_OK) {
        FailPending({"invalid_configuration", "Wi-Fi configuration was rejected.", 0, result});
        return false;
    }
    result = esp_wifi_connect();
    if (result != ESP_OK) {
        FailPending({"connect_failed", "Could not start Wi-Fi connection.", 0, result});
        return false;
    }
    return true;
}
void StackChanWifiStation::FailPending(WifiConnectionFailure failure, bool only_if_uncommitted) {
    std::string failed_ssid;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!pending_ && !is_connecting_) return;
        // Poll's snapshot can race an IP event claiming persistence. The
        // timeout and commit must claim the attempt under this same mutex.
        if (only_if_uncommitted && committing_) return;
        failed_ssid = ssid_;
        WipePendingLocked();
        Wipe(ssid_);
        Wipe(ip_address_);
    }
    xEventGroupClearBits(event_group_, WIFI_EVENT_CONNECTED);
    network_setup::NotifyCredentialsSaved(false);
    wifi_config_t empty{};
    esp_wifi_disconnect();
    esp_wifi_set_config(WIFI_IF_STA, &empty); // RAM only: discard failed credentials
    if (on_connect_failed_) on_connect_failed_(failed_ssid, failure);
    Wipe(failed_ssid);
}
void StackChanWifiStation::Poll() {
    bool expired, associated;
    WifiBandInfo band;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        expired = pending_ && !committing_ && esp_timer_get_time() - connection_started_at_ >= 30000000;
        associated = associated_;
        band = band_;
    }
    if (!expired) return;
    if (associated) FailPending({"dhcp_timeout", "Connected to the router, but no IP address was assigned. Check router DHCP settings."}, true);
    else if (band.has_5ghz_hint()) FailPending(wifiNot2G4Failure(), true);
    else FailPending({"connection_timeout", "Wi-Fi connection timed out. Check SSID, password and router range."}, true);
}
void StackChanWifiStation::Stop(){(void)StopChecked();}
bool StackChanWifiStation::StopChecked(){
    if(!driver_touched_&&!instance_any_id_&&!instance_got_ip_&&!station_netif_)return true;
    if(companion::network::Authority().Current()!=companion::network::Owner::Vendor)return false;
    callbacks_enabled_=false;
    {std::lock_guard<std::mutex> lock(mutex_);is_started_=false;WipePendingLocked();}
    if(instance_any_id_){if(esp_event_handler_instance_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,instance_any_id_)!=ESP_OK)return false;instance_any_id_=nullptr;}
    if(instance_got_ip_){if(esp_event_handler_instance_unregister(IP_EVENT,IP_EVENT_STA_GOT_IP,instance_got_ip_)!=ESP_OK)return false;instance_got_ip_=nullptr;}
    auto error=esp_wifi_stop();
    if(error!=ESP_ERR_WIFI_NOT_INIT){
        if(error!=ESP_OK&&error!=ESP_ERR_WIFI_NOT_STARTED)return false;
        if(esp_wifi_set_mode(WIFI_MODE_NULL)!=ESP_OK)return false;
    }
    wifi_mode_t mode{};error=esp_wifi_get_mode(&mode);
    if(error!=ESP_ERR_WIFI_NOT_INIT&&(error!=ESP_OK||mode!=WIFI_MODE_NULL))return false;
    if(!companion::network::DrainStationEvents())return false;
    if(station_netif_){esp_netif_destroy_default_wifi(station_netif_);station_netif_=nullptr;}
    if(!companion::network::DrainStationEvents())return false;
    // Initialized driver is shared boot state; vendor manager may already own its objects.
    driver_touched_=false;network_generation_=0;return true;
}
void StackChanWifiStation::OnConnect(std::function<void(const std::string&)> callback) { on_connect_ = std::move(callback); }
void StackChanWifiStation::OnConnected(std::function<void(const std::string&)> callback) { on_connected_ = std::move(callback); }
void StackChanWifiStation::OnConnectFailed(std::function<void(const std::string&)> callback) {
    on_connect_failed_ = [callback = std::move(callback)](const std::string& ssid, const WifiConnectionFailure&) { if (callback) callback(ssid); };
}
void StackChanWifiStation::OnConnectFailed(std::function<void(const std::string&, const WifiConnectionFailure&)> callback) { on_connect_failed_ = std::move(callback); }
bool StackChanWifiStation::Start(){
    auto& authority=companion::network::Authority();
    if(!network_setup::CredentialPersistenceAllowed()||!event_group_||authority.Current()!=companion::network::Owner::Vendor||!authority.CanAcquire(companion::network::Owner::Vendor))return false;
    {std::lock_guard<std::mutex> lock(mutex_);if(is_started_)return true;}
    if(!StopChecked())return false;
    wifi_mode_t mode{};auto error=esp_wifi_get_mode(&mode);
    if(error!=ESP_ERR_WIFI_NOT_INIT&&(error!=ESP_OK||mode!=WIFI_MODE_NULL))return false;
    driver_touched_=true;network_generation_=authority.Generation();
    error=esp_netif_init();if(error!=ESP_OK&&error!=ESP_ERR_INVALID_STATE)return false;
    error=esp_event_loop_create_default();if(error!=ESP_OK&&error!=ESP_ERR_INVALID_STATE)return false;
    if(esp_wifi_get_mode(&mode)==ESP_ERR_WIFI_NOT_INIT){wifi_init_config_t config=WIFI_INIT_CONFIG_DEFAULT();config.nvs_enable=false;if(esp_wifi_init(&config)!=ESP_OK)return false;}
    station_netif_=esp_netif_create_default_wifi_sta();if(!station_netif_)return false;
    if(esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&StackChanWifiStation::WifiEventHandler,this,&instance_any_id_)!=ESP_OK)return false;
    if(esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP,&StackChanWifiStation::IpEventHandler,this,&instance_got_ip_)!=ESP_OK)return false;
    callbacks_enabled_=true;
    if(esp_wifi_set_storage(WIFI_STORAGE_RAM)!=ESP_OK||esp_wifi_set_mode(WIFI_MODE_STA)!=ESP_OK||esp_wifi_start()!=ESP_OK){callbacks_enabled_=false;return false;}
    std::lock_guard<std::mutex> lock(mutex_);is_started_=true;return true;
    // Only a validated phone submission calls AddAuth; no automatic connection.
}
bool StackChanWifiStation::WaitForConnected(int timeout_ms) {
    return (xEventGroupWaitBits(event_group_, WIFI_EVENT_CONNECTED, pdFALSE, pdFALSE,
        pdMS_TO_TICKS(timeout_ms)) & WIFI_EVENT_CONNECTED) != 0;
}
int8_t StackChanWifiStation::GetRssi() {
    wifi_ap_record_t record{};
    return esp_wifi_sta_get_ap_info(&record) == ESP_OK ? record.rssi : 0;
}
uint8_t StackChanWifiStation::GetChannel() {
    wifi_ap_record_t record{};
    return esp_wifi_sta_get_ap_info(&record) == ESP_OK ? record.primary : 0;
}
bool StackChanWifiStation::IsConnected() { return (xEventGroupGetBits(event_group_) & WIFI_EVENT_CONNECTED) != 0; }
void StackChanWifiStation::SetPowerSaveMode(bool enabled) { esp_wifi_set_ps(enabled ? WIFI_PS_MIN_MODEM : WIFI_PS_NONE); }
void StackChanWifiStation::WifiEventHandler(void* arg, esp_event_base_t, int32_t id, void* data) {
    auto self = static_cast<StackChanWifiStation*>(arg);
    if(!self->callbacks_enabled_||!companion::network::Authority().Accepts(companion::network::Owner::Vendor,self->network_generation_))return;
    if (id == WIFI_EVENT_STA_CONNECTED) {
        auto event = static_cast<wifi_event_sta_connected_t*>(data);
        std::lock_guard<std::mutex> lock(self->mutex_);
        if (self->pending_ && !self->awaiting_disconnect_ && event &&
            event->ssid_len == self->ssid_.size() &&
            std::memcmp(event->ssid, self->ssid_.data(), event->ssid_len) == 0) self->associated_ = true;
        return;
    }
    if (id != WIFI_EVENT_STA_DISCONNECTED) return;
    xEventGroupClearBits(self->event_group_, WIFI_EVENT_CONNECTED);
    bool apply = false, retry = false, fail = false;
    {
        std::lock_guard<std::mutex> lock(self->mutex_);
        self->associated_ = false;
        if (!self->pending_ || self->committing_) return;
        if (self->awaiting_disconnect_) {
            self->awaiting_disconnect_ = false;
            apply = true;
        } else if (self->reconnect_count_ < MAX_RECONNECT_COUNT) {
            ++self->reconnect_count_;
            retry = true;
        } else fail = true;
    }
    if (apply) self->ApplyPending();
    if (retry && esp_wifi_connect() != ESP_OK) self->FailPending();
    if (fail) {
        const auto* event = static_cast<wifi_event_sta_disconnected_t*>(data);
        WifiBandInfo band;
        { std::lock_guard<std::mutex> lock(self->mutex_); band = self->band_; }
        self->FailPending(wifiDisconnectFailure(event ? event->reason : 0, band));
    }
}
void StackChanWifiStation::IpEventHandler(void* arg, esp_event_base_t, int32_t id, void* data) {
    if (id != IP_EVENT_STA_GOT_IP || !data) return;
    auto self = static_cast<StackChanWifiStation*>(arg);
    if(!self->callbacks_enabled_||!companion::network::Authority().Accepts(companion::network::Owner::Vendor,self->network_generation_))return;
    auto event = static_cast<ip_event_got_ip_t*>(data);
    wifi_ap_record_t record{};
    if (event->esp_netif != self->station_netif_ || esp_wifi_sta_get_ap_info(&record) != ESP_OK) return;
    std::string selected_ssid, selected_password;
    {
        std::lock_guard<std::mutex> lock(self->mutex_);
        auto length = strnlen(reinterpret_cast<const char*>(record.ssid), sizeof(record.ssid));
        if (!self->pending_ || !self->associated_ || self->committing_ || self->awaiting_disconnect_ ||
            !network_setup::CredentialPersistenceAllowed() || length != self->ssid_.size() ||
            std::memcmp(record.ssid, self->ssid_.data(), length) != 0) return;
        self->committing_ = true;
        selected_ssid = self->ssid_;
        selected_password = self->pending_password_;
    }
    // Only a successful association/IP qualifies persistence. The backend writes
    // the explicitly selected Wi-Fi blob atomically and reports commit success.
    network_setup::NotifyCredentialsSaved(false);
    SsidManager::GetInstance().AddSsid(selected_ssid, selected_password);
    Wipe(selected_password);
    bool saved = network_setup::CredentialSaveSucceeded();
    if (!saved) { self->FailPending(); return; }
    {
        std::lock_guard<std::mutex> lock(self->mutex_);
        char address[16];
        esp_ip4addr_ntoa(&event->ip_info.ip, address, sizeof(address));
        self->ip_address_ = address;
        self->WipePendingLocked();
    }
    xEventGroupSetBits(self->event_group_, WIFI_EVENT_CONNECTED);
    if (self->on_connected_) self->on_connected_(selected_ssid);
}
