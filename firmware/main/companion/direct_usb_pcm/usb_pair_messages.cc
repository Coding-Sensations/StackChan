#include "companion/local_trust_binding.hpp"
#include "usb_pair_messages.h"
#include <cJSON.h>
#include <cmath>
#include <set>
#include <cstring>
namespace direct_usb_pcm {
namespace {
using Json=std::unique_ptr<cJSON,decltype(&cJSON_Delete)>;
Json Parse(const std::string& text){
 if(text.empty()||text.size()>2048||text.find('\0')!=std::string::npos)return {nullptr,cJSON_Delete};
 // Fixed schemas are flat scalar objects. Reject nested/array material before
 // invoking the SDK parser; decoded NUL is refused including escaped forms.
 bool quoted=false,escape=false;unsigned depth=0;
 for(size_t i=0;i<text.size();++i){char c=text[i];if(quoted){if(escape){if(c=='u'&&text.compare(i,5,"u0000")==0)return {nullptr,cJSON_Delete};escape=false;}else if(c=='\\')escape=true;else if(c=='"')quoted=false;}else if(c=='"')quoted=true;else if(c=='[')return {nullptr,cJSON_Delete};else if(c=='{'){if(++depth>1)return {nullptr,cJSON_Delete};}else if(c=='}'){if(!depth)return {nullptr,cJSON_Delete};--depth;}}
 if(quoted||escape||depth)return {nullptr,cJSON_Delete};
 Json j(cJSON_ParseWithLengthOpts(text.c_str(),text.size()+1,nullptr,true),cJSON_Delete);if(!j||!cJSON_IsObject(j.get()))return {nullptr,cJSON_Delete};return j;
}
bool Keys(cJSON* r,std::initializer_list<const char*> keys){std::set<std::string> actual,wanted;for(auto p=r->child;p;p=p->next)if(!p->string||!actual.insert(p->string).second)return false;for(auto p:keys)wanted.insert(p);return actual==wanted;}
std::string Str(cJSON* r,const char* k){auto p=cJSON_GetObjectItemCaseSensitive(r,k);return cJSON_IsString(p)&&p->valuestring?p->valuestring:"";}
uint32_t Num(cJSON* r,const char* k,uint32_t max){auto p=cJSON_GetObjectItemCaseSensitive(r,k);if(!cJSON_IsNumber(p)||!std::isfinite(p->valuedouble)||p->valuedouble<1||p->valuedouble>max)return 0;auto n=uint32_t(p->valuedouble);return n==p->valuedouble?n:0;}
bool Match(cJSON* r,const Identity& i,const companion::NumericPairingContext& pair,uint64_t now){
 return companion::PersonalBindingAvailable()&&pair.Active(now)&&i.device==STACKCHAN_PIN_DEVICE&&Num(r,"version",1)==1&&Str(r,"transport")=="usb-tls-pcm-v1"&&Str(r,"device")==i.device&&Str(r,"request_id")==i.request&&Str(r,"device_nonce")==i.device_nonce&&Str(r,"host_nonce")==i.host_nonce&&Num(r,"boot_nonce",UINT32_MAX)==i.boot&&Num(r,"epoch",UINT32_MAX)==i.epoch&&Num(r,"stream",65535)==i.stream&&i.request==pair.Request()&&i.device_nonce==pair.DeviceNonce()&&i.host_nonce==pair.HostNonce()&&i.boot==pair.Boot()&&now<i.deadline_ms;
}
}
bool ApplyUsbChallenge(const std::string& text,companion::NumericPairingContext& pair,uint32_t epoch,uint16_t stream,uint64_t now){
 auto j=Parse(text);if(!companion::PersonalBindingAvailable()||!j||!Keys(j.get(),{"type","version","request_id","device","device_nonce","host_nonce","code","audio_requested","expires_ms","transport","boot_nonce","epoch","stream","active_limit_seconds"})||Str(j.get(),"type")!="pair.challenge"||Num(j.get(),"version",1)!=1||Str(j.get(),"transport")!="usb-tls-pcm-v1"||Str(j.get(),"device")!=STACKCHAN_PIN_DEVICE||Str(j.get(),"request_id")!=pair.Request()||Str(j.get(),"device_nonce")!=pair.DeviceNonce()||Num(j.get(),"boot_nonce",UINT32_MAX)!=pair.Boot()||Num(j.get(),"epoch",UINT32_MAX)!=epoch||Num(j.get(),"stream",65535)!=stream||Num(j.get(),"active_limit_seconds",60)!=60||!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j.get(),"audio_requested")))return false;
 return pair.Challenge(Str(j.get(),"host_nonce"),Str(j.get(),"code"),Num(j.get(),"expires_ms",60000),now,true);
}
std::string UsbHello(const companion::NumericPairingContext& pair){
 if(!companion::PairHex(pair.Request())||!companion::PairHex(pair.DeviceNonce())||!pair.Boot())return {};
 return "{\"type\":\"pair.hello\",\"version\":1,\"request_id\":\""+pair.Request()+"\",\"device\":\""+std::string(STACKCHAN_PIN_DEVICE)+"\",\"device_nonce\":\""+pair.DeviceNonce()+"\",\"boot_nonce\":"+std::to_string(pair.Boot())+",\"capabilities\":[\"microphone\",\"speaker\"]}";
}
std::string UsbConfirm(const companion::NumericPairingContext& pair){
 if(!pair.Confirmed()||!companion::PairHex(pair.HostNonce()))return {};
 return "{\"type\":\"pair.confirm\",\"version\":1,\"request_id\":\""+pair.Request()+"\",\"device_nonce\":\""+pair.DeviceNonce()+"\",\"host_nonce\":\""+pair.HostNonce()+"\",\"physical\":true}";
}
bool ApplyUsbGrant(const std::string& text,companion::NumericPairingContext& pair,const Identity& i,uint64_t now){
 auto j=Parse(text);if(!j||!Keys(j.get(),{"type","version","transport","request_id","device","device_nonce","host_nonce","boot_nonce","epoch","stream","ttl_seconds","active_limit_seconds"})||Str(j.get(),"type")!="pair.usb-grant"||!Match(j.get(),i,pair,now)||Num(j.get(),"active_limit_seconds",60)!=60||!pair.AudioRequested())return false;
 return pair.Grant(i.request,i.device_nonce,i.host_nonce,Num(j.get(),"ttl_seconds",120),now,true);
}
bool UsbHeartbeat(const std::string& text,const companion::NumericPairingContext& pair,const Identity& i,uint64_t now){
 auto j=Parse(text);return j&&Keys(j.get(),{"type","version","transport","request_id","device","device_nonce","host_nonce","boot_nonce","epoch","stream"})&&Str(j.get(),"type")=="pair.usb-heartbeat"&&Match(j.get(),i,pair,now)&&pair.Granted()&&pair.Confirmed();
}
bool UsbReady(const std::string& text,const companion::NumericPairingContext& pair,const Identity& i,uint64_t now){
 auto j=Parse(text);if(!j||!Keys(j.get(),{"type","version","transport","request_id","device","device_nonce","host_nonce","boot_nonce","epoch","stream","ready"})||Str(j.get(),"type")!="pair.usb-ready"||!Match(j.get(),i,pair,now)||!pair.Granted()||!pair.Confirmed())return false;return cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j.get(),"ready"));
}
std::string UsbStaged(const Identity& i){
 // Fields originate from validated constant/hex/numeric local context only.
 if(!companion::PersonalBindingAvailable()||i.device!=STACKCHAN_PIN_DEVICE||!companion::PairHex(i.request)||!companion::PairHex(i.device_nonce)||!companion::PairHex(i.host_nonce)||!i.boot||!i.epoch||!i.stream)return {};
 return "{\"type\":\"pair.usb-staged\",\"version\":1,\"transport\":\"usb-tls-pcm-v1\",\"device\":\""+i.device+"\",\"request_id\":\""+i.request+"\",\"device_nonce\":\""+i.device_nonce+"\",\"host_nonce\":\""+i.host_nonce+"\",\"boot_nonce\":"+std::to_string(i.boot)+",\"epoch\":"+std::to_string(i.epoch)+",\"stream\":"+std::to_string(i.stream)+",\"staged\":true}";
}
}
