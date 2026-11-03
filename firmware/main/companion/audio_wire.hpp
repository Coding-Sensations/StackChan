#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace companion {
constexpr std::size_t wire_header=16, opus_max=4096;
struct WireFrame { uint32_t epoch,sequence; bool speaker; const uint8_t* payload; std::size_t size; };
inline uint32_t be32(const uint8_t* p) { return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3]; }
inline void put32(uint8_t* p,uint32_t n) { p[0]=n>>24; p[1]=n>>16; p[2]=n>>8; p[3]=n; }
inline bool decode_wire(const uint8_t* data,std::size_t size,WireFrame& out) {
    if(!data || size<wire_header || size>wire_header+opus_max || std::memcmp(data,"SCAV",4) || data[4]!=1 || (data[5]!=1 && data[5]!=2)) return false;
    auto bytes=std::size_t(data[14])*256+data[15];
    if(!bytes || bytes!=size-wire_header || !be32(data+6) || be32(data+10)==UINT32_MAX) return false;
    out={be32(data+6),be32(data+10),data[5]==2,data+wire_header,bytes}; return true;
}
inline std::size_t encode_wire(const WireFrame& in,uint8_t* out,std::size_t capacity) {
    if(!out || !in.payload || !in.epoch || in.sequence==UINT32_MAX || !in.size || in.size>opus_max || capacity<wire_header+in.size) return 0;
    std::memcpy(out,"SCAV",4); out[4]=1; out[5]=in.speaker?2:1;
    put32(out+6,in.epoch); put32(out+10,in.sequence);out[14]=in.size>>8;out[15]=in.size;
    std::memcpy(out+wire_header,in.payload,in.size);return wire_header+in.size;
}
}
