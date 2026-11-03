#pragma once
#include "wol_packet.hpp"
#include <charconv>
#include <string_view>
namespace stack_tools {
inline bool ParseMac(std::string_view text,MacAddress& out) {
    if(text.size()!=17)return false;
    MacAddress candidate{};
    for(unsigned i=0;i<6;++i){
        if(i && text[i*3-1]!=':')return false;
        unsigned byte=0;
        auto token=text.substr(i*3,2);
        auto result=std::from_chars(token.data(),token.data()+2,byte,16);
        if(result.ec!=std::errc{} || result.ptr!=token.data()+2 || byte>255)return false;
        candidate[i]=static_cast<uint8_t>(byte);
    }
    MagicPacket test{};if(!MakeMagicPacket(candidate,test))return false;
    out=candidate;return true;
}
inline bool ParseIpv4(std::string_view text,uint32_t& out) {
    uint32_t candidate=0;
    for(unsigned i=0;i<4;++i){
        auto dot=text.find('.');auto token=text.substr(0,dot);
        if(token.empty() || token.size()>3 || (token.size()>1 && token.front()=='0'))return false;
        unsigned octet=0;auto r=std::from_chars(token.data(),token.data()+token.size(),octet);
        if(r.ec!=std::errc{} || r.ptr!=token.data()+token.size() || octet>255)return false;
        candidate=(candidate<<8)|octet;
        if(i==3){if(dot!=std::string_view::npos)return false;}
        else {if(dot==std::string_view::npos)return false;text.remove_prefix(dot+1);}
    }
    out=candidate;return true;
}
struct WifiObservation { bool connected=false;int rssi=0;uint32_t address=0,netmask=0; };
inline bool IsSubnetBroadcast(uint32_t target,const WifiObservation& wifi) {
    const auto host=~wifi.netmask;
    return wifi.connected && wifi.address && wifi.netmask && host>=3 && !(host&(host+1)) &&
           target==((wifi.address&wifi.netmask)|host);
}
enum class WakeResult { Sent, NoNetwork, InvalidTarget, OpenFailed, SetupFailed, SendFailed, CloseFailed };
// Owner must outlive app views. A failed close retains the exact descriptor.
template<class Ops> class OwnedWakeSocket {
    Ops ops_{};
    int socket_=-1;
public:
    OwnedWakeSocket()=default;
    OwnedWakeSocket(const OwnedWakeSocket&)=delete;
    OwnedWakeSocket& operator=(const OwnedWakeSocket&)=delete;
    Ops& Operations(){return ops_;}
    bool PendingClosure() const{return socket_>=0;}
    bool Open(){if(PendingClosure())return false;socket_=ops_.Open();return socket_>=0;}
    bool ConfigureBroadcast(){return socket_>=0 && ops_.ConfigureBroadcast(socket_);}
    int Send(const uint8_t* bytes,size_t count,uint32_t target,uint16_t port){return socket_>=0 ? ops_.Send(socket_,bytes,count,target,port) : -1;}
    bool Close(){if(socket_<0)return true;if(!ops_.Close(socket_))return false;socket_=-1;return true;}
};
// Attempt owned close on every outcome after Open succeeds; failure retains custody.
template<class Transport> WakeResult SendWakeOnce(Transport& transport,std::string_view mac_text,std::string_view broadcast_text,const WifiObservation& wifi) {
    if(!wifi.connected)return WakeResult::NoNetwork;
    MacAddress mac{};uint32_t broadcast=0;MagicPacket packet{};
    if(!ParseMac(mac_text,mac) || !ParseIpv4(broadcast_text,broadcast) || !IsSubnetBroadcast(broadcast,wifi) || !MakeMagicPacket(mac,packet))return WakeResult::InvalidTarget;
    if(!transport.Open())return WakeResult::OpenFailed;
    auto result=WakeResult::SetupFailed;
    if(transport.ConfigureBroadcast())result=transport.Send(packet.data(),packet.size(),broadcast,9)==static_cast<int>(packet.size()) ? WakeResult::Sent : WakeResult::SendFailed;
    if(!transport.Close())return WakeResult::CloseFailed;
    return result;
}
}
