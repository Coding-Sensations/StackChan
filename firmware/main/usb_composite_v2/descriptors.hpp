#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
namespace usb_composite_v2 {
// USB 2.0 full-speed, CDC ACM + boot keyboard + MIDI 1.0, one cable.
// Five interfaces and six non-control endpoint addresses over numbers 1..4.
inline constexpr std::array<uint8_t,63> keyboard_report={
 0x05,0x01,0x09,0x06,0xa1,0x01,0x05,0x07,0x19,0xe0,0x29,0xe7,0x15,0x00,0x25,0x01,
 0x75,0x01,0x95,0x08,0x81,0x02,0x95,0x01,0x75,0x08,0x81,0x01,
 0x95,0x05,0x75,0x01,0x05,0x08,0x19,0x01,0x29,0x05,0x91,0x02,
 0x95,0x01,0x75,0x03,0x91,0x01,0x95,0x06,0x75,0x08,0x15,0x00,0x25,0x65,
 0x05,0x07,0x19,0x00,0x29,0x65,0x81,0x00,0xc0};
inline constexpr uint8_t configuration[]={
 9,2,192,0,5,1,0,0x80,50,
 // CDC IAD, communication interface, functional descriptors, interrupt IN.
 8,11,0,2,2,2,0,0, 9,4,0,0,1,2,2,0,4,
 5,0x24,0,0x20,0x01, 5,0x24,1,0,1, 4,0x24,2,6, 5,0x24,6,0,1,
 7,5,0x81,3,8,0,16,
 9,4,1,0,2,0x0a,0,0,0, 7,5,0x02,2,64,0,0, 7,5,0x82,2,64,0,0,
 // Boot keyboard (report ID zero, 8-byte input, LED output via control EP0).
 9,4,2,0,1,3,1,1,5, 9,0x21,0x11,0x01,0,1,0x22,63,0, 7,5,0x83,3,8,0,10,
 // AudioControl plus MIDIStreaming class-specific header and one jack pair.
 9,4,3,0,0,1,1,0,6, 9,0x24,1,0x00,0x01,9,0,1,4,
 9,4,4,0,2,1,3,0,0, 7,0x24,1,0x00,0x01,65,0,
 6,0x24,2,1,1,0, 6,0x24,2,2,2,0,
 9,0x24,3,1,3,1,2,1,0, 9,0x24,3,2,4,1,1,1,0,
 9,5,0x04,2,64,0,0,0,0, 5,0x25,1,1,1,
 9,5,0x84,2,64,0,0,0,0, 5,0x25,1,1,3
};
static_assert(sizeof(configuration)==192);
// No fabricated VID/PID: caller must supply an approved distinct composite ID.
inline std::array<uint8_t,sizeof(configuration)> Configuration(bool self_powered,unsigned bus_power_ma){
 std::array<uint8_t,sizeof(configuration)> result{};
 for(size_t i=0;i<result.size();++i)result[i]=configuration[i];
 result[7]=self_powered?0xc0:0x80;
 result[8]=uint8_t((bus_power_ma+1)/2); // Caller validates qualified <=500 mA.
 return result;
}
inline std::array<uint8_t,18> Device(uint16_t vid,uint16_t pid){
 return {18,1,0,2,0xef,2,1,64,uint8_t(vid),uint8_t(vid>>8),uint8_t(pid),uint8_t(pid>>8),0,1,1,2,3,1};
}
}
