#pragma once
#include <array>
#include <cstdint>
namespace stack_tools {
using MacAddress = std::array<uint8_t,6>;
using MagicPacket = std::array<uint8_t,102>;
inline bool MakeMagicPacket(const MacAddress& mac, MagicPacket& output) {
    bool nonzero=false;
    for(auto octet:mac) nonzero=nonzero||octet!=0;
    if(!nonzero || (mac[0]&1))return false;
    MagicPacket candidate{};
    for(unsigned i=0;i<6;++i)candidate[i]=0xff;
    for(unsigned copy=0;copy<16;++copy)
        for(unsigned octet=0;octet<6;++octet)candidate[6+copy*6+octet]=mac[octet];
    output=candidate;
    return true;
}
// Adapter supplies actual network send. Packet construction never implies wake success.
}
