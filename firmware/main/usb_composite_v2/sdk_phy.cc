#include "sdk_phy.h"
#include <driver/usb_serial_jtag.h>
#include <sdkconfig.h>
namespace usb_composite_v2 {
bool SdkPhy::LegacyQuiescent(bool boot_owner_closed)const{
#if defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG_ENABLED) || defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG) || defined(CONFIG_ESP_CONSOLE_SECONDARY_USB_SERIAL_JTAG)
 (void)boot_owner_closed;return false; // Secondary VFS bypasses the installed-driver check.
#else
 return boot_owner_closed&&!usb_serial_jtag_is_driver_installed();
#endif
}
bool SdkPhy::Open(bool self_powered,int vbus_gpio){
 if(handle_){last_=ESP_ERR_INVALID_STATE;return false;}
 usb_phy_config_t cfg{};cfg.controller=USB_PHY_CTRL_OTG;cfg.target=USB_PHY_TARGET_INT;cfg.otg_mode=USB_OTG_MODE_DEVICE;cfg.otg_speed=USB_PHY_SPEED_FULL;
 if(self_powered&&vbus_gpio<0){last_=ESP_ERR_INVALID_ARG;return false;}
 const usb_phy_otg_io_conf_t otg_io=USB_PHY_SELF_POWERED_DEVICE(vbus_gpio);
 if(self_powered)cfg.otg_io_conf=&otg_io;
 last_=usb_new_phy(&cfg,&handle_);return last_==ESP_OK;
}
bool SdkPhy::Delete(){
 if(!handle_)return true;
 last_=usb_del_phy(handle_);if(last_!=ESP_OK)return false;handle_=nullptr;return true;
}
}
