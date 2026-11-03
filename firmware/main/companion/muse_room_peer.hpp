#pragma once
#include "provider_room_runtime.h"
#include "provider_room_wire.hpp"
#include "local_tls_policy.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

namespace companion::room {
inline bool MuseRoomUri(const std::string &uri,const std::string &approved) {
  const std::string prefix="wss://",path="/companion/muse-room";
  if(uri.size()>256||uri.rfind(prefix,0)||uri.size()<=prefix.size()+path.size()||
     uri.substr(uri.size()-path.size())!=path||uri.find_first_of("\r\n@?#")!=std::string::npos)return false;
  auto authority=uri.substr(prefix.size(),uri.size()-prefix.size()-path.size());
  auto colon=authority.find(':');std::string host=authority.substr(0,colon);int port=443;
  if(colon!=std::string::npos&&!companion::ParseTlsPort(authority.substr(colon+1),port))return false;
  return companion::CanonicalPrivateIpv4(host)&&host==approved;
}

// Local trusted owner API only. No received packet can create this grant.
struct MuseRoomGrant {
  std::string uri,binding,generation;
  std::array<unsigned char,64> event{},control{};
  size_t event_size=0,control_size=0;
  uint64_t expires_ms=0;
  // Trusted caller retains an independently approved network lease.
  std::function<bool()> network_current;
};

class MuseRoomPeer {
  std::array<unsigned char,64> event_{},control_{};
  size_t event_size_=0,control_size_=0;
  std::string binding_,generation_;
  bool active_=false,challenged_=false,authenticated_=false;
  uint64_t last_=0,expires_=0;
  void Wipe(){for(volatile unsigned char &c:event_)c=0;for(volatile unsigned char &c:control_)c=0;event_size_=control_size_=0;}
  std::string Ready(const std::string& challenge) {
    if(challenge.size()<67||challenge.size()>1024||challenge[64]!='\n'||!Digest(challenge.substr(0,64)))return {};
    auto body=challenge.substr(65);if(body.find("\\u0000")!=std::string::npos)return {};
    auto tag=Mac(event_.data(),event_size_,"stackchan-muse-room-open-v1",body);
    if(tag.size()!=64)return {};
    unsigned different=0;for(size_t n=0;n<64;++n)different|=unsigned(tag[n]^challenge[n]);if(different)return {};
    const char* end=nullptr;cJSON* raw=cJSON_ParseWithLengthOpts(body.c_str(),body.size()+1,&end,true);
    if(!cJSON_IsObject(raw)){cJSON_Delete(raw);return {};}
    struct Owner{cJSON* p;~Owner(){cJSON_Delete(p);}} owner{raw};
    const char* fields[]={"version","provider","binding","generation","kind","nonce"};bool seen[6]{};unsigned count=0;
    for(auto item=raw->child;item;item=item->next){int index=-1;for(int n=0;n<6;++n)if(item->string&&!std::strcmp(item->string,fields[n]))index=n;if(index<0||seen[index])return {};seen[index]=true;++count;}
    if(count!=6)return {};
    auto version=cJSON_GetObjectItemCaseSensitive(raw,"version");if(!cJSON_IsNumber(version)||version->valuedouble!=1)return {};
    auto text=[&](const char* name){auto value=cJSON_GetObjectItemCaseSensitive(raw,name);return cJSON_IsString(value)&&value->valuestring?std::string(value->valuestring):std::string();};
    auto nonce=text("nonce");if(text("provider")!="muse"||text("binding")!=binding_||text("generation")!=generation_||text("kind")!="challenge"||nonce.size()!=32)return {};
    for(char c:nonce)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return {};
    auto kind=cJSON_GetObjectItemCaseSensitive(raw,"kind");if(!cJSON_SetValuestring(kind,"ready"))return {};
    char* encoded=cJSON_PrintUnformatted(raw);if(!encoded)return {};std::string reply=encoded;cJSON_free(encoded);
    auto proof=Mac(control_.data(),control_size_,"stackchan-muse-room-open-v1",reply);
    return proof.size()==64?proof+'\n'+reply:std::string();
  }
public:
  ~MuseRoomPeer(){Close();}
  MuseRoomPeer()=default;MuseRoomPeer(const MuseRoomPeer&)=delete;MuseRoomPeer& operator=(const MuseRoomPeer&)=delete;
  bool Open(const MuseRoomGrant& grant,uint64_t now) {
    if(active_||grant.event_size<32||grant.event_size>64||grant.control_size<32||grant.control_size>64||grant.expires_ms<=now)return false;
    if(!BindProvider(Provider::Muse,grant.binding,grant.generation,grant.event.data(),grant.event_size,grant.control.data(),grant.control_size,false))return false;
    event_=grant.event;control_=grant.control;event_size_=grant.event_size;control_size_=grant.control_size;
    binding_=grant.binding;generation_=grant.generation;expires_=grant.expires_ms;last_=now;active_=true;challenged_=authenticated_=false;return true;
  }
  bool Receive(const std::string& wire,bool binary,uint64_t now,std::string& reply) {
    reply.clear();if(!Tick(now)||binary||wire.size()>2048){Close();return false;}
    if(!challenged_){reply=Ready(wire);if(reply.empty()){Close();return false;}challenged_=true;last_=now;return true;}
    if(!AcceptProvider(Provider::Muse,wire,now)){Close();return false;}
    const auto state=ViewProvider(Provider::Muse,now);
    if(!state.verified||state.scene=="decision_sign"){Close();return false;}
    authenticated_=true;last_=now;reply="room-ack:"+std::to_string(state.sequence);return true;
  }
  bool Tick(uint64_t now) {
    if(!active_||!ProviderSelected(Provider::Muse)||now<last_||now>=expires_||now-last_>=2000){Close();return false;}
    return true;
  }
  bool Authenticated()const{return active_&&authenticated_;}
  void Close(){if(active_)DisconnectProvider(Provider::Muse);active_=challenged_=authenticated_=false;binding_.clear();generation_.clear();Wipe();}
};
}
