#include "personal_binding.hpp"
#include "local_consent.hpp"
#include "local_wake_controller.hpp"
#include "boot.h"
#include "station_authority.hpp"
#include "checked_station_stop.hpp"
#include "station_event_barrier.hpp"
#include <esp_wifi_default.h>
#include <esp_sntp.h>
#include <memory>
#include <new>
#include "remembered_service.hpp"
#include "remembered_intent_route.hpp"
#include <apps/led_expression/led_service_api.h>
#include "feature_link.h"
#include "diagnostics.h"
#include "diagnostic_usb_export.hpp"
#include "audio_owner.hpp"
#include "lease_reactive_avatar.hpp"
#include "companion_reactive_view.hpp"
#include "provider_room_runtime.h"
#include "companion_character_layout.hpp"
#include "reactive_avatar_service.hpp"
#include "closing_view_policy.hpp"
#include "usb_owner.hpp"
#include "numeric_usb.hpp"
#include "numeric_pairing.h"
#include "persistent_pairing.hpp"
#include "enrollment_observation.hpp"
#include "public_status_owner.hpp"
#include "companion_header_layout.hpp"
#include "companion_details_view.hpp"
#include "companion_initial_status.hpp"
#ifndef STACKCHAN_COMPANION_REMEMBERED_RUNTIME
#define STACKCHAN_COMPANION_REMEMBERED_RUNTIME 0
#endif
#include "pair_connection_consent.hpp"
#include "pair_ui_action.hpp"
#include "local_tls.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "connect_gate.hpp"
#include <driver/usb_serial_jtag.h>
#include <usb_composite_v2/startup.h>
#include <usb_composite_v2/cdc_session.hpp>
#include <esp_mac.h>
#include <esp_random.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <cstdio>
#include <mbedtls/sha256.h>
#ifndef STACKCHAN_COMPANION_TRUSTED_USB_PROVISIONING
#define STACKCHAN_COMPANION_TRUSTED_USB_PROVISIONING 0
#endif
#ifndef STACKCHAN_COMPANION_WIRELESS_PAIRING
#define STACKCHAN_COMPANION_WIRELESS_PAIRING 0
#endif
#include "companion_socket.h"
#include <hal/hal.h>
#include <board.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include "saved_wifi_owner.hpp"
#include "wifi_survey.hpp"
#include <array>
#include <esp_event.h>
#include <esp_netif.h>
#include <lwip/inet.h>
#include <nvs.h>
#include <ssid_manager.h>
#include <wifi_manager.h>
#include <apps/network_setup/wifi_credentials.hpp>
#include <apps/network_setup/persistence_gate.h>
#include <lvgl.h>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <mutex>
#include <optional>
namespace companion {
namespace {
std::mutex grant_mutex;
std::optional<AuthorizedGrant> pending_grant;
std::atomic<bool> remember_requested{false},forget_requested{false};
std::atomic<uint32_t> listen_epoch_requested{0};
std::atomic<uint64_t> remembered_listen_requested{0};
std::atomic<bool> physical_mute_requested{false};
std::atomic<bool> confirm_requested{false},reject_requested{false},diagnostic_requested{false};
std::atomic<bool> public_status_requested{false},public_status_close{false},public_status_next{false};
std::mutex wifi_start_mutex;
SavedWifiOwner saved_wifi_owner;
esp_netif_t* saved_station_netif=nullptr;
esp_event_handler_instance_t saved_wifi_handler=nullptr,saved_ip_handler=nullptr;
struct SavedWifiCallback {uint64_t generation;};
std::unique_ptr<SavedWifiCallback> saved_callback;
std::atomic<int> saved_stop_job{0}; // 0 retry/idle, 1 retained worker
bool AcquireSavedStation(){
    auto& authority=network::Authority();
    if(authority.Current()==network::Owner::None){
        wifi_mode_t mode{};const auto error=esp_wifi_get_mode(&mode);
        if(error!=ESP_ERR_WIFI_NOT_INIT&&(error!=ESP_OK||mode!=WIFI_MODE_NULL))return false;
    }
    return authority.Acquire(network::Owner::Saved);
}
std::atomic<bool> pair_requested{false}, connect_requested{false}, listen_requested{false}, mute_requested{false}, connected{false};
// ESP-IDF requires interrupt install/uninstall on the same core. The shell
// task is pinned to CPU0; the helper also enforces that contract at runtime.
struct LegacyUsbSdkDriver {
    bool OnOwnerCore() const { return xPortGetCoreID()==0; }
    bool Installed() const { return usb_serial_jtag_is_driver_installed(); }
    bool Install() { return InstallWithTxCapacity(64); }
    bool InstallDiagnostic() { return InstallWithTxCapacity(DiagnosticUsbMaxBytes); }
    bool InstallWithTxCapacity(size_t capacity) {
        usb_serial_jtag_driver_config_t config={};config.rx_buffer_size=1024;config.tx_buffer_size=capacity;
        return usb_serial_jtag_driver_install(&config)==ESP_OK;
    }
    bool Uninstall() { return usb_serial_jtag_driver_uninstall()==ESP_OK; }
    int Read(uint8_t* bytes,size_t capacity) { return usb_serial_jtag_read_bytes(bytes,capacity,0); }
    int WriteAck(const uint8_t* bytes,size_t size) { return usb_serial_jtag_write_bytes(bytes,size,pdMS_TO_TICKS(100)); }
    bool FlushAck() { return usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(100))==ESP_OK; }
};
// Same approved protocol owners below; only their SDK boundary is selected.
// The default never selects TinyUSB. A refused composite request never falls
// back into an uncertain live Serial/JTAG PHY.
struct UsbSdkDriver {
    LegacyUsbSdkDriver legacy_;
    std::optional<usb_composite_v2::CdcSessionDriver<usb_composite_v2::TinyUsbBridge>> cdc_;
    bool Composite()const{return usb_composite_v2::CompositeRequested();}
    auto* Cdc(){
        auto* bridge=usb_composite_v2::CompositeBridge();
        if(!bridge)return static_cast<usb_composite_v2::CdcSessionDriver<usb_composite_v2::TinyUsbBridge>*>(nullptr);
        if(!cdc_)cdc_.emplace(*bridge);
        return &*cdc_;
    }
    bool OnOwnerCore()const{return legacy_.OnOwnerCore();}
    bool Installed(){if(!Composite())return legacy_.Installed();auto* c=Cdc();return c&&c->Installed();}
    bool Install(){if(!Composite())return legacy_.Install();auto* c=Cdc();return c&&c->Install();}
    bool InstallDiagnostic(){if(!Composite())return legacy_.InstallDiagnostic();auto* c=Cdc();return c&&c->InstallDiagnostic();}
    bool Uninstall(){if(!Composite())return legacy_.Uninstall();return !cdc_||cdc_->Uninstall();}
    int Read(uint8_t* p,size_t n){if(!Composite())return legacy_.Read(p,n);auto* c=Cdc();return c?c->Read(p,n):-1;}
    int WriteAck(const uint8_t* p,size_t n){if(!Composite())return legacy_.WriteAck(p,n);auto* c=Cdc();return c?c->WriteAck(p,n):-1;}
    bool FlushAck(){if(!Composite())return legacy_.FlushAck();auto* c=Cdc();return c&&c->FlushAck();}
};
class UsbProvisioner {
    UsbSdkDriver driver_;
    const uint32_t boot_nonce_=diagnostic::BootNonce();
    UsbProvisioningOwner<UsbSdkDriver> owner_{driver_};
    NumericUsbOwner<UsbSdkDriver> numeric_owner_{driver_};
    bool numeric_=false;
    const char* open_message_="Pair unavailable: provisioning disabled";
    DiagnosticUsbExport<UsbSdkDriver> diagnostics_{driver_};
public:
    bool Close() {
        bool closed=diagnostics_.Close()&&numeric_owner_.Close()&&owner_.Close();
        if(!closed) ESP_LOGW("CompanionUSB","USB owner cleanup failed (%d)",static_cast<int>(owner_.LastError()));
        return closed;
    }
    bool Active() const { return diagnostics_.OwnsDriver()||(numeric_?numeric_owner_.Active():owner_.Active()); }
    bool Stopped()const{return !diagnostics_.OwnsDriver()&&!numeric_owner_.OwnsDriver()&&!owner_.OwnsDriver();}
    bool ExportDiagnostics(bool eligible){if(!eligible)return diagnostics_.Export(false,nullptr,0);auto frame=diagnostic::Snapshot();return diagnostics_.Export(true,frame.bytes.data(),frame.size);}
    const char* ExportMessage()const{return DiagnosticExportMessage(diagnostics_.LastResult());}
    const char* OpenMessage()const{return open_message_;}
    bool Numeric()const{return numeric_;}
    uint32_t BootNonce()const{return boot_nonce_?boot_nonce_:1;}
    NumericUsbRead PollNumeric(NumericUsbRequest& request){return numeric_owner_.Poll(esp_timer_get_time()/1000,request);}
    bool Open() {
        open_message_="Pair unavailable: provisioning disabled";
        if(!STACKCHAN_COMPANION_TRUSTED_USB_PROVISIONING||!PersonalBindingAvailable())return false;
        open_message_="Pair unavailable: USB cleanup failed";
        if(!Close())return false;
        const char* ca=companion_local_ca_pem();const char* host=companion_local_tls_host();
        numeric_=PersonalBindingAvailable()&&ca&&*ca&&host&&std::strcmp(host,STACKCHAN_PIN_HOST)==0;
        if(!numeric_)return false;
        if(numeric_){
            unsigned char digest[32];
            if(mbedtls_sha256(reinterpret_cast<const unsigned char*>(ca),std::strlen(ca),digest,0))return false;
            char hash[65];const char* hex="0123456789abcdef";
            for(unsigned i=0;i<32;++i){hash[2*i]=hex[digest[i]>>4];hash[2*i+1]=hex[digest[i]&15];}hash[64]=0;
            if(std::strcmp(hash,STACKCHAN_PIN_CA_SHA256))return false;
            bool opened=numeric_owner_.Open(esp_timer_get_time()/1000,BootNonce(),static_cast<uint32_t>(esp_reset_reason()));
            open_message_=NumericUsbOpenMessage(numeric_owner_.LastOpenError());
            return opened;
        }
        open_message_="Pair unavailable: legacy provisioning failed";
        uint8_t mac[6];if(esp_read_mac(mac,ESP_MAC_WIFI_STA)!=ESP_OK)return false;
        char device[32];std::snprintf(device,sizeof(device),"stackchan-%02x%02x%02x%02x%02x%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
        return owner_.Open(esp_timer_get_time()/1000,device,true,boot_nonce_?boot_nonce_:1,static_cast<uint32_t>(esp_reset_reason()));
    }
    ProvisioningRead Poll(ProvisioningFields& fields) { return owner_.Poll(esp_timer_get_time()/1000,fields); }
    bool CompleteAccepted(bool staged,const std::string& request) { return owner_.CompleteAccepted(staged,request); }
};
std::atomic<bool> wifi_survey_busy{false};
std::atomic<bool> survey_done{false};std::atomic<bool> survey_success{false};
void wifi_event(void* arg, esp_event_base_t base, int32_t id, void* data) {
    const auto* callback=static_cast<SavedWifiCallback*>(arg);
    if(!callback||network::Authority().Current()!=network::Owner::Saved||callback->generation!=network::Authority().Generation())return;
    // The owned scan must still receive its completion while cancellation is pending.
    if(base==WIFI_EVENT&&id==WIFI_EVENT_SCAN_DONE){const auto* done=static_cast<wifi_event_sta_scan_done_t*>(data);survey_success=done&&done->status==0;survey_done=true;return;}
    if(!network::Authority().Accepts(network::Owner::Saved,callback->generation))return;
    if(base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) {
        const auto* event=static_cast<ip_event_got_ip_t*>(data);wifi_ap_record_t ap{};
        if(!event||event->esp_netif!=saved_station_netif||esp_wifi_sta_get_ap_info(&ap)!=ESP_OK)return;
        connected=true;saved_wifi_owner.GotIp();
    }
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED) {
        auto& trace=enrollment_observation::State();
        if(trace.Recording()||trace.Get().pair==enrollment_observation::PairStage::WifiWait)trace.Pair(enrollment_observation::PairStage::WifiDisconnected);
        connected=false;if(!RememberedTransportActive())mute_requested=true;
        const auto* event=static_cast<wifi_event_sta_disconnected_t*>(data);
        saved_wifi_owner.Disconnected(event?event->reason:0);
    }
}
bool InitializeOwnedStation(wifi_config_t* wifi){
    saved_wifi_owner.Touched();
    auto error=esp_netif_init();if(error!=ESP_OK&&error!=ESP_ERR_INVALID_STATE)return false;
    error=esp_event_loop_create_default();if(error!=ESP_OK&&error!=ESP_ERR_INVALID_STATE)return false;
    wifi_mode_t mode{};error=esp_wifi_get_mode(&mode);
    if(error==ESP_ERR_WIFI_NOT_INIT){wifi_init_config_t config=WIFI_INIT_CONFIG_DEFAULT();config.nvs_enable=false;if(esp_wifi_init(&config)!=ESP_OK)return false;}
    else if(error!=ESP_OK||mode!=WIFI_MODE_NULL)return false;
    saved_station_netif=esp_netif_create_default_wifi_sta();if(!saved_station_netif)return false;
    saved_callback.reset(new(std::nothrow) SavedWifiCallback{network::Authority().Generation()});if(!saved_callback)return false;
    if(esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event,saved_callback.get(),&saved_wifi_handler)!=ESP_OK)return false;
    if(esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event,saved_callback.get(),&saved_ip_handler)!=ESP_OK)return false;
    if(esp_wifi_set_storage(WIFI_STORAGE_RAM)!=ESP_OK||esp_wifi_set_mode(WIFI_MODE_STA)!=ESP_OK||(wifi&&esp_wifi_set_config(WIFI_IF_STA,wifi)!=ESP_OK)||esp_wifi_start()!=ESP_OK)return false;
    return true;
}
struct SavedStationStopDriver {
    bool UnregisterWifi(){if(!saved_wifi_handler)return true;if(esp_event_handler_instance_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,saved_wifi_handler)!=ESP_OK)return false;saved_wifi_handler=nullptr;return true;}
    bool UnregisterIp(){if(!saved_ip_handler)return true;if(esp_event_handler_instance_unregister(IP_EVENT,IP_EVENT_STA_GOT_IP,saved_ip_handler)!=ESP_OK)return false;saved_ip_handler=nullptr;return true;}
    bool StopDriver(){
        auto error=esp_wifi_stop();if(error==ESP_ERR_WIFI_NOT_INIT)return true;
        if(error!=ESP_OK&&error!=ESP_ERR_WIFI_NOT_STARTED)return false;
        return esp_wifi_set_mode(WIFI_MODE_NULL)==ESP_OK;
    }
    bool DrainEvents(){return network::DrainStationEvents();}
    bool DestroyNetif(){if(saved_station_netif){esp_netif_destroy_default_wifi(saved_station_netif);saved_station_netif=nullptr;}saved_callback.reset();return true;}
    bool VerifyIdle(){wifi_mode_t mode{};const auto error=esp_wifi_get_mode(&mode);return error==ESP_ERR_WIFI_NOT_INIT||(error==ESP_OK&&mode==WIFI_MODE_NULL);}
};

