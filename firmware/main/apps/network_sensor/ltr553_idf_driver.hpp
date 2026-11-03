#pragma once
#include <hal/board/hal_bridge.h>
#include <driver/i2c_master.h>
#include <cstddef>
#include <cstdint>
namespace stack_tools {
// Explicitly started by the foreground app. Never creates/deletes the board bus.
class Ltr553IdfDriver {
    i2c_master_dev_handle_t device_=nullptr;
public:
    bool Attach() {
        if(device_)return false;
        auto bus=hal_bridge::board_get_i2c_bus();
        if(!bus || i2c_master_probe(bus,0x23,20)!=ESP_OK)return false;
        i2c_device_config_t config{};
        config.dev_addr_length=I2C_ADDR_BIT_LEN_7;
        config.device_address=0x23;config.scl_speed_hz=100000;
        return i2c_master_bus_add_device(bus,&config,&device_)==ESP_OK;
    }
    bool Read(uint8_t reg,uint8_t* bytes,size_t count) {
        return device_ && count && count<=4 && i2c_master_transmit_receive(device_,&reg,1,bytes,count,20)==ESP_OK;
    }
    bool Write(uint8_t reg,uint8_t value) {
        uint8_t bytes[]{reg,value};
        return device_ && i2c_master_transmit(device_,bytes,sizeof(bytes),20)==ESP_OK;
    }
    bool Detach() {
        if(!device_)return true;
        if(i2c_master_bus_rm_device(device_)!=ESP_OK)return false;
        device_=nullptr;return true;
    }
};
}
