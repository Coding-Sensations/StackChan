#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace companion {
constexpr size_t CameraBGRABytes=320*240*4,CameraWireMax=4112,CameraWireHeader=56;
struct CameraVideoIdentity {const char* device;const char* conversation;const char* turn;uint64_t generation,epoch;};
inline size_t CameraIdentifierLength(const char* s,size_t max) noexcept {
    if(!s)return 0;
    size_t n=0;
    for(;n<=max&&s[n];++n){const char c=s[n];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c==':'))return 0;}
    return n&&n<=max?n:0;
}
inline void CameraPut16(uint8_t* p,uint16_t n){p[0]=uint8_t(n>>8);p[1]=uint8_t(n);}
inline void CameraPut32(uint8_t* p,uint32_t n){for(int i=3;i>=0;--i){p[i]=uint8_t(n);n>>=8;}}
inline void CameraPut64(uint8_t* p,uint64_t n){for(int i=7;i>=0;--i){p[i]=uint8_t(n);n>>=8;}}
// Framing alone grants no permission; caller must hold current issuer authority
// separately and recheck it before each fragment dispatch outside producer lock.
inline size_t EncodeCameraFragment(CameraVideoIdentity id,uint64_t sequence,uint64_t producer_captured_ns,
                                  const uint8_t* bgra,size_t length,size_t offset,uint8_t* out,size_t capacity,size_t& consumed) {
    consumed=0;
    const size_t d=CameraIdentifierLength(id.device,64),c=CameraIdentifierLength(id.conversation,128),t=CameraIdentifierLength(id.turn,128);
    const size_t start=CameraWireHeader+d+c+t;
    if(!d||!c||!t||!id.generation||!id.epoch||!sequence||!producer_captured_ns||!bgra||length!=CameraBGRABytes||offset>=length||
       !out||capacity>CameraWireMax||capacity<=start)return 0;
    const size_t count=length-offset<capacity-start?length-offset:capacity-start;
    const uintptr_t src=reinterpret_cast<uintptr_t>(bgra),dst=reinterpret_cast<uintptr_t>(out);
    if(src>UINTPTR_MAX-length||dst>UINTPTR_MAX-capacity||(dst<src+length&&src<dst+capacity))return 0;
    std::memcpy(out,"WCV1",4);out[4]=1;out[5]=1;CameraPut16(out+6,320);CameraPut16(out+8,240);
    CameraPut64(out+10,id.generation);CameraPut64(out+18,id.epoch);CameraPut64(out+26,sequence);CameraPut64(out+34,producer_captured_ns);
    CameraPut32(out+42,uint32_t(length));CameraPut32(out+46,uint32_t(offset));CameraPut16(out+50,uint16_t(d));CameraPut16(out+52,uint16_t(c));CameraPut16(out+54,uint16_t(t));
    std::memcpy(out+56,id.device,d);std::memcpy(out+56+d,id.conversation,c);std::memcpy(out+56+d+c,id.turn,t);
    std::memcpy(out+start,bgra+offset,count);consumed=count;return start+count;
}
}
