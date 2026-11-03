#include "firmware_install_policy.h"
#include "firmware_install_ui.h"
#include <ssid_manager.h>
#include <apps/network_setup/wifi_credentials.hpp>
#include <nvs.h>
namespace stock_services {
bool PrepareOriginalFirmwareWifi(){
 if(!FirmwareInstallAllowed())return false;
 const auto& list=SsidManager::GetInstance().GetSsidList();
 if(list.empty()||list.size()>10)return false;
 for(const auto& c:list)if(!network_setup::Valid({c.ssid,c.password}))return false;
 nvs_handle_t handle=0;
 if(nvs_open("wifi",NVS_READWRITE,&handle)!=ESP_OK)return false;
 bool okay=true;
 for(size_t i=0;i<10&&okay;++i){
  auto suffix=i?std::to_string(i):std::string{};
  auto ssid="ssid"+suffix,password="password"+suffix;
  if(i<list.size()){
   okay=nvs_set_str(handle,ssid.c_str(),list[i].ssid.c_str())==ESP_OK;
   if(okay)okay=nvs_set_str(handle,password.c_str(),list[i].password.c_str())==ESP_OK;
  }else{
   auto a=nvs_erase_key(handle,ssid.c_str()),b=nvs_erase_key(handle,password.c_str());
   okay=(a==ESP_OK||a==ESP_ERR_NVS_NOT_FOUND)&&(b==ESP_OK||b==ESP_ERR_NVS_NOT_FOUND);
  }
 }
 if(okay)okay=nvs_commit(handle)==ESP_OK;
 // Preserve the authoritative custom blob even if legacy export fails. Refuse
 // any image write/boot selection unless legacy export and readback succeed.
 for(size_t i=0;i<list.size()&&okay;++i){
  auto suffix=i?std::to_string(i):std::string{};char ssid[33]{},password[65]{};
  size_t a=sizeof(ssid),b=sizeof(password);
  okay=nvs_get_str(handle,("ssid"+suffix).c_str(),ssid,&a)==ESP_OK&&nvs_get_str(handle,("password"+suffix).c_str(),password,&b)==ESP_OK&&list[i].ssid==ssid&&list[i].password==password;
 }
 nvs_close(handle);return okay;
}
}
