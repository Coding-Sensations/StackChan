#pragma once
#include "wol_runtime.hpp"
#include <esp_wifi.h>
#include <esp_netif.h>
#include <lwip/sockets.h>
namespace stack_tools {
inline WifiObservation ObserveExistingWifi() {
    WifiObservation result;wifi_ap_record_t ap{};
    if(esp_wifi_sta_get_ap_info(&ap)!=ESP_OK)return result;
    auto netif=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");esp_netif_ip_info_t info{};
    if(!netif || esp_netif_get_ip_info(netif,&info)!=ESP_OK || !info.ip.addr)return result;
    result.connected=true;result.rssi=ap.rssi;
    result.address=ntohl(info.ip.addr);result.netmask=ntohl(info.netmask.addr);return result;
}
struct IdfWakeSocketOps {
    int Open(){return lwip_socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);}
    bool ConfigureBroadcast(int socket){
        int enabled=1;timeval timeout{2,0};
        // Supported socket setting, not a proven UDP wall-clock completion bound.
        return lwip_setsockopt(socket,SOL_SOCKET,SO_BROADCAST,&enabled,sizeof(enabled))==0 &&
               lwip_setsockopt(socket,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout))==0;
    }
    int Send(int socket,const uint8_t* bytes,size_t count,uint32_t target,uint16_t port){
        sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(port);address.sin_addr.s_addr=htonl(target);
        return static_cast<int>(lwip_sendto(socket,bytes,count,0,reinterpret_cast<sockaddr*>(&address),sizeof(address)));
    }
    bool Close(int socket){return lwip_close(socket)==0;}
};
using IdfWakeTransport=OwnedWakeSocket<IdfWakeSocketOps>;
}
