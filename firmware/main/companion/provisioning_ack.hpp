#pragma once
#include <array>
#include <cstdint>
#include <string>
namespace companion {
// SCPS v1: 22 nonsecret bytes. Grant SCPG v1 remains unchanged.
// Ready payload = boot nonce / esp_reset_reason / eight reserved zero bytes.
// Staged payload = request UUID, never a token. Legacy request IDs yield zeros.
enum class AckStatus : uint8_t { Ready=1, Staged=2, Rejected=3, Expired=4 };
using AckFrame=std::array<uint8_t,22>;
inline AckFrame AckBase(AckStatus status) {
    AckFrame out{};out[0]='S';out[1]='C';out[2]='P';out[3]='S';out[4]=1;out[5]=static_cast<uint8_t>(status);return out;
}
inline AckFrame EncodeStatusAck(AckStatus status) {
    return AckBase(status==AckStatus::Expired?status:AckStatus::Rejected);
}
inline AckFrame EncodeReadyAck(uint32_t boot_nonce,uint32_t reset_reason) {
    if(!boot_nonce||reset_reason>64)return EncodeStatusAck(AckStatus::Rejected);
    auto out=AckBase(AckStatus::Ready);
    for(unsigned i=0;i<4;++i){out[6+i]=static_cast<uint8_t>(boot_nonce>>(24-8*i));out[10+i]=static_cast<uint8_t>(reset_reason>>(24-8*i));}
    return out;
}
inline AckFrame EncodeStagedAck(const std::string& request) {
    auto out=AckBase(AckStatus::Staged);if(request.size()!=32)return out;
    auto hex=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;};
    for(unsigned i=0;i<16;++i){int a=hex(request[2*i]),b=hex(request[2*i+1]);if(a<0||b<0){for(unsigned j=6;j<22;++j)out[j]=0;return out;}out[6+i]=static_cast<uint8_t>((a<<4)|b);}
    return out;
}
}
