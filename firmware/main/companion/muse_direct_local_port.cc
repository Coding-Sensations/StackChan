// SPDX-License-Identifier: Apache-2.0
#include "muse_direct_local_port.h"
#include "muse_direct_live.h"
#include "provider_room_runtime.h"
#include "station_authority.hpp"
#include <esp_chip_info.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <cstdio>
#include <new>
namespace companion {bool IsWifiConnected();}
namespace companion::muse {
namespace {
bool ReadNetwork(LocalNetwork& output){
 output={};auto& authority=network::Authority();if(!companion::IsWifiConnected()||authority.Current()!=network::Owner::Saved)return false;
 const auto epoch=authority.Generation();wifi_ap_record_t ap{};if(esp_wifi_sta_get_ap_info(&ap)!=ESP_OK||!authority.Accepts(network::Owner::Saved,epoch))return false;
 std::memcpy(output.ssid.data(),ap.ssid,32);std::memcpy(output.bssid.data(),ap.bssid,6);output.epoch=epoch;output.valid=true;return true;
}
bool Matches(const LocalNetwork& expected){LocalNetwork actual;return expected.valid&&ReadNetwork(actual)&&expected.epoch==actual.epoch&&expected.ssid==actual.ssid&&expected.bssid==actual.bssid;}
class SavedUse final:public LocalNetworkUse {
 network::StationBorrow borrow_;LocalNetwork expected_;
public:
 SavedUse(network::StationBorrow&& borrow,const LocalNetwork& expected):borrow_(std::move(borrow)),expected_(expected){}
 bool Current()override{return bool(borrow_)&&Matches(expected_);}
};
class Port final:public LocalPort {
public:
 uint64_t Now()override{return uint64_t(esp_timer_get_time()/1000);}
 bool Foreground()override{return room::ProviderSelected(room::Provider::Muse);}
 bool Identity(LocalIdentity& output)override{
  output={};
#if defined(CONFIG_BOARD_TYPE_M5STACK_STACK_CHAN) && CONFIG_BOARD_TYPE_M5STACK_STACK_CHAN
  esp_chip_info_t chip{};esp_chip_info(&chip);if(chip.model!=CHIP_ESP32S3)return false;
  uint8_t bytes[6]{};if(esp_read_mac(bytes,ESP_MAC_WIFI_STA)!=ESP_OK)return false;
  char mac[18]{},node[32]{};std::snprintf(mac,sizeof(mac),"%02x:%02x:%02x:%02x:%02x:%02x",bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5]);std::snprintf(node,sizeof(node),"homelink-%02x%02x%02x",bytes[3],bytes[4],bytes[5]);
  output.mac=mac;output.node=node;output.device="hatch-link:"+output.mac;
#ifdef FIRMWARE_VERSION
  output.version=FIRMWARE_VERSION;
#else
  output.version="unknown";
#endif
  return Id(output.version,32);
#else
  return false;
#endif
 }
 bool Network(LocalNetwork& output)override{return ReadNetwork(output);}
 bool NetworkMatches(const LocalNetwork& expected)override{return Matches(expected);}
 std::unique_ptr<LocalNetworkUse> Borrow(const LocalNetwork& expected)override{
  if(!Matches(expected))return {};
  auto borrow=network::Authority().BorrowSaved();if(!borrow)return {};
  auto result=std::unique_ptr<LocalNetworkUse>(new(std::nothrow)SavedUse(std::move(borrow),expected));if(!result||!result->Current())return {};return result;
 }
 bool PairIdle()override{return DevicePairingCleanupComplete();}bool ChatIdle()override{return NativeSessionCleanupComplete();}
 bool PreparePair(const PairingGrant& grant,std::string_view token)override{return PrepareDevicePairing(grant,token);}
 bool PrepareChat(const SessionGrant& grant)override{return PrepareNativeSession(grant);}
 void Stop(LocalMode mode)override{if(mode==LocalMode::Pair)StopDevicePairing();else StopNativeSession();}
 bool Text(const std::string& generation,const std::string& request,const std::string& text)override{return QueueNativeText(generation,request,text);}
 bool RandomId(std::string& output)override{uint8_t random[16]{};esp_fill_random(random,sizeof(random));const char hex[]="0123456789abcdef";output="ui-";for(auto byte:random){output+=hex[byte>>4];output+=hex[byte&15];}for(volatile uint8_t& byte:random)byte=0;return true;}
};
}
LocalPort& DeviceLocalPort(){static Port port;return port;}
}