bool StartSavedWifi() {
    std::lock_guard<std::mutex> lock(wifi_start_mutex);
    if(wifi_survey_busy.load())return false;
    const auto& saved=SsidManager::GetInstance().GetSsidList();
    const bool valid=!saved.empty()&&network_setup::Valid({saved.front().ssid,saved.front().password});
    if(!AcquireSavedStation())return false;
    const auto action=saved_wifi_owner.Begin(valid);
    if(action==SavedWifiAction::AlreadyConnected||action==SavedWifiAction::Waiting)return true;
    if(action==SavedWifiAction::Refused)return false;

    wifi_config_t wifi={};
    struct Wipe {wifi_config_t& value;~Wipe(){std::memset(&value,0,sizeof(value));}} wipe{wifi};
    std::memcpy(wifi.sta.ssid,saved.front().ssid.data(),saved.front().ssid.size());
    std::memcpy(wifi.sta.password,saved.front().password.data(),saved.front().password.size());
    if(action==SavedWifiAction::Connect){if(!valid||esp_wifi_set_config(WIFI_IF_STA,&wifi)!=ESP_OK){saved_wifi_owner.ConnectResult(false);return false;}const bool ok=esp_wifi_connect()==ESP_OK;saved_wifi_owner.ConnectResult(ok);return ok;}
    const auto fail=[](){saved_wifi_owner.InitializationFailed();return false;};
    if(!InitializeOwnedStation(&wifi))return fail();
    saved_wifi_owner.DriverReady();
    const bool ok=esp_wifi_connect()==ESP_OK;saved_wifi_owner.ConnectResult(ok);return ok;
}
class SurveyDriver {
public:
 survey::Phase Start(){
  if(!AcquireSavedStation())return survey::Phase::NotReady;
  auto action=saved_wifi_owner.BeginSurvey(false);
  if(action==SavedWifiAction::Refused)return survey::Phase::NotReady;
  if(action==SavedWifiAction::Waiting)return survey::Phase::Busy;
  if(action==SavedWifiAction::Initialize){if(!InitializeOwnedStation(nullptr)){saved_wifi_owner.InitializationFailed();return survey::Phase::NotReady;}saved_wifi_owner.SurveyReady();}
  wifi_mode_t mode{};if(!saved_wifi_owner.DriverIsReady()||esp_wifi_get_mode(&mode)!=ESP_OK||mode!=WIFI_MODE_STA)return survey::Phase::NotReady;
  survey_done=false;survey_success=false;
  wifi_scan_config_t config{};config.show_hidden=true;config.scan_type=WIFI_SCAN_TYPE_PASSIVE;config.scan_time.passive=120;config.home_chan_dwell_time=30;
  const auto error=esp_wifi_scan_start(&config,false);
  return error==ESP_OK?survey::Phase::Scanning:error==ESP_ERR_WIFI_STATE?survey::Phase::Busy:survey::Phase::Failed;
 }
 bool Done()const{return survey_done.load();}bool Success()const{return survey_success.load();}
 bool Stop(){return esp_wifi_scan_stop()==ESP_OK;}
 bool Read(std::array<survey::Ap,survey::MaxAccessPoints>& out,size_t& count,bool& truncated){
  uint16_t total=0,n=survey::MaxAccessPoints;std::array<wifi_ap_record_t,survey::MaxAccessPoints> records{};
  if(esp_wifi_scan_get_ap_num(&total)!=ESP_OK||esp_wifi_scan_get_ap_records(&n,records.data())!=ESP_OK)return false;
  if(n>survey::MaxAccessPoints)return false;
  count=n;truncated=total>n;
  for(size_t i=0;i<n;++i){out[i]={};size_t written=0;for(size_t c=0;c<32&&records[i].ssid[c];++c){auto byte=records[i].ssid[c];if(byte>=32&&byte<127&&byte!='\\')out[i].ssid[written++]=char(byte);else{constexpr char hex[]="0123456789ABCDEF";out[i].ssid[written++]='\\';out[i].ssid[written++]='x';out[i].ssid[written++]=hex[byte>>4];out[i].ssid[written++]=hex[byte&15];}}std::memcpy(out[i].bssid.data(),records[i].bssid,6);out[i].channel=records[i].primary;out[i].security=uint8_t(records[i].authmode);out[i].rssi=records[i].rssi;}
  return true;
 }
 bool Clear(){return esp_wifi_clear_ap_list()==ESP_OK;}
};
SurveyDriver survey_driver;survey::Controller<SurveyDriver> wifi_survey{survey_driver};


}
bool StageAuthorizedGrant(AuthorizedGrant grant) {
    if(CompanionClosingPending()){std::fill(grant.credential.begin(),grant.credential.end(),'\0');return false;}
    if(!RememberedRuntimeStopped()){std::fill(grant.credential.begin(),grant.credential.end(),'\0');return false;}
    if((grant.expires_at_ms&&static_cast<uint64_t>(esp_timer_get_time()/1000)>=grant.expires_at_ms) || !grant.host_audio_qualified || grant.credential.empty() || grant.credential.size()>256 || grant.uri.rfind("wss://",0)!=0 || grant.device.empty() || grant.request.empty()){std::fill(grant.credential.begin(),grant.credential.end(),'\0');return false;}
    std::lock_guard<std::mutex> lock(grant_mutex);if(pending_grant){std::fill(grant.credential.begin(),grant.credential.end(),'\0');return false;}
    pending_grant=std::move(grant);return true;
}
namespace {
// Persistent boot-lifetime owner: never destroy callbacks while workers unwind.
struct Runtime {
    static uint64_t LocalIntentClock(void*)noexcept{return static_cast<uint64_t>(esp_timer_get_time()/1000);}
    wake_runtime::ConsentAuthority local_consent{LocalIntentClock};
    wake_runtime::ExistingPhysicalDispatcher local_intent{local_consent};
    wake_runtime::ExplicitLocalConsent borrowed_physical_proof;
    wake_runtime::FactoryHandle borrowed_factory;wake_runtime::DetachedCpuEndpointReceipt borrowed_detached;
    wake_runtime::LocalWakeController local_wake{local_consent,LocalIntentClock,nullptr,[]()noexcept{return Board::GetInstance().GetAudioCodec();}};
    std::atomic<uint64_t> local_view_generation{0};lv_obj_t* local_wake_button=nullptr;
    lv_obj_t* camera_allow_button=nullptr;lv_obj_t* camera_decline_button=nullptr;
    // LocalWake has a separately named queued intent; ordinary Listen never dispatches it.
    bool QueueExplicitLocalWakeOptIn(){return local_intent.CaptureLocalWakeOptIn(LocalIntentClock(nullptr));}
    bool AcceptExplicitLocalWakeOptIn(wake_runtime::ExplicitLocalConsent& proof){return local_intent.AcceptLocalWakeOptIn(audio_lease,socket.Epoch(),proof);}
    UsbProvisioner provisioner;
    NumericPairing numeric_pairing;
    FeatureLink* feature_authority=nullptr;
    struct FeatureListen {FeatureLink* owner;std::string request;uint32_t epoch;};
    std::optional<FeatureListen> feature_listen;
    NumericUsbRequest numeric_request;
    uint64_t numeric_until=0;
    bool numeric_connected=false,wireless_pairing=false,remember_enrollment=false,forget_pending=false;
    uint64_t saved_deadline=0;uint32_t saved_expiry=0;
    desk::LeaseReactiveAvatar reactive;
    desk::CompanionReactiveView reactive_view;
    CompanionDetailsView details_view;
    desk::ClosingViewPolicy closing_view;
    lv_obj_t* closing_fallback=nullptr;
    CodecTasks tasks;
    CompanionSocket socket{tasks};
    std::atomic<bool> wifi_started{false};bool grant_connected=false,owns_audio=false;AudioLease audio_lease;uint64_t audio_connection_epoch=0;
    ConnectGate connect_gate;
    PairConnectionConsent matching_consent;
    PairUiAction pair_ui;
    lv_obj_t* status=nullptr;
    lv_obj_t *status_panel=nullptr,*connection_badge=nullptr,*presentation_toggle=nullptr,*details_toggle=nullptr,*settings_button=nullptr;
    int view_width=320,view_height=240;bool view_supported=false,character_surface=false,presentation_prompt=false,remembered_view_pending=false;
    lv_obj_t* diagnostic_button=nullptr;
    lv_obj_t* public_status_button=nullptr;
    lv_obj_t* public_status_root=nullptr;lv_obj_t* public_status_text=nullptr;
    bool public_status_tls=false;
    enrollment_observation::Snapshot public_snapshot;
    enrollment_observation::Current public_current;
    lv_obj_t* pair_button=nullptr;
    lv_obj_t* remember_button=nullptr;lv_obj_t* connect_button=nullptr;lv_obj_t* listen_button=nullptr;
    Runtime(){numeric_pairing.ConfigureRemembered(CurrentPairPolicy,SaveRememberedPair);}
    const char* message=InitialViewStatus(STACKCHAN_COMPANION_REMEMBERED_RUNTIME!=0,RememberedStatusText());
};
Runtime& runtime() { static auto* value=new Runtime;return *value; }
bool CloseRuntimeBorrower(Runtime& state,bool joined){
    if(!state.borrowed_factory.Endpoint())return true;
    if(!joined)return false;
    if(!state.borrowed_detached)state.borrowed_detached=state.tasks.DetachBorrowedCpuAfterStop(state.borrowed_factory.Endpoint());
    if(!state.borrowed_detached||!state.borrowed_factory.DestroyDetached(state.borrowed_detached))return false;
    state.borrowed_detached={};return true;
}
bool StopReactiveTasks(Runtime& state){
    state.reactive.RequestStopFor(state.audio_lease);RevokeAudio(state.audio_lease);
    const bool stopped=state.tasks.Stop();state.reactive.ClosedFor(state.audio_lease,stopped);return stopped&&CloseRuntimeBorrower(state,stopped);
}
bool PublicStatusIdle(){
    auto& state=runtime();bool pending=false;{std::lock_guard<std::mutex> lock(grant_mutex);pending=pending_grant.has_value();}
    const bool action_pending=remember_requested.load()||pair_requested.load()||connect_requested.load()||listen_requested.load()||listen_epoch_requested.load()||forget_requested.load()||confirm_requested.load()||reject_requested.load();
    return RememberedRuntimeStopped()&&!state.grant_connected&&!state.numeric_until&&!pending&&!state.feature_authority&&state.provisioner.Stopped()&&state.tasks.IsStopped()&&CurrentAudioOwner()==AudioOwner::None&&!action_pending&&PublicStatusOwner().Idle();
}
enrollment_observation::Current CurrentPublicStatus(){
    enrollment_observation::Current value{enrollment_observation::Clock(std::time(nullptr)),false,0};
    wifi_ap_record_t ap{};esp_netif_ip_info_t ip{};
    auto* netif=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if(esp_wifi_sta_get_ap_info(&ap)==ESP_OK&&netif&&esp_netif_get_ip_info(netif,&ip)==ESP_OK&&ip.ip.addr){value.associated=true;value.address=ntohl(ip.ip.addr);}
    return value; // Never copy SSID/BSSID, keys, or other AP fields.
}
void RenderPublicStatus(){
    auto& s=runtime();char text[384];
    if(s.public_status_tls)enrollment_observation::TlsDetails(text,sizeof(text),s.public_snapshot);
    else enrollment_observation::Overview(text,sizeof(text),s.public_snapshot,s.public_current);
    lv_label_set_text(s.public_status_text,text);
}
void ClosePublicStatus(){
    auto& s=runtime();LvglLockGuard lock;
    if(s.public_status_root)lv_obj_delete(s.public_status_root);
    s.public_status_root=s.public_status_text=nullptr;public_status_close=false;public_status_next=false;
}
void OpenPublicStatus(){
    auto& s=runtime();s.public_snapshot=enrollment_observation::State().Get();s.public_current=CurrentPublicStatus();s.public_status_tls=false;
    LvglLockGuard lock;s.public_status_root=lv_obj_create(lv_layer_top());
    lv_obj_set_pos(s.public_status_root,0,0);lv_obj_set_size(s.public_status_root,320,240);lv_obj_set_style_pad_all(s.public_status_root,0,0);
    lv_obj_remove_flag(s.public_status_root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_bg_color(s.public_status_root,lv_color_hex(0x10232d),0);
    lv_obj_set_style_text_color(s.public_status_root,lv_color_hex(0xffffff),0);lv_obj_set_style_text_font(s.public_status_root,&lv_font_montserrat_14,0);
    auto title=lv_label_create(s.public_status_root);lv_obj_set_pos(title,8,8);lv_label_set_text(title,"Enrollment status - read only");
    s.public_status_text=lv_label_create(s.public_status_root);lv_obj_set_pos(s.public_status_text,8,38);lv_obj_set_size(s.public_status_text,304,154);
    auto next=lv_button_create(s.public_status_root);lv_obj_set_pos(next,8,198);lv_obj_set_size(next,148,36);
    auto next_label=lv_label_create(next);lv_label_set_text(next_label,"Clock / TLS");lv_obj_center(next_label);
    lv_obj_add_event_cb(next,[](lv_event_t*){public_status_next=true;},LV_EVENT_CLICKED,nullptr);
    auto close=lv_button_create(s.public_status_root);lv_obj_set_pos(close,164,198);lv_obj_set_size(close,148,36);
    auto close_label=lv_label_create(close);lv_label_set_text(close_label,"Close status");lv_obj_center(close_label);
    lv_obj_add_event_cb(close,[](lv_event_t*){public_status_close=true;},LV_EVENT_CLICKED,nullptr);RenderPublicStatus();
}
void SetViewStatus(lv_obj_t* label,const char* text){
    if(std::strcmp(lv_label_get_text(label),text)==0)return;
    lv_label_set_text(label,text);
    lv_obj_scroll_to_y(lv_obj_get_parent(label),0,LV_ANIM_OFF);
}
void UpdateViewPresentation(const char* display,bool comparing,bool camera_prompt,bool closing=false){
    auto& s=runtime();if(!s.view_supported||!s.status_panel||!s.presentation_toggle)return;
    const bool prompt=comparing||camera_prompt;
    const bool controls=!s.reactive.Selected()||prompt||closing;
    auto visible=[](lv_obj_t* object,bool show){if(!object)return;if(show)lv_obj_remove_flag(object,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(object,LV_OBJ_FLAG_HIDDEN);};
    s.character_surface=!controls;s.presentation_prompt=prompt;
    visible(s.status_panel,controls);visible(s.connection_badge,!controls);visible(s.presentation_toggle,controls&&!closing);visible(s.details_toggle,controls&&!closing);visible(s.settings_button,!controls);
    visible(s.pair_button,controls&&!camera_prompt&&!closing);
    visible(s.connect_button,controls&&!prompt&&!closing);visible(s.listen_button,controls&&!prompt&&!closing);
    visible(s.camera_allow_button,camera_prompt&&!closing);visible(s.camera_decline_button,camera_prompt&&!closing);
    if(prompt||closing)lv_obj_add_state(s.presentation_toggle,LV_STATE_DISABLED);else lv_obj_remove_state(s.presentation_toggle,LV_STATE_DISABLED);
    lv_label_set_text(lv_obj_get_child(s.presentation_toggle,0),"Mira");
    const auto compact=desk::CharacterConnectionStatus(display);lv_label_set_text(s.connection_badge,compact.c_str());
    if(closing)s.details_view.Hide();
    const auto frame=s.reactive.SnapshotCurrent(uint32_t(esp_timer_get_time()/1000));
    if(frame.status.stop&&*frame.status.stop)visible(s.connection_badge,false);
    s.reactive_view.Render(frame,controls);
}
void HeaderRect(lv_obj_t* object,header_layout::Rect rect){lv_obj_set_pos(object,rect.x,rect.y);lv_obj_set_size(object,rect.width,rect.height);}
}
bool CompanionCharacterVisible(){return runtime().view_supported&&runtime().character_surface;}
void ShowMiraCharacter(){auto& s=runtime();if(!s.view_supported||s.presentation_prompt||s.closing_view.Pending())return;LvglLockGuard lock;s.reactive_view.SelectRoom(false);s.reactive.Select(true);UpdateViewPresentation(s.message,false,false);}
bool StageFeatureAudioGrant(FeatureLink& link,uint64_t now){
 if(CompanionClosingPending())return false;
 auto& state=runtime();
 if(!RememberedRuntimeStopped()||state.feature_authority||state.grant_connected||state.numeric_until||state.provisioner.Active()||!state.tasks.IsStopped()||CurrentAudioOwner()!=AudioOwner::None)return false;
 auto grant=link.TakeAudioGrant(now);if(!grant||grant->source_!=&link||!link.MatchesAudioGrant(grant->request_,grant->deadline_,now))return false;
 AuthorizedGrant staged;staged.uri=STACKCHAN_PIN_URI;staged.device=STACKCHAN_PIN_DEVICE;staged.request=grant->request_;staged.credential=std::move(grant->credential_);staged.host_audio_qualified=true;staged.expires_at_ms=grant->deadline_;staged.feature_authority=&link;
 if(!StageAuthorizedGrant(std::move(staged)))return false;
 state.feature_authority=&link;state.matching_consent.Cancel(); // Pair does not auto Listen.
 state.message="Mira companion\nMuted - union grant staged; Connect then Listen";return true;
}
bool ConnectFeatureAudio(FeatureLink& link,const std::string& request){
 if(CompanionClosingPending())return false;
 auto& state=runtime();auto now=static_cast<uint64_t>(esp_timer_get_time()/1000);
 if(state.feature_authority!=&link||request!=link.PairRequest()||!link.AudioAuthorityCurrent(now)||state.grant_connected)return false;
 {std::lock_guard<std::mutex> lock(grant_mutex);if(!pending_grant||pending_grant->feature_authority!=&link||!link.MatchesAudioGrant(pending_grant->request,pending_grant->expires_at_ms,now))return false;}
 connect_requested=true;return true;
}
bool ListenFeatureAudio(FeatureLink& link,const std::string& request,uint32_t epoch){
 if(CompanionClosingPending())return false;
 auto& state=runtime();auto now=static_cast<uint64_t>(esp_timer_get_time()/1000);
 if(state.feature_authority!=&link||request!=link.PairRequest()||!link.AudioAuthorityCurrent(now)||!state.grant_connected||!epoch||epoch!=state.socket.Epoch()||state.socket.GetState()!=CompanionSocket::State::ConsentRequired)return false;
 state.feature_listen=Runtime::FeatureListen{&link,request,epoch};return true;
}
uint32_t FeatureAudioEpoch(const FeatureLink& link){if(CompanionClosingPending())return 0;auto& state=runtime();return state.feature_authority==&link?state.socket.Epoch():0;}
bool FeatureAudioStaged(const FeatureLink& link){return !CompanionClosingPending()&&runtime().feature_authority==&link;}
bool FeatureAudioIdle(){if(CompanionClosingPending())return false;auto& s=runtime();std::lock_guard<std::mutex> lock(grant_mutex);return RememberedRuntimeStopped()&&!s.feature_authority&&!s.grant_connected&&!s.numeric_until&&!pending_grant&&!s.provisioner.Active()&&s.tasks.IsStopped()&&CurrentAudioOwner()==AudioOwner::None;}
bool RequestWifiSurvey(){
 if(!RememberedRuntimeStopped()||CurrentAudioOwner()==AudioOwner::Mira)return false;
 std::lock_guard<std::mutex> lock(wifi_start_mutex);
 wifi_survey.Poll(esp_timer_get_time()/1000);
 const bool ok=wifi_survey.Start(esp_timer_get_time()/1000);wifi_survey_busy=wifi_survey.OwnsScan();return ok;
}
void PollWifiSurvey(){std::lock_guard<std::mutex> lock(wifi_start_mutex);wifi_survey.Poll(esp_timer_get_time()/1000);wifi_survey_busy=wifi_survey.OwnsScan();}
bool CancelWifiSurvey(){std::lock_guard<std::mutex> lock(wifi_start_mutex);const bool ok=wifi_survey.Cancel();wifi_survey_busy=wifi_survey.OwnsScan();return ok;}
survey::Snapshot WifiSurveySnapshot(){std::lock_guard<std::mutex> lock(wifi_start_mutex);return wifi_survey.Get();}
survey::Phase WifiSurveyPhase(){std::lock_guard<std::mutex> lock(wifi_start_mutex);return wifi_survey.Get().phase;}
void CopyWifiSurveySnapshot(survey::Snapshot& output){std::lock_guard<std::mutex> lock(wifi_start_mutex);output=wifi_survey.Get();}
bool EnsureSavedNetwork() {
    PollWifiSurvey();
    auto& state=runtime();
    state.wifi_started=StartSavedWifi();
    return state.wifi_started;
}
bool PollSavedNetworkStopped(){
    auto& authority=network::Authority();
    const auto owner=authority.Current();
    if(owner==network::Owner::None)return true;
    if(owner!=network::Owner::Saved)return false;
    authority.RequestStop(network::Owner::Saved);saved_wifi_owner.StopRequested();
    if(saved_stop_job.load()==1||!FeatureAudioIdle()||authority.Borrowers())return false;
    std::unique_lock<std::mutex> lock(wifi_start_mutex,std::try_to_lock);if(!lock)return false;
    wifi_survey.Poll(esp_timer_get_time()/1000);
    if(wifi_survey.OwnsScan()){wifi_survey.Cancel();wifi_survey.Poll(esp_timer_get_time()/1000);wifi_survey_busy=wifi_survey.OwnsScan();if(wifi_survey_busy)return false;}
    saved_stop_job=1;
    if(xTaskCreate([](void*){
        {std::lock_guard<std::mutex> hold(wifi_start_mutex);
         SavedStationStopDriver driver;
         if(network::StopStationChecked(driver)){
            if(esp_sntp_enabled())esp_sntp_stop();
            connected=false;runtime().wifi_started=false;saved_wifi_owner.Stopped(true);
            network::Authority().FinishStop(network::Owner::Saved,true);
         }}
        saved_stop_job=0;vTaskDelete(nullptr);
    },"saved_wifi_stop",4096,nullptr,3,nullptr)!=pdPASS)saved_stop_job=0;
    return false;
}
bool IsWifiConnected(){return connected.load()&&network::Authority().CanAcquire(network::Owner::Saved);}
bool NetworkWasStarted(){return saved_wifi_owner.WasStarted();}
SavedWifiPhase SavedNetworkPhase(){return saved_wifi_owner.Phase();}
uint16_t SavedNetworkDisconnectReason(){return saved_wifi_owner.DisconnectReason();}
bool RememberedStartAllowed(){
 if(CompanionClosingPending()||!network::Authority().CanAcquire(network::Owner::Saved))return false;
 auto& s=runtime();std::lock_guard<std::mutex> lock(grant_mutex);
 return !s.feature_authority&&!s.grant_connected&&!s.numeric_until&&!s.forget_pending&&!s.public_status_root&&!wifi_survey_busy.load()&&!pending_grant&&s.provisioner.Stopped()&&s.tasks.IsStopped()&&CurrentAudioOwner()==AudioOwner::None;
}
void CancelRememberedViewConnection(){runtime().remembered_view_pending=false;}
// Run only from the first ordinary UI tick after the completed view is published.
// A close cancels this pending user-open intent before it can start any worker.
void BeginRememberedViewConnection(){
    auto& state=runtime();if(!state.remembered_view_pending)return;
    if(!state.view_supported||state.closing_view.Pending()){state.remembered_view_pending=false;return;}
    if(!RememberedRuntimeStopped()){state.remembered_view_pending=false;return;}
    if(!RememberedStartAllowed())return;
    state.remembered_view_pending=false;BootRememberedRuntime();
    if(STACKCHAN_COMPANION_REMEMBERED_RUNTIME&&!RememberedRuntimePresent())state.message="Mira companion\nSaved connection unavailable - see Settings\nCapture unverified; camera/motion inactive";
}
void OpenView(lv_obj_t* parent) {
    if(runtime().closing_view.Pending()){RenderClosingCompanionView();return;}
    {LvglLockGuard lock;auto& state=runtime();state.view_supported=false;state.view_width=lv_obj_get_width(parent);state.view_height=lv_obj_get_height(parent);
     if(!desk::CharacterScreen(state.view_width,state.view_height).supported){
         state.reactive_view.Detach();state.details_view.Detach();state.character_surface=false;state.remembered_view_pending=false;state.status=state.status_panel=state.connection_badge=state.presentation_toggle=state.details_toggle=state.settings_button=nullptr;
         auto label=lv_label_create(parent);lv_obj_set_pos(label,0,0);lv_obj_set_size(label,state.view_width,state.view_height);
         lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);lv_label_set_text(label,"Mira needs a 320x240 or larger display");return;
     }state.view_supported=true;}
    room::SelectProvider(room::Provider::Mira);
    runtime().remembered_view_pending=true;
    diagnostic_requested=false;
    public_status_requested=false;ClosePublicStatus();
    auto& state=runtime();state.pair_ui.Reset();state.reactive.Select(true);auto& status=state.status;
    if(state.local_view_generation!=UINT64_MAX)++state.local_view_generation;
    pair_requested=false;connect_requested=false;listen_requested=false;listen_epoch_requested=0;remembered_listen_requested=0;remember_requested=false;forget_requested=false;mute_requested=false;confirm_requested=false;reject_requested=false;
    {LvglLockGuard lock;auto screen=parent;
     lv_obj_set_style_bg_color(screen,lv_color_hex(0x000000),0);
     // A bounded scrollable status area preserves long errors and consent copy.
     auto status_panel=lv_obj_create(screen);state.status_panel=status_panel;lv_obj_set_pos(status_panel,8,48);lv_obj_set_size(status_panel,304,78);
     lv_obj_set_style_pad_all(status_panel,0,0);lv_obj_set_style_border_width(status_panel,0,0);lv_obj_set_style_bg_opa(status_panel,LV_OPA_TRANSP,0);
     status=lv_label_create(status_panel);lv_obj_set_width(status,304);lv_obj_set_height(status,LV_SIZE_CONTENT);lv_label_set_long_mode(status,LV_LABEL_LONG_WRAP);lv_obj_set_pos(status,0,0);
     lv_obj_set_style_text_font(status,&lv_font_montserrat_14,0);
     lv_obj_set_style_text_color(status,lv_color_hex(0xffffff),0);
     lv_label_set_text(status,InitialViewStatus(STACKCHAN_COMPANION_REMEMBERED_RUNTIME!=0,RememberedStatusText()));
     state.reactive_view.Open(screen);
     auto face_select=lv_button_create(screen);state.presentation_toggle=face_select;lv_obj_set_pos(face_select,116,2);lv_obj_set_size(face_select,68,40);
     lv_obj_set_style_pad_all(face_select,0,0);auto face_label=lv_label_create(face_select);lv_obj_set_style_text_font(face_label,&lv_font_montserrat_14,0);lv_label_set_text(face_label,"Controls");lv_obj_center(face_label);
     lv_obj_add_event_cb(face_select,[](lv_event_t*){auto& r=runtime();r.reactive.Select(!r.reactive.Selected());UpdateViewPresentation(r.message,false,false);},LV_EVENT_CLICKED,nullptr);
     auto connect=lv_button_create(screen);state.connect_button=connect;lv_obj_set_size(connect,148,40);lv_obj_set_pos(connect,8,174);
     auto connect_label=lv_label_create(connect);lv_label_set_text(connect_label,"Connect");lv_obj_center(connect_label);
     lv_obj_add_event_cb(connect,[](lv_event_t*){connect_requested=true;},LV_EVENT_CLICKED,nullptr);
     auto listen=lv_button_create(screen);state.listen_button=listen;lv_obj_set_size(listen,148,40);lv_obj_set_pos(listen,164,174);
     auto listen_label=lv_label_create(listen);lv_label_set_text(listen_label,"Listen");lv_obj_center(listen_label);
     lv_obj_add_event_cb(listen,[](lv_event_t* event){auto& r=runtime();const auto view=static_cast<uint64_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));if(!view||view!=r.local_view_generation||lv_event_get_target(event)!=r.listen_button)return;auto captured=CaptureRememberedListen();if(RouteRememberedListen(captured,r.grant_connected)){CaptureRememberedLocalProcessingGesture(r.listen_button,view,captured);remembered_listen_requested=captured;}else if(r.socket.GetState()==CompanionSocket::State::ConsentRequired){r.local_intent.CapturePhysicalListen(Runtime::LocalIntentClock(nullptr));listen_epoch_requested=r.socket.Epoch();}},LV_EVENT_CLICKED,reinterpret_cast<void*>(static_cast<uintptr_t>(state.local_view_generation)));
     RegisterRememberedPhysicalControl(listen,state.local_view_generation);
     auto camera_allow=lv_button_create(screen);state.camera_allow_button=camera_allow;lv_obj_set_size(camera_allow,148,40);lv_obj_set_pos(camera_allow,8,174);
     auto camera_allow_label=lv_label_create(camera_allow);lv_label_set_text(camera_allow_label,"Allow camera");lv_obj_center(camera_allow_label);
     auto camera_decline=lv_button_create(screen);state.camera_decline_button=camera_decline;lv_obj_set_size(camera_decline,148,40);lv_obj_set_pos(camera_decline,164,174);
     auto camera_decline_label=lv_label_create(camera_decline);lv_label_set_text(camera_decline_label,"Decline camera");lv_obj_center(camera_decline_label);
     lv_obj_add_flag(camera_allow,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(camera_decline,LV_OBJ_FLAG_HIDDEN);
     state.socket.PublishCameraPhysicalControls(camera_allow,camera_decline,state.local_view_generation);
     RegisterRememberedCameraControls(camera_allow,camera_decline,state.local_view_generation);
     auto camera_click=[](lv_event_t* event){auto& r=runtime();const auto view=static_cast<uint64_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
         if(!view||view!=r.local_view_generation||r.public_status_root||(lv_event_get_target(event)!=r.camera_allow_button&&lv_event_get_target(event)!=r.camera_decline_button))return;
         if(RememberedTransportActive())CaptureRememberedCameraChoice(lv_event_get_target(event),view);
         else r.socket.CaptureCameraLocalChoice(lv_event_get_target(event),view);};
     lv_obj_add_event_cb(camera_allow,camera_click,LV_EVENT_CLICKED,reinterpret_cast<void*>(static_cast<uintptr_t>(state.local_view_generation)));
     lv_obj_add_event_cb(camera_decline,camera_click,LV_EVENT_CLICKED,reinterpret_cast<void*>(static_cast<uintptr_t>(state.local_view_generation)));
     auto pair=lv_button_create(screen);state.pair_button=pair;lv_obj_set_size(pair,304,40);lv_obj_set_pos(pair,8,130);
     auto pair_label=lv_label_create(pair);lv_label_set_text(pair_label,"Pair");lv_obj_center(pair_label);
     lv_obj_add_event_cb(pair,[](lv_event_t*){runtime().pair_ui.Click();},LV_EVENT_CLICKED,nullptr);

     auto details_toggle=lv_button_create(screen);state.details_toggle=details_toggle;lv_obj_set_pos(details_toggle,8,2);lv_obj_set_size(details_toggle,100,40);
     auto details_label=lv_label_create(details_toggle);lv_label_set_text(details_label,"Tools");lv_obj_center(details_label);
     lv_obj_add_event_cb(details_toggle,[](lv_event_t*){runtime().details_view.Toggle();},LV_EVENT_CLICKED,nullptr);
     auto badge=lv_label_create(screen);state.connection_badge=badge;lv_obj_set_pos(badge,8,state.view_height-24);lv_obj_set_size(badge,state.view_width-152,20);
     lv_obj_set_style_bg_color(badge,lv_color_hex(0x000000),0);lv_obj_set_style_bg_opa(badge,200,0);lv_obj_set_style_text_color(badge,lv_color_hex(0xffffff),0);
     lv_label_set_long_mode(badge,LV_LABEL_LONG_DOT);lv_obj_set_style_text_font(badge,&lv_font_montserrat_14,0);
     auto settings=lv_button_create(screen);state.settings_button=settings;lv_obj_set_pos(settings,state.view_width-88,8);lv_obj_set_size(settings,80,36);
     lv_obj_set_style_bg_color(settings,lv_color_hex(0x000000),0);lv_obj_set_style_bg_opa(settings,180,0);lv_obj_set_style_pad_all(settings,0,0);
     auto settings_label=lv_label_create(settings);lv_label_set_text(settings_label,"Settings");lv_obj_center(settings_label);
     lv_obj_add_event_cb(settings,[](lv_event_t*){auto& r=runtime();r.reactive.Select(false);UpdateViewPresentation(r.message,false,false);},LV_EVENT_CLICKED,nullptr);
     auto details=state.details_view.Open(screen);
     auto diagnostics=lv_button_create(details);state.diagnostic_button=diagnostics;HeaderRect(diagnostics,header_layout::Export);
     auto diagnostic_label=lv_label_create(diagnostics);lv_label_set_text(diagnostic_label,"Export");lv_obj_center(diagnostic_label);
     lv_obj_add_event_cb(diagnostics,[](lv_event_t*){runtime().details_view.Hide();diagnostic_requested=true;},LV_EVENT_CLICKED,nullptr);
     auto public_status=lv_button_create(details);state.public_status_button=public_status;HeaderRect(public_status,header_layout::Status);
     auto public_label=lv_label_create(public_status);lv_label_set_text(public_label,"Status");lv_obj_center(public_label);
     lv_obj_add_event_cb(public_status,[](lv_event_t*){runtime().details_view.Hide();public_status_requested=true;},LV_EVENT_CLICKED,nullptr);
     auto remember=lv_button_create(details);state.remember_button=remember;HeaderRect(remember,header_layout::Remember);
     auto remember_label=lv_label_create(remember);lv_label_set_text(remember_label,"Pair and Remember");lv_obj_center(remember_label);
     lv_obj_add_event_cb(remember,[](lv_event_t*){runtime().details_view.Hide();remember_requested=true;},LV_EVENT_CLICKED,nullptr);
     auto forget=lv_button_create(details);HeaderRect(forget,header_layout::Forget);
     auto forget_label=lv_label_create(forget);lv_label_set_text(forget_label,"Forget");lv_obj_center(forget_label);
     lv_obj_add_event_cb(forget,[](lv_event_t*){runtime().details_view.Hide();forget_requested=true;},LV_EVENT_CLICKED,nullptr);
     if(wake_runtime::kLocalWakeOptInHookEnabled){
         auto local_wake=lv_button_create(details);state.local_wake_button=local_wake;HeaderRect(local_wake,header_layout::LocalWake);
         auto label=lv_label_create(local_wake);lv_label_set_text(label,"Local\nwake");lv_obj_center(label);
         state.local_wake.PublishControl(local_wake,state.local_view_generation);
         lv_obj_add_event_cb(local_wake,[](lv_event_t* event){runtime().details_view.Hide();runtime().local_wake.QueueExplicitLocalWakeOptIn(lv_event_get_target(event),static_cast<uint64_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event))));},LV_EVENT_CLICKED,reinterpret_cast<void*>(static_cast<uintptr_t>(state.local_view_generation)));
     }
     auto room_button=lv_button_create(details);HeaderRect(room_button,header_layout::Room);
     auto room_label=lv_label_create(room_button);lv_label_set_text(room_label,"Room");lv_obj_center(room_label);
     lv_obj_add_event_cb(room_button,[](lv_event_t*){auto& r=runtime();r.details_view.Hide();r.reactive.Select(true);r.reactive_view.SelectRoom(true);UpdateViewPresentation(r.message,false,false);},LV_EVENT_CLICKED,nullptr);
     state.reactive_view.SetDecisionSink([](const std::string& choice,const std::string& ticket){return room::QueueDecision(room::Provider::Mira,choice,ticket,uint64_t(esp_timer_get_time()/1000));});
     auto mute=lv_button_create(details);HeaderRect(mute,header_layout::Mute);
     auto mute_label=lv_label_create(mute);lv_label_set_text(mute_label,"Mute");lv_obj_center(mute_label);
     lv_obj_add_event_cb(mute,[](lv_event_t*){runtime().details_view.Hide();runtime().reactive.RequestStop();InterruptRememberedRuntimeImmediately();physical_mute_requested=true;mute_requested=true;},LV_EVENT_CLICKED,nullptr);
     lv_obj_add_event_cb(mute,[](lv_event_t*){runtime().details_view.Hide();runtime().reactive.RequestStop();InterruptRememberedRuntimeImmediately();physical_mute_requested=true;mute_requested=true;},LV_EVENT_LONG_PRESSED,nullptr);
    }
    state.message=InitialViewStatus(STACKCHAN_COMPANION_REMEMBERED_RUNTIME!=0,RememberedStatusText());
    if(STACKCHAN_COMPANION_REMEMBERED_RUNTIME&&!RememberedStatusText())state.message="Mira companion\nOpening saved connection\nCapture unverified; camera/motion inactive";
    {LvglLockGuard lock;SetViewStatus(state.status,state.message);UpdateViewPresentation(state.message,false,false);}
}
static bool CloseViewOwned() {room::DisconnectProvider(room::Provider::Mira);CancelRememberedViewConnection();MuteRememberedRuntime();TickRememberedRuntime();led_expression::PublishStatus(led_expression::Status::None);
    diagnostic_requested=false;
    public_status_requested=false;ClosePublicStatus();
    auto& state=runtime();state.local_wake.PublishControl(nullptr,0);RegisterRememberedPhysicalControl(nullptr,0);
    state.socket.PublishCameraPhysicalControls(nullptr,nullptr,0);RegisterRememberedCameraControls(nullptr,nullptr,0);const bool local_closed=state.local_wake.Stop();state.feature_authority=nullptr;state.feature_listen.reset();state.pair_ui.Reset();bool usb_closed=state.provisioner.Close();
    diagnostic::Stage(diagnostic::Task::Main,diagnostic::StageCode::NumericCloseEnter);state.numeric_pairing.Cancel();diagnostic::Stage(diagnostic::Task::Main,diagnostic::StageCode::NumericCloseExit);state.numeric_until=0;state.numeric_request={};state.numeric_connected=false;state.wireless_pairing=false;state.remember_enrollment=false;state.saved_deadline=0;state.saved_expiry=0;state.connect_gate.Cancel();state.matching_consent.Cancel();
    pair_requested=false;connect_requested=false;listen_requested=false;listen_epoch_requested=0;remembered_listen_requested=0;remember_requested=false;forget_requested=false;mute_requested=false;confirm_requested=false;reject_requested=false;
    {std::lock_guard<std::mutex> lock(grant_mutex);if(pending_grant)std::fill(pending_grant->credential.begin(),pending_grant->credential.end(),'\0');pending_grant.reset();}
    diagnostic::Stage(diagnostic::Task::Main,diagnostic::StageCode::AudioCloseEnter);state.reactive.RequestStopFor(state.audio_lease);state.socket.Close();diagnostic::Stage(diagnostic::Task::Main,diagnostic::StageCode::AudioCloseExit);state.grant_connected=false;
    diagnostic::Stage(diagnostic::Task::Main,diagnostic::StageCode::TasksStopEnter);bool stopped=StopReactiveTasks(state);diagnostic::Stage(diagnostic::Task::Main,diagnostic::StageCode::TasksStopExit);diagnostic::Sample(diagnostic::Task::Main);
    if(stopped&&state.owns_audio){state.owns_audio=!ReleaseAudio(state.audio_lease,true);}
    if(stopped&&RememberedRuntimeStopped()&&!state.closing_view.Pending())state.status=nullptr;
    return stopped && local_closed && usb_closed && RememberedRuntimeStopped();
}
bool CloseView(){
    // A re-opened/forced-close app may render while the retained cleanup worker
    // runs, but cannot concurrently mutate the boot-lifetime closure owner.
    if(runtime().closing_view.Pending()&&!runtime().closing_view.OwnerMustRetry())return false;
    return CloseViewOwned();
}
void TickView() {
    if(runtime().closing_view.Pending()){
        if(runtime().closing_view.OwnerMustRetry())runtime().closing_view.ReportClosed(CloseView());
        RenderClosingCompanionView();
        if(!runtime().closing_view.Pending()){LvglLockGuard lock;auto& state=runtime();if(state.closing_fallback){lv_obj_delete(state.closing_fallback);state.closing_fallback=nullptr;}}
        return;
    }
    TickRememberedRuntime();
    if(!runtime().view_supported)return;
    BeginRememberedViewConnection();
    auto& state=runtime();if(!state.status)return;
    if(state.local_wake_button)state.local_wake.PublishControl(state.local_wake_button,state.local_view_generation);
    state.local_wake.StartPending();
    // Metadata publication only: provider/model availability and both intent hooks remain false.
    state.local_intent.Publish(state.audio_lease,state.socket.Epoch());
    if(state.public_status_root){
        if(public_status_close.exchange(false))ClosePublicStatus();
        else if(public_status_next.exchange(false)){LvglLockGuard lock;state.public_status_tls=!state.public_status_tls;RenderPublicStatus();}
        // Keep normal expiry/cleanup processing active behind this read-only view.
    }
    diagnostic::Stage(diagnostic::Task::Main,diagnostic::StageCode::TickEnter);diagnostic::CountTick();
    struct TickEnd{~TickEnd(){diagnostic::Stage(diagnostic::Task::Main,diagnostic::StageCode::TickExit);}} tick_end;
    auto& provisioner=state.provisioner;auto& tasks=state.tasks;auto& socket=state.socket;
    auto& wifi_started=state.wifi_started;auto& grant_connected=state.grant_connected;
    auto& connect_gate=state.connect_gate;auto& message=state.message;auto status=state.status;
    uint64_t now=esp_timer_get_time()/1000;
    {LvglLockGuard lock;state.reactive_view.SetProviderSnapshot(room::ViewProvider(room::Provider::Mira,now),now);}
    switch(state.pair_ui.Take(state.numeric_pairing.Code(),state.numeric_request.request,state.numeric_pairing.AudioRequested())) {
        case PairUiAction::Action::Pair:pair_requested=true;break;
        case PairUiAction::Action::Confirm:confirm_requested=true;break;
        case PairUiAction::Action::Stale:message="Pairing changed - compare code again";break;
        case PairUiAction::Action::None:break;
    }
    if(forget_requested.exchange(false)){state.local_wake.Stop(0);MuteRememberedRuntime();state.forget_pending=true;}
    if(state.forget_pending){
        // Retain the pending Forget on every cleanup or erase failure. No competing action can run.
        pair_requested=false;remember_requested=false;connect_requested=false;confirm_requested=false;listen_requested=false;listen_epoch_requested=0;remembered_listen_requested=0;
        state.feature_authority=nullptr;state.feature_listen.reset();state.matching_consent.Cancel();connect_gate.Cancel();state.pair_ui.Reset();
        state.numeric_pairing.Cancel();state.numeric_until=0;state.numeric_connected=false;state.numeric_request={};state.remember_enrollment=false;state.wireless_pairing=false;
        {std::lock_guard<std::mutex> lock(grant_mutex);if(pending_grant)std::fill(pending_grant->credential.begin(),pending_grant->credential.end(),'\0');pending_grant.reset();}
        state.reactive.RequestStopFor(state.audio_lease);socket.Close();grant_connected=false;state.saved_deadline=0;state.saved_expiry=0;
        bool local_closed=state.local_wake.Stop(0);bool usb_closed=provisioner.Close();bool stopped=StopReactiveTasks(state);if(stopped&&state.owns_audio){state.owns_audio=!ReleaseAudio(state.audio_lease,true);}
        if(local_closed&&usb_closed&&stopped&&RememberedRuntimeStopped()&&CurrentAudioOwner()==AudioOwner::None){
            auto forgotten=RememberedRuntimePresent()?RevokeRememberedRuntime():StoreResult::Missing;
            if((forgotten==StoreResult::Ok||forgotten==StoreResult::Missing)&&ForgetRememberedPair()){state.forget_pending=false;message="Mira companion\nForgot robot pairing - Forget on Mac too";}
            else message="Mira companion\nMuted - Forget storage failed; retrying";
        }else message="Mira companion\nMuted - Forget cleanup pending";
        {LvglLockGuard lock;lv_label_set_text(status,message);}
        return;
    }
    if(state.saved_deadline){auto policy=CurrentPairPolicy();if(now>=state.saved_deadline||!policy.now||policy.now>=state.saved_expiry||policy.expiry!=state.saved_expiry)mute_requested=true;}
    if(reject_requested.exchange(false))mute_requested=true;
    if(state.feature_authority&&!state.feature_authority->AudioAuthorityCurrent(now))mute_requested=true;
    if(state.numeric_until){
        if(state.remember_enrollment&&now>=state.numeric_until)enrollment_observation::State().Pair(enrollment_observation::PairStage::Expired);
        state.numeric_pairing.Tick(now);
        if(state.remember_enrollment&&state.numeric_pairing.RememberedReady()){
            AuthenticatedRememberedEnrollmentStored();
            state.numeric_pairing.Cancel();state.numeric_until=0;state.numeric_connected=false;state.numeric_request={};state.wireless_pairing=false;state.remember_enrollment=false;state.pair_ui.Reset();
            message="Mira companion\nPairing remembered - muted; tap Connect";
        }
        if(state.numeric_until&&(now>=state.numeric_until||(state.numeric_connected&&!state.numeric_pairing.Active(now))))mute_requested=true;
    }
    {std::lock_guard<std::mutex> lock(grant_mutex);if(pending_grant&&pending_grant->expires_at_ms&&now>=pending_grant->expires_at_ms){std::fill(pending_grant->credential.begin(),pending_grant->credential.end(),'\0');pending_grant.reset();mute_requested=true;}}
        if(mute_requested.exchange(false)) {
            state.local_wake.Stop(0);
            if(physical_mute_requested.exchange(false))MuteRememberedRuntime();
            bool remember_failed=state.remember_enrollment;
            state.feature_authority=nullptr;state.feature_listen.reset();
            provisioner.Close();diagnostic::Stage(diagnostic::Task::Main,diagnostic::StageCode::NumericCloseEnter);state.numeric_pairing.Cancel();diagnostic::Stage(diagnostic::Task::Main,diagnostic::StageCode::NumericCloseExit);state.numeric_request={};state.numeric_until=0;state.numeric_connected=false;state.wireless_pairing=false;state.matching_consent.Cancel();state.remember_enrollment=false;state.saved_deadline=0;state.saved_expiry=0;listen_epoch_requested=0;remembered_listen_requested=0;confirm_requested=false;connect_gate.Cancel();pair_requested=false;connect_requested=false;listen_requested=false;
            {std::lock_guard<std::mutex> lock(grant_mutex);if(pending_grant)std::fill(pending_grant->credential.begin(),pending_grant->credential.end(),'\0');pending_grant.reset();}
            diagnostic::Stage(diagnostic::Task::Main,diagnostic::StageCode::AudioCloseEnter);state.reactive.RequestStopFor(state.audio_lease);socket.Close();diagnostic::Stage(diagnostic::Task::Main,diagnostic::StageCode::AudioCloseExit);
            diagnostic::Stage(diagnostic::Task::Main,diagnostic::StageCode::TasksStopEnter);if(StopReactiveTasks(state)&&state.owns_audio){state.owns_audio=!ReleaseAudio(state.audio_lease,true);}diagnostic::Stage(diagnostic::Task::Main,diagnostic::StageCode::TasksStopExit);diagnostic::Sample(diagnostic::Task::Main);grant_connected=false;message=remember_failed?"Mira companion\nRemember ended - if stored, Forget before retry":"Mira companion\nMuted - connection closed";}
        if(diagnostic_requested.exchange(false)) {
            bool pending=false;{std::lock_guard<std::mutex> lock(grant_mutex);pending=pending_grant.has_value();}
            bool eligible=RememberedRuntimeStopped()&&!grant_connected&&!state.numeric_until&&!pending&&!provisioner.Active()&&tasks.IsStopped()&&CurrentAudioOwner()==AudioOwner::None;
            provisioner.ExportDiagnostics(eligible);message=provisioner.ExportMessage();
        }
        if(public_status_requested.exchange(false)){
            if(PublicStatusIdle()){OpenPublicStatus();return;}
            message="Mira companion\nStatus needs fully stopped pairing and audio";
        }
        bool remembered_action=remember_requested.exchange(false);
        if(remembered_action){enrollment_observation::State().Begin(enrollment_observation::Clock(std::time(nullptr)));pair_requested=true;}
        if(pair_requested.exchange(false)) {
            connect_gate.Cancel();
            bool pending=false; {std::lock_guard<std::mutex> lock(grant_mutex);pending=pending_grant.has_value();}
            if(remembered_action&&!RememberedPairStoreEmpty()){enrollment_observation::State().Pair(enrollment_observation::PairStage::StoredRecordPresent);message="Mira companion\nForget existing robot pairing first";}
            else if(!RememberedRuntimeStopped() || grant_connected || pending){if(remembered_action)enrollment_observation::State().Pair(enrollment_observation::PairStage::CleanupRequired);message="Mira companion\nMuted - use Back to stop before pairing";}
            else {
                state.numeric_pairing.Cancel();state.numeric_request={};state.numeric_until=0;state.numeric_connected=false;state.wireless_pairing=false;state.matching_consent.Cancel();
                state.remember_enrollment=remembered_action;
                if(remembered_action||STACKCHAN_COMPANION_WIRELESS_PAIRING){
                    bool ready=provisioner.Close()&&tasks.IsStopped()&&CurrentAudioOwner()==AudioOwner::None;
                    state.wireless_pairing=ready;
                    if(ready){
                        state.numeric_until=now+120000;
                        if(!wifi_started||!connected)wifi_started=StartSavedWifi();
                        if(remembered_action)enrollment_observation::State().Pair(wifi_started?enrollment_observation::PairStage::WifiWait:enrollment_observation::PairStage::WifiStartFailed);
                        if(!wifi_started)mute_requested=true;
                    }
                    message=ready?"Mira companion\nWi-Fi pairing - keep Mac open":"Mira companion\nMuted - cleanup required";
                    if(remembered_action&&!ready)enrollment_observation::State().Pair(enrollment_observation::PairStage::CleanupRequired);
                }else{
                    bool opened=provisioner.Open();
                    if(opened&&provisioner.Numeric())state.numeric_until=now+120000;
                    message=opened?"Mira companion\nPairing ready - keep Mac open":provisioner.OpenMessage();
                }
            }
        }
        if(provisioner.Active()&&provisioner.Numeric()) {
            NumericUsbRequest request;auto result=provisioner.PollNumeric(request);
            if(result==NumericUsbRead::Accepted){
                state.numeric_request=std::move(request);
                if(!wifi_started)wifi_started=StartSavedWifi();
                else if(!connected)wifi_started=StartSavedWifi();
                if(!wifi_started)mute_requested=true;
                message="Mira companion\nPairing - connecting saved Wi-Fi";
            }else if(result==NumericUsbRead::Rejected){mute_requested=true;message="Mira companion\nNumeric request expired or rejected";}
        }
        if(state.numeric_until&&(state.wireless_pairing||!state.numeric_request.request.empty())&&!state.numeric_connected&&wifi_started&&connected){
            auto network=Board::GetInstance().GetNetwork();
            uint64_t start=state.numeric_until-120000;
            if(state.wireless_pairing){
                state.numeric_connected=network&&(state.remember_enrollment?state.numeric_pairing.StartRemembered(*network,provisioner.BootNonce(),start):state.numeric_pairing.StartWireless(*network,provisioner.BootNonce(),start));
                if(state.numeric_connected){state.numeric_request.request=state.numeric_pairing.Request();state.numeric_request.boot_nonce=provisioner.BootNonce();}
            }else state.numeric_connected=network&&state.numeric_pairing.Start(*network,state.numeric_request.request,state.numeric_request.boot_nonce,start);
            if(!state.numeric_connected)mute_requested=true;
            else message="Mira companion\nWaiting for matching code";
        }
        if(confirm_requested.exchange(false)){if(!state.numeric_pairing.Confirm(now))mute_requested=true;else message="Mira companion\nRobot confirmed - confirm on Mac";}
        NumericGrant numeric_grant;
        if(state.numeric_pairing.TakeGrant(numeric_grant)){
            AuthorizedGrant grant;grant.uri=STACKCHAN_PIN_URI;grant.device=STACKCHAN_PIN_DEVICE;grant.request=numeric_grant.request;grant.credential=std::move(numeric_grant.credential);grant.host_audio_qualified=true;grant.expires_at_ms=numeric_grant.expires_at_ms;grant.numeric_pairing=true;
            bool staged=false;
            if(state.numeric_pairing.Active(now))staged=StageAuthorizedGrant(std::move(grant));
            else std::fill(grant.credential.begin(),grant.credential.end(),'\0');
            if(!staged||!state.numeric_pairing.Staged()){state.matching_consent.Cancel();mute_requested=true;}
            else if(state.matching_consent.Arm(numeric_grant.request,numeric_grant.expires_at_ms,numeric_grant.audio_requested,state.numeric_pairing.Confirmed(),now)) {
                message="Mira companion\nConfirmed - connecting local audio";
            } else message="Mira companion\nPairing confirmed - muted";
        }
        if(provisioner.Active()&&!provisioner.Numeric()) {
            ProvisioningFields fields; auto result=provisioner.Poll(fields);
            if(result==ProvisioningRead::Accepted) {
                AuthorizedGrant grant;grant.uri=fields.uri;grant.device=fields.device;grant.credential=std::move(fields.credential);grant.request=fields.request;
                grant.host_audio_qualified=STACKCHAN_COMPANION_TRUSTED_USB_PROVISIONING;
                bool staged=StageAuthorizedGrant(std::move(grant));
                bool receipt_sent=provisioner.CompleteAccepted(staged,fields.request);
                if(!receipt_sent&&staged){
                    std::lock_guard<std::mutex> lock(grant_mutex);
                    if(pending_grant)std::fill(pending_grant->credential.begin(),pending_grant->credential.end(),'\0');
                    pending_grant.reset();staged=false;
                }
                message=staged?"Mira companion\nMuted - grant staged; explicit session authorization required":"Mira companion\nMuted - grant or acknowledgement rejected";
            } else if(result==ProvisioningRead::Rejected) message="Mira companion\nMuted - pairing expired or rejected";
            fields.Clear();
        }
        if(state.matching_consent.RequestConnection(state.numeric_pairing.ConnectionReady()))connect_requested=true;
        bool legacy_pending=false;{std::lock_guard<std::mutex> lock(grant_mutex);legacy_pending=pending_grant.has_value();}
        if(RouteRememberedConnect(RememberedRuntimePresent(),legacy_pending,state.feature_authority!=nullptr,grant_connected,state.numeric_until!=0)&&connect_requested.exchange(false)) {
            if(RememberedRuntimeStopped()&&RememberedStartAllowed())BootRememberedRuntime();
        }
        if(connect_requested.exchange(false)) {
            bool pending=false;{std::lock_guard<std::mutex> lock(grant_mutex);pending=pending_grant.has_value();}
            if(!pending&&!grant_connected&&!state.numeric_until&&!state.feature_authority&&tasks.IsStopped()&&CurrentAudioOwner()==AudioOwner::None){
                PairRecord record;
                if(LoadRememberedPair(record)){
                    auto policy=CurrentPairPolicy();
                    uint8_t bytes[16];esp_fill_random(bytes,sizeof bytes);static const char hex[]="0123456789abcdef";std::string request;for(auto b:bytes){request.push_back(hex[b>>4]);request.push_back(hex[b&15]);}
                    AuthorizedGrant grant;grant.uri=record.uri;grant.device=record.device;grant.credential=record.credential;grant.request=request;grant.host_audio_qualified=true;
                    grant.expires_at_ms=now+uint64_t(record.expiry-policy.now)*1000;
                    if(ValidPairRecord(record,policy)&&PairHex(request)&&StageAuthorizedGrant(std::move(grant))){state.saved_deadline=now+uint64_t(record.expiry-policy.now)*1000;state.saved_expiry=record.expiry;}
                }
                ClearPairRecord(record);
            }
            bool usb_closed=provisioner.Close();
            bool have_grant=false;{std::lock_guard<std::mutex> lock(grant_mutex);have_grant=pending_grant.has_value();}
            connect_gate.Request(usb_closed&&have_grant);
            if(!usb_closed||!have_grant)state.matching_consent.Cancel();
            if(!usb_closed)message="Mira companion\nMuted - USB cleanup pending; use Back to stop";
            else if(!have_grant)message="Mira companion\nMuted - trusted host grant required";
            else if(!wifi_started){wifi_started=StartSavedWifi();message=wifi_started?"Mira companion\nConnecting saved Wi-Fi":"Mira companion\nMuted - Wi-Fi unavailable";}
            else if(!connected){wifi_started=StartSavedWifi();message=wifi_started?"Mira companion\nConnecting saved Wi-Fi":"Mira companion\nWi-Fi request failed";}
        }
        if(connect_gate.Ready(wifi_started && connected) && !grant_connected) {
            std::optional<AuthorizedGrant> grant;{std::lock_guard<std::mutex> lock(grant_mutex);grant=std::move(pending_grant);pending_grant.reset();}
            if(grant) {
                connect_gate.Cancel();
                if((grant->feature_authority&&!grant->feature_authority->MatchesAudioGrant(grant->request,grant->expires_at_ms,static_cast<uint64_t>(esp_timer_get_time()/1000)))||(grant->expires_at_ms&&static_cast<uint64_t>(esp_timer_get_time()/1000)>=grant->expires_at_ms)){std::fill(grant->credential.begin(),grant->credential.end(),'\0');mute_requested=true;return;}
                auto network=Board::GetInstance().GetNetwork();
                grant_connected=network && socket.Connect(*network,grant->uri,grant->device,grant->credential,grant->request);
                uint64_t after_connect=esp_timer_get_time()/1000;
                if(grant_connected&&((grant->expires_at_ms&&after_connect>=grant->expires_at_ms)||(grant->numeric_pairing&&!state.numeric_pairing.Active(after_connect))||(grant->feature_authority&&!grant->feature_authority->MatchesAudioGrant(grant->request,grant->expires_at_ms,after_connect)))){state.reactive.RequestStopFor(state.audio_lease);socket.Close();grant_connected=false;}
                if(grant_connected){state.owns_audio=state.audio_connection_epoch!=UINT64_MAX&&AcquireAudio(AudioOwner::Mira,AudioPurpose::Session,++state.audio_connection_epoch,state.audio_lease);const auto captured_lease=state.audio_lease;
                    bool borrower_bound=true;const auto asset=wake_runtime::QualifiedAssetView();
                    if(wake_runtime::kLiveBorrowedProcessingAvailable&&asset.qualification&&asset.bytes&&asset.size)borrower_bound=state.borrowed_factory.CreateQualified(captured_lease.generation(),state.local_consent,Runtime::LocalIntentClock,nullptr,asset.bytes,asset.size)&&tasks.BindBorrowedCpu(state.borrowed_factory.Endpoint());
                    const bool reactive_bound=borrower_bound&&state.owns_audio&&state.reactive.Bind(captured_lease)&&tasks.BindAudioLease(captured_lease)&&
                        tasks.ConfigureLevelObserver(captured_lease.generation(),[captured_lease,&state](uint64_t generation,uint32_t epoch,uint16_t peak,bool playback){
                            state.reactive.Observe(captured_lease,generation,epoch,state.socket.Epoch(),peak,playback,uint32_t(esp_timer_get_time()/1000));
                        });
                    grant_connected=reactive_bound && tasks.Start(Board::GetInstance().GetAudioCodec(),[&socket](const uint8_t* p,size_t n){return socket.SendAudio(p,n);},[captured_lease,&state](uint32_t,bool){state.reactive.RequestStopFor(captured_lease);mute_requested=true;});}
                std::fill(grant->credential.begin(),grant->credential.end(),'\0');
                if(!grant_connected){state.matching_consent.Cancel();state.reactive.RequestStopFor(state.audio_lease);socket.Close();if(StopReactiveTasks(state)&&state.owns_audio){state.owns_audio=!ReleaseAudio(state.audio_lease,true);}message="Mira companion\nMuted - host connection failed";}
            }
        }
        if(grant_connected && state.matching_consent.Take(esp_timer_get_time()/1000,socket.GetState()==CompanionSocket::State::ConsentRequired)) {
            // Consent was explicitly included in the fresh physical code confirmation.
            if(!socket.PhysicalConsent())mute_requested=true;
        }
        if(state.feature_listen){
            auto intent=std::move(*state.feature_listen);state.feature_listen.reset();
            if(state.feature_authority!=intent.owner||intent.request!=intent.owner->PairRequest()||!intent.owner->AudioAuthorityCurrent(esp_timer_get_time()/1000)||!socket.PhysicalConsent(intent.epoch)){
                message="Mira companion\nMuted - captured session/epoch changed";mute_requested=true;
            }
        }
        if(auto captured=remembered_listen_requested.exchange(0)){if(!RememberedPhysicalListen(captured))message="Mira companion\nMuted - remembered Listen session changed";}
        if(auto captured=listen_epoch_requested.exchange(0)){
            if(!grant_connected||!socket.PhysicalConsent(captured))message="Mira companion\nMuted - Listen session changed";
            else if(state.local_intent.AcceptPhysicalListen(state.audio_lease,captured,state.borrowed_physical_proof))state.tasks.StageBorrowedPhysicalProof(state.audio_lease,captured,state.borrowed_physical_proof);
        }
        if(listen_requested.exchange(false)) {
            if(!grant_connected || !socket.PhysicalConsent())message="Mira companion\nMuted - host session not ready";
        }
        led_expression::PublishStatus(RememberedAudioArmed()?led_expression::Status::Listening:state.numeric_pairing.Code().empty()?led_expression::Status::None:led_expression::Status::Pairing);
        if(grant_connected) {
            socket.Tick();using S=CompanionSocket::State;
            switch(socket.GetState()) {
                case S::AwaitingBegin:message="Mira companion\nMuted - waiting for host session";break;
                case S::ConsentRequired:message="Mira companion\nMuted - explicit session authorization required";break;
                case S::Arming:message="Mira companion\nMuted - waiting for host approval";break;
                case S::Listening:led_expression::PublishStatus(led_expression::Status::Listening);message="Mira companion\nListening";break;
                case S::Speaking:led_expression::PublishStatus(led_expression::Status::Speaking);message="Mira companion\nSpeaking";break;
                case S::Muted:case S::Disconnected:led_expression::PublishStatus(led_expression::Status::None);mute_requested=true;break;
                case S::Fault:led_expression::PublishStatus(led_expression::Status::Fault);mute_requested=true;break;
            }
        }
        bool diagnostic_pending=false;{std::lock_guard<std::mutex> lock(grant_mutex);diagnostic_pending=pending_grant.has_value();}
        bool export_ready=RememberedRuntimeStopped()&&!grant_connected&&!state.numeric_until&&!diagnostic_pending&&!provisioner.Active()&&tasks.IsStopped()&&CurrentAudioOwner()==AudioOwner::None;
        const bool public_status_ready=PublicStatusIdle();
        diagnostic::Stage(diagnostic::Task::Main,diagnostic::StageCode::LvglWait);
        {LvglLockGuard lock;diagnostic::Stage(diagnostic::Task::Main,diagnostic::StageCode::LvglAcquired);
            if(export_ready)lv_obj_remove_state(state.diagnostic_button,LV_STATE_DISABLED);else lv_obj_add_state(state.diagnostic_button,LV_STATE_DISABLED);
            if(public_status_ready)lv_obj_remove_state(state.public_status_button,LV_STATE_DISABLED);else lv_obj_add_state(state.public_status_button,LV_STATE_DISABLED);
            bool compare=!state.numeric_pairing.Code().empty();
            state.pair_ui.Render(state.numeric_pairing.Code(),state.numeric_request.request,state.numeric_pairing.AudioRequested());
            if(compare&&state.numeric_pairing.Confirmed())lv_obj_add_state(state.pair_button,LV_STATE_DISABLED);
            else lv_obj_remove_state(state.pair_button,LV_STATE_DISABLED);
            if(!grant_connected&&!state.numeric_until&&tasks.IsStopped()&&RememberedRuntimeStopped())lv_obj_remove_state(state.connect_button,LV_STATE_DISABLED);else lv_obj_add_state(state.connect_button,LV_STATE_DISABLED);
            if(CaptureRememberedListen()||(grant_connected&&socket.GetState()==CompanionSocket::State::ConsentRequired))lv_obj_remove_state(state.listen_button,LV_STATE_DISABLED);else lv_obj_add_state(state.listen_button,LV_STATE_DISABLED);
            if(export_ready)lv_obj_remove_state(state.remember_button,LV_STATE_DISABLED);else lv_obj_add_state(state.remember_button,LV_STATE_DISABLED);
            auto connect_label=lv_obj_get_child(state.connect_button,0);lv_label_set_text(connect_label,RouteRememberedConnect(RememberedRuntimePresent(),diagnostic_pending,state.feature_authority!=nullptr,grant_connected,state.numeric_until!=0)?"Sync":"Connect");
            auto pair_label=lv_obj_get_child(state.pair_button,0);
            lv_label_set_text(pair_label,compare&&state.remember_enrollment?"Pair and Remember":compare?(state.numeric_pairing.AudioRequested()?"Pair & authorize mic/speaker":"Pair - confirm code"):"Pair");
            if(state.audio_lease)state.reactive.UpdateMediaEpoch(state.audio_lease,socket.Epoch());
            if(!compare&&RouteRememberedStatus(RememberedRuntimePresent(),diagnostic_pending,state.feature_authority!=nullptr,grant_connected,state.numeric_until!=0)){if(auto text=RememberedStatusText())message=text;}
            std::string camera_prompt;
            if(!compare&&!state.public_status_root)camera_prompt=RememberedTransportActive()?ShowRememberedCameraPermission():socket.ShowCameraPermission();
            else{socket.HideCameraPermission();HideRememberedCameraPermission();}
            state.details_view.UpdatePrompt(compare?"pair:"+state.numeric_pairing.Code():camera_prompt.empty()?"":"camera:"+camera_prompt);
            std::string display=compare?"Compare code: "+state.numeric_pairing.Code()+(state.numeric_pairing.AudioRequested()?"\nConfirm permits local mic/speaker\nfor this temporary session":"\nPairing only - audio stays muted")+(state.numeric_pairing.Confirmed()?"\nRobot confirmed - confirm on Mac":"\nCompare on Mac, then tap Pair"):message;if(!camera_prompt.empty())display=camera_prompt;SetViewStatus(status,display.c_str());UpdateViewPresentation(display.c_str(),compare,!camera_prompt.empty());
        }
        diagnostic::Stage(diagnostic::Task::Main,diagnostic::StageCode::LvglReleased);
 }
