#pragma once
#include <cstdint>
// Checked read-modify-write of one AW9523 P0 output bit, independent of ESP-IDF so it is host-tested.
// Bus: int Read(uint8_t reg,uint8_t&v); int Write(uint8_t reg,uint8_t v) (0 = success, else error).
// Returns 0 only when the output latch reads back exactly as written: requested bit at the requested level
// and every other bit equal to the value read immediately before. Any bus error is returned unchanged.
// kNotOutput/kReadbackMismatch are distinct from bus errors (which are ESP error codes, all > 0x100 or small).
namespace aw9523 {
constexpr uint8_t kOutP0=0x02,kConfigP0=0x04;
constexpr int kNotOutput=-1001,kReadbackMismatch=-1002;
template<class Bus> int SetP0Bit(Bus& bus,uint8_t bit,bool level){
    if(bit>7)return kNotOutput;
    const uint8_t mask=uint8_t(1u<<bit);uint8_t config=0,before=0,after=0;
    if(int e=bus.Read(kConfigP0,config))return e;
    if(config&mask)return kNotOutput; // configured as input: never drive it
    if(int e=bus.Read(kOutP0,before))return e;
    const uint8_t want=level?uint8_t(before|mask):uint8_t(before&~mask);
    if(int e=bus.Write(kOutP0,want))return e;
    if(int e=bus.Read(kOutP0,after))return e;
    return after==want?0:kReadbackMismatch;
}
}
