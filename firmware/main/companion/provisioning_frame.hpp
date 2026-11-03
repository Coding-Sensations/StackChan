#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

namespace companion {
struct ProvisioningFields {
    std::string uri, device, credential, request;
    void Clear() { std::fill(credential.begin(), credential.end(), '\0'); credential.clear(); uri.clear(); device.clear(); request.clear(); }
};
// SCPG v1: magic/version/body-length, then four u16-length ASCII fields.
// No qualification bit in client bytes. Trust and physical consent are separate.
inline bool DecodeProvisioningFrame(const uint8_t* bytes, size_t size, ProvisioningFields& out) {
    out.Clear();
    if (!bytes || size < 19 || size > 755 || bytes[0]!='S' || bytes[1]!='C' || bytes[2]!='P' || bytes[3]!='G' || bytes[4]!=1) return false;
    if ((static_cast<size_t>(bytes[5])<<8 | bytes[6]) != size-7) return false;
    constexpr size_t limits[4]={256,100,256,128};
    size_t offsets[4], lengths[4], pos=7;
    for(size_t i=0;i<4;++i) {
        if(pos+2>size) return false;
        size_t n=static_cast<size_t>(bytes[pos])<<8 | bytes[pos+1]; pos+=2;
        if(!n || n>limits[i] || pos+n>size) return false;
        for(size_t j=pos;j<pos+n;++j) if(bytes[j]<33 || bytes[j]>126) return false;
        offsets[i]=pos; lengths[i]=n; pos+=n;
    }
    if(pos!=size) return false;
    std::string uri(reinterpret_cast<const char*>(bytes+offsets[0]),lengths[0]);
    if(uri.rfind("wss://",0)!=0 || uri.find_first_of("@?#")!=std::string::npos || uri.size()<23 || uri.substr(uri.size()-16)!="/companion/audio") return false;
    auto path=uri.find('/',6); if(path==std::string::npos || path==6 || path!=uri.size()-16) return false;
    out.uri=std::move(uri);
    out.device.assign(reinterpret_cast<const char*>(bytes+offsets[1]),lengths[1]);
    out.credential.assign(reinterpret_cast<const char*>(bytes+offsets[2]),lengths[2]);
    out.request.assign(reinterpret_cast<const char*>(bytes+offsets[3]),lengths[3]);
    return true;
}
// Caller owns a physically opened, volatile window. No default-open USB listener.
class ProvisioningWindow {
    uint64_t deadline_=0;
public:
    void Open(uint64_t now_ms) { deadline_=now_ms<=UINT64_MAX-60000 ? now_ms+60000 : 0; }
    void Close() { deadline_=0; }
    bool Accept(const uint8_t* bytes,size_t size,uint64_t now_ms,const std::string& local_device,bool qualified,ProvisioningFields& out) {
        out.Clear();
        if(!deadline_ || now_ms>=deadline_ || !qualified) return false;
        if(!DecodeProvisioningFrame(bytes,size,out)) return false;
        if(out.device!=local_device) { out.Clear(); return false; }
        Close(); return true; // one accepted grant per physical window
    }
};
}