[[noreturn]] void RunBoot() {
    if(!GetHAL().initCompanion())while(true)GetHAL().delay(1000);
    OpenView(lv_screen_active());
    while(true){TickView();GetHAL().delay(20);}
}
bool BindReactiveAvatar(const AudioLease& lease){return runtime().reactive.Bind(lease);}
bool ObserveReactiveAvatar(const AudioLease& lease,uint64_t generation,uint32_t epoch,uint32_t current_epoch,uint16_t peak,bool playback,uint32_t now){return runtime().reactive.Observe(lease,generation,epoch,current_epoch,peak,playback,now);}
void UpdateReactiveMediaEpoch(const AudioLease& lease,uint32_t epoch){runtime().reactive.UpdateMediaEpoch(lease,epoch);}
void StopReactiveAvatar(const AudioLease& lease){runtime().reactive.RequestStopFor(lease);}
void ClosedReactiveAvatar(const AudioLease& lease,bool checked){runtime().reactive.ClosedFor(lease,checked);}
bool CompanionClosingPending(){return runtime().closing_view.Pending();}
void RenderClosingCompanionView(){
    auto& state=runtime();LvglLockGuard lock;
    // This is a render-only path: no TickView, reconnect, capture, playback or
    // normal controls execute while Back waits for closure.
    if(state.closing_fallback)lv_label_set_text(state.closing_fallback,state.closing_view.Text());
    else if(state.status){
        lv_label_set_text(state.status,"Mic/output state unverified\nStop unconfirmed - retrying closure");
        UpdateViewPresentation("Mic/output state unverified\nStop unconfirmed - retrying closure",false,false,true);
    }
}
void DetachClosingCompanionView(){
    auto& state=runtime();state.closing_view.Retain();
    {LvglLockGuard lock;
        CancelRememberedViewConnection();state.details_view.Detach();state.reactive_view.Detach();state.view_supported=false;state.status=nullptr;state.status_panel=state.connection_badge=state.presentation_toggle=state.details_toggle=nullptr;
        state.diagnostic_button=state.public_status_button=state.pair_button=state.remember_button=state.connect_button=state.listen_button=nullptr;
        if(!state.closing_fallback){
            state.closing_fallback=lv_label_create(lv_layer_top());lv_obj_set_pos(state.closing_fallback,0,188);lv_obj_set_size(state.closing_fallback,320,52);
            lv_obj_set_style_text_font(state.closing_fallback,&lv_font_montserrat_14,0);
            lv_obj_set_style_bg_color(state.closing_fallback,lv_color_hex(0x10232d),0);lv_obj_set_style_bg_opa(state.closing_fallback,LV_OPA_COVER,0);
        }
        lv_label_set_text(state.closing_fallback,state.closing_view.Text());
    }
    // App/root deletion cannot destroy Runtime, codec callbacks, custody or
    // this top-layer fallback. A cleanup-only worker retries checked closure.
    if(!state.closing_view.BeginWorker())return;
    TaskHandle_t cleanup=nullptr;
    if(xTaskCreatePinnedToCore([](void*){
        auto& owned=runtime();
        while(owned.closing_view.Pending()){
            owned.closing_view.ReportClosed(CloseViewOwned());RenderClosingCompanionView();
            if(owned.closing_view.Pending())vTaskDelay(pdMS_TO_TICKS(100));
        }
        {LvglLockGuard lock;if(owned.closing_fallback){lv_obj_delete(owned.closing_fallback);owned.closing_fallback=nullptr;}}
        owned.closing_view.EndWorker();vTaskDelete(nullptr);
    },"companion_close",8192,nullptr,3,&cleanup,0)!=pdPASS){
        state.closing_view.EndWorker();state.closing_view.ReportClosed(false);RenderClosingCompanionView();
        // No forced deletion/release. A later Companion Tick is a cleanup-only
        // retry; new view activation stays gated until checked closure succeeds.
    }
}
}
