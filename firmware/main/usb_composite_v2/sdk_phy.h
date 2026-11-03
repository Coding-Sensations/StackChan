#pragma once
#include <esp_private/usb_phy.h>
namespace usb_composite_v2 {
class SdkPhy {
 usb_phy_handle_t handle_=nullptr;esp_err_t last_=ESP_OK;
public:
 bool LegacyQuiescent(bool boot_owner_closed)const;
 bool Open(bool self_powered=false,int vbus_gpio=-1);
 bool Delete();
 esp_err_t LastError()const{return last_;}
};
}
