#pragma once
#include "usb_serial_owner.hpp"
#include <usb_composite_v2/controller.hpp>
#include <driver/usb_serial_jtag.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
namespace direct_usb_pcm {
struct SdkSerial {
 bool OnOwnerCore()const{return xPortGetCoreID()==0;}
 bool Installed()const{return usb_serial_jtag_is_driver_installed();}
 bool Install(){usb_serial_jtag_driver_config_t c={};c.rx_buffer_size=8192;c.tx_buffer_size=8192;return usb_serial_jtag_driver_install(&c)==ESP_OK;}
 bool Uninstall(){return usb_serial_jtag_driver_uninstall()==ESP_OK;}
 int Read(uint8_t* p,size_t n){return usb_serial_jtag_read_bytes(p,n,0);}
 int Write(const uint8_t* p,size_t n){return usb_serial_jtag_write_bytes(p,n,0);}
};
struct SharedUsbClaim {
 bool Take(){bool expected=false;return usb_composite_v2::composite_claim.compare_exchange_strong(expected,true);}
 void Release(){usb_composite_v2::composite_claim.store(false);}
};
}
