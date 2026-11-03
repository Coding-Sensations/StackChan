#pragma once
#include <array>
#include <string_view>
#define COMPANION_TLS_CHECKED_PORT 1
namespace companion {
inline bool ParseTlsPort(std::string_view text,int& port) {
    if(text.empty() || text.size()>5)return false;
    unsigned value=0;
    for(char c:text){if(c<'0'||c>'9')return false;value=value*10+unsigned(c-'0');}
    if(value<1 || value>65535)return false;
    port=static_cast<int>(value);return true;
}

enum class TlsRoute { Public, Local, Reject };
inline bool CanonicalPrivateIpv4(std::string_view host) {
    std::array<unsigned,4> parts{};
    for (unsigned i=0; i<4; ++i) {
        auto dot=host.find('.'); auto part=host.substr(0,dot);
        if(part.empty() || part.size()>3 || (part.size()>1 && part.front()=='0'))return false;
        for(char c:part){if(c<'0'||c>'9')return false;parts[i]=parts[i]*10+unsigned(c-'0');}
        if(parts[i]>255)return false;
        if(i==3){if(dot!=std::string_view::npos)return false;}
        else {if(dot==std::string_view::npos)return false;host.remove_prefix(dot+1);}
    }
    return parts[0]==10 || (parts[0]==172 && parts[1]>=16 && parts[1]<=31) ||
        (parts[0]==192 && parts[1]==168);
}
inline TlsRoute SelectTlsRoute(std::string_view ca, std::string_view approved_host, std::string_view requested_host) {
    if(ca.empty() && approved_host.empty())return TlsRoute::Public;
    if(ca.empty() || ca.size()>16384 || !CanonicalPrivateIpv4(approved_host) || requested_host!=approved_host)return TlsRoute::Reject;
    return TlsRoute::Local;
}
template<class Config> inline bool ConfigureLocalTls(Config& cfg,const char* ca,const char* approved,std::string_view host,int port) {
    if(!ca || !approved || SelectTlsRoute(ca,approved,host)!=TlsRoute::Local || port<1 || port>65535)return false;
    cfg.cacert_buf=reinterpret_cast<const unsigned char*>(ca);
    cfg.cacert_bytes=std::string_view(ca).size()+1;
    cfg.common_name=approved;cfg.skip_common_name=false;
    cfg.crt_bundle_attach=nullptr;cfg.timeout_ms=5000;
    return true;
}

}
