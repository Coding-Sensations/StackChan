#include "cleanup.h"
#include "session.h"
#include <hal/hal.h>
#include <hal/board/hal_bridge.h>
#include <hal/board/stackchan_display.h>
#include <stackchan/stackchan.h>
#include <apps/common/common.h>
#include <apps/network_setup/persistence_gate.h>
#include <apps/stock_services/firmware_install_ui.h>
#include <companion/boot.h>
#include <companion/station_authority.hpp>
#include <companion/station_event_barrier.hpp>
#include <wifi_manager.h>
#include <board.h>
#include <esp_wifi.h>
#include <esp_sntp.h>
#include <atomic>
namespace runtime_lifecycle {
namespace {std::atomic<int> job{0};mooncake::Mooncake* old_shell=nullptr;}
bool VendorNetworkAvailable(){
 if(companion::network::Authority().Current()!=companion::network::Owner::None)return false;
 wifi_mode_t mode{};const auto error=esp_wifi_get_mode(&mode);
 return error==ESP_ERR_WIFI_NOT_INIT||(error==ESP_OK&&mode==WIFI_MODE_NULL);
}
bool PrepareVendorNetwork(){
 if(!companion::PollSavedNetworkStopped())return false;
 return VendorNetworkAvailable();
}

void QuietHardware(){
 GetHAL().setServoPowerEnabled(false);GetHAL().setLaserEnabled(false);GetHAL().showRgbColor(0,0,0);
}
void ClearNativeSignals(){
 auto& h=GetHAL();
 h.onBleAvatarData.clear();h.onBleMotionData.clear();h.onBleRgbData.clear();h.onAppConfigEvent.clear();
 h.onWsAvatarData.clear();h.onWsMotionData.clear();h.onWsCallRequest.clear();h.onWsCallResponse.clear();
 h.onWsCallEnd.clear();h.onWsTextMessage.clear();h.onWsDanceData.clear();h.onWsLog.clear();
 h.onWsVideoFrame.clear();h.onWsVideoModeChange.clear();h.onEzdataPairCode.clear();h.onEspNowData.clear();
}
bool PollSourcesStopped(mooncake::Mooncake* shell){
 // The installer may be blocked in a bounded HTTP/flash operation. Keep its
 // UI targets and network owner until cancellation has returned and joined.
 if(!InstallerJoined())return false;
 int state=job.load();if(state==2)return true;
 if(state==1)return false;
 old_shell=shell;job=1;
 if(xTaskCreate([](void*){
    // No UI lock here: callbacks may need it to return and join.
    network_setup::EndOriginalOnboardingSession();network_setup::EndStockWifiProvisioningSession();
    const bool ble=GetHAL().stopBleServer();const bool espnow=GetHAL().stopEspNow();
    if(!ble||!espnow){job=0;vTaskDelete(nullptr);return;}
    if(old_shell){
      auto* extensions=old_shell->extensionManager();
      for(auto* ability:extensions->getAllAbilityInstance())extensions->destroyAbility(ability->getID());
      extensions->updateAbilities();
      if(extensions->getAbilityNum()!=0){job=0;vTaskDelete(nullptr);return;}
    }
    auto& board=Board::GetInstance();board.SetNetworkEventCallback(nullptr);
    auto& wifi=WifiManager::GetInstance();wifi.SetEventCallback(nullptr);
    if(wifi.IsInitialized()){wifi.StopConfigAp();wifi.StopStation();}
    wifi_mode_t mode;auto err=esp_wifi_get_mode(&mode);
    if(err==ESP_OK){
      err=esp_wifi_stop();if(err!=ESP_OK&&err!=ESP_ERR_WIFI_NOT_STARTED){job=0;vTaskDelete(nullptr);return;}
      if(esp_wifi_set_mode(WIFI_MODE_NULL)!=ESP_OK){job=0;vTaskDelete(nullptr);return;}
      if(esp_wifi_get_mode(&mode)!=ESP_OK||mode!=WIFI_MODE_NULL){job=0;vTaskDelete(nullptr);return;}
    }else if(err!=ESP_ERR_WIFI_NOT_INIT){job=0;vTaskDelete(nullptr);return;}
    if(!companion::network::DrainStationEvents()){job=0;vTaskDelete(nullptr);return;}
    if(esp_sntp_enabled())esp_sntp_stop();
    GetHAL().forgetNativeNetwork();ClearNativeSignals();
    auto& authority=companion::network::Authority();
    authority.RequestStop(companion::network::Owner::Vendor);
    if(!authority.FinishStop(companion::network::Owner::Vendor,true)){job=0;vTaskDelete(nullptr);return;}
    job=2;vTaskDelete(nullptr);
 },"runtime_stop",8192,nullptr,3,nullptr)!=pdPASS)job=0;
 return false;
}
void ResetCleanupJob(){if(job!=1){job=0;old_shell=nullptr;}}
void ResetSessionUi(){
 stock_services::ResetFirmwareInstallUi();
 LvglLockGuard lock;
 tools::on_reminder_triggered().clear();
 view::destroy_home_indicator();view::destroy_status_bar();view::reset_toast_runtime();
 static_cast<StackChanAvatarDisplay*>(Board::GetInstance().GetDisplay())->ResetSessionUi();
 GetStackChan().clearModifiers();GetStackChan().resetAvatar();
 hal_bridge::set_xiaozhi_mode(false);
}
}
