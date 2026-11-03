#include "companion/local_trust_binding.hpp"
#include "feature_link.h"
#include <cJSON.h>
#include <esp_random.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
extern "C" const char* companion_local_ca_pem();
extern "C" const char* companion_local_tls_host();
namespace companion {
namespace {
constexpr const char* Device=STACKCHAN_PIN_DEVICE;
void Delete(cJSON* p){if(p){auto c=cJSON_GetObjectItemCaseSensitive(p,"credential");if(cJSON_IsString(c)&&c->valuestring){volatile char* b=c->valuestring;const size_t length=std::strlen(c->valuestring);for(size_t n=0;n<length;++n)b[n]=0;}cJSON_Delete(p);}}
using Json=std::unique_ptr<cJSON,decltype(&Delete)>;
void Wipe(std::string& s){volatile char* p=s.empty()?nullptr:&s[0];for(size_t n=0;n<s.size();++n)p[n]=0;s.clear();}
std::string Nonce(){uint8_t b[16];esp_fill_random(b,sizeof(b));std::string s;const char* h="0123456789abcdef";for(auto n:b){s+=h[n>>4];s+=h[n&15];}return s;}
std::string Str(cJSON* p,const char* k){auto v=cJSON_GetObjectItemCaseSensitive(p,k);return cJSON_IsString(v)&&v->valuestring?v->valuestring:"";}
bool Keys(cJSON* p,std::initializer_list<const char*> names){if(!cJSON_IsObject(p))return false;
std::set<std::string> a,e;for(auto v=p->child;v;v=v->next){if(!v->string||!a.insert(v->string).second)return false;
}for(auto n:names)e.insert(n);return a==e;}
bool Shape(cJSON* p,unsigned depth=0){if(depth>6)return false;
if(cJSON_IsNumber(p)&&!std::isfinite(p->valuedouble))return false;
std::set<std::string> names;for(auto v=p->child;v;v=v->next){if(cJSON_IsObject(p)&&(!v->string||!names.insert(v->string).second))return false;
if(!Shape(v,depth+1))return false;
}return true;}
bool Depth(const std::string& s){unsigned depth=0;bool quote=false,escape=false;for(char c:s){if(quote){if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='\"')quote=false;}else if(c=='\"')quote=true;else if(c=='{'||c=='['){if(++depth>6)return false;}else if(c=='}'||c==']'){if(!depth)return false;--depth;}}return !quote&&!depth;}
Json Parse(const std::string& s){if(s.empty()||s.size()>2048||s.find('\0')!=std::string::npos||s.find("\\u0000")!=std::string::npos||!Depth(s))return Json(nullptr,Delete);Json p(cJSON_ParseWithLengthOpts(s.c_str(),s.size()+1,nullptr,true),Delete);if(!p||!Shape(p.get()))p.reset();return p;}
bool Integer(cJSON* p,const char* k,uint32_t& out){auto v=cJSON_GetObjectItemCaseSensitive(p,k);if(!cJSON_IsNumber(v)||!std::isfinite(v->valuedouble)||v->valuedouble<0||v->valuedouble>UINT32_MAX)return false;
out=static_cast<uint32_t>(v->valuedouble);return v->valuedouble==out;}
std::string Print(cJSON* p){char* s=cJSON_PrintUnformatted(p);if(!s)return {};std::string v=s;cJSON_free(s);return v;}
void Text(cJSON* p,const char* k,const std::string& v){cJSON_AddStringToObject(p,k,v.c_str());}
void AddScopes(cJSON* p,const char* key,const std::vector<std::string>& s){auto a=cJSON_AddArrayToObject(p,key);for(auto& v:s)cJSON_AddItemToArray(a,cJSON_CreateString(v.c_str()));}
bool SameScopes(cJSON* p,const char* key,const std::vector<std::string>& expected){auto a=cJSON_GetObjectItemCaseSensitive(p,key);if(!cJSON_IsArray(a)||cJSON_GetArraySize(a)!=static_cast<int>(expected.size()))return false;
int n=0;for(auto v=a->child;v;v=v->next){if(!cJSON_IsString(v)||!v->valuestring||expected[n++]!=v->valuestring)return false;
}return true;}
Json Base(const char* type,const NumericPairingContext& pair){Json p(cJSON_CreateObject(),Delete);if(p){Text(p.get(),"type",type);cJSON_AddNumberToObject(p.get(),"version",1);Text(p.get(),"request_id",pair.Request());Text(p.get(),"device_nonce",pair.DeviceNonce());}return p;}
bool Label(const std::string& s,size_t max){return !s.empty()&&s.size()<=max&&std::all_of(s.begin(),s.end(),[](unsigned char c){return c>=32&&c!=127;});}
bool Id(const std::string& s){return !s.empty()&&s.size()<=64&&std::all_of(s.begin(),s.end(),[](char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';});}
bool ResourceList(cJSON* root,const char* key,std::vector<FeatureResource>& output){auto a=cJSON_GetObjectItemCaseSensitive(root,key);if(!cJSON_IsArray(a)||cJSON_GetArraySize(a)>8)return false;
std::set<std::string> ids;for(auto v=a->child;v;v=v->next){if(!Keys(v,{"id","label"}))return false;
auto id=Str(v,"id"),label=Str(v,"label");if(!Id(id)||!Label(label,96)||!ids.insert(id).second)return false;
output.push_back({id,label});}return true;}
}
bool FeatureLink::Send(const std::string& s){return !s.empty()&&s.size()<=2048&&socket_&&socket_->IsConnected()&&socket_->Send(s);}
bool FeatureLink::StartPair(NetworkInterface& network,const std::string& request,uint32_t boot,const std::vector<std::string>& scopes,uint64_t now,bool audio){
 Close();const char* ca=companion_local_ca_pem();const char* host=companion_local_tls_host();
 if(!companion::LocalTrustBindingMatches()||(!audio&&std::any_of(scopes.begin(),scopes.end(),[](const std::string& c){return c.rfind("mira.",0)==0;}))||!FeatureScopes(scopes)||!ca||!*ca||!host||std::strcmp(host,STACKCHAN_PIN_HOST)||!pair_.Start(request,Nonce(),boot,now)){notice_="Trusted feature pairing unavailable";return false;
}
 network_=&network;scopes_=scopes;audio_selected_=audio;grant_scopes_=scopes;if(audio){grant_scopes_.push_back("microphone");grant_scopes_.push_back("speaker");std::sort(grant_scopes_.begin(),grant_scopes_.end());}fault_=false;socket_=std::make_shared<BoundedWebSocket>(&network,3);socket_->SetReceiveBufferSize(2048);
 socket_->OnData([this](const char* b,size_t n,bool binary){if(!b||!n||n>2048||binary){fault_=true;return;

}std::lock_guard<std::mutex> lock(mutex_);if(queue_.size()>=2){fault_=true;return;

}queue_.emplace_back(b,n);});
 socket_->OnDisconnected([this]{fault_=true;});socket_->OnError([this](int){fault_=true;});
 if(!socket_->Connect(STACKCHAN_PIN_PAIR_URI)){Close();notice_="Feature pairing connection refused";return false;
}
 auto p=Base("pair.hello",pair_);Text(p.get(),"device",Device);cJSON_AddNumberToObject(p.get(),"boot_nonce",boot);AddScopes(p.get(),"capabilities",{"microphone","speaker"});AddScopes(p.get(),"feature_capabilities",scopes_);AddScopes(p.get(),"grant_capabilities",grant_scopes_);
 if(!Send(Print(p.get()))){Close();notice_="Feature hello not sent";return false;
}notice_="Compare code and feature scopes";return true;
}
bool FeatureLink::Confirm(uint64_t now){if(feature_socket_||!pair_.Active(now)||pair_.Confirmed()||!pair_.Confirm(now))return false;
auto p=Base("pair.confirm",pair_);Text(p.get(),"host_nonce",pair_.HostNonce());cJSON_AddBoolToObject(p.get(),"physical",true);AddScopes(p.get(),"feature_capabilities",scopes_);AddScopes(p.get(),"grant_capabilities",grant_scopes_);if(!Send(Print(p.get()))){Close();notice_="Confirmation send uncertain";return false;
}notice_="Robot confirmed; confirm host scopes";return true;}
bool FeatureLink::OpenFeature(){
 // NumericGateway revokes when the pair connection closes. Retain it until session close.
 pair_socket_=std::move(socket_);
 socket_=std::make_shared<BoundedWebSocket>(network_,4);socket_->SetReceiveBufferSize(2048);socket_->SetHeader("Authorization",("Bearer "+credential_).c_str());socket_->SetHeader("X-Companion-Device",Device);Wipe(credential_);
 socket_->OnData([this](const char* b,size_t n,bool binary){if(!b||!n||n>2048||binary){fault_=true;return;

}std::lock_guard<std::mutex> lock(mutex_);if(queue_.size()>=2){fault_=true;return;

}queue_.emplace_back(b,n);});socket_->OnDisconnected([this]{fault_=true;});socket_->OnError([this](int){fault_=true;});
 if(!socket_->Connect(STACKCHAN_PIN_FEATURE_URI))return false;

 feature_socket_=true;Json p(cJSON_CreateObject(),Delete);Text(p.get(),"type","feature.open");cJSON_AddNumberToObject(p.get(),"version",1);Text(p.get(),"pair_request_id",pair_.Request());Text(p.get(),"device_nonce",pair_.DeviceNonce());Text(p.get(),"host_nonce",pair_.HostNonce());AddScopes(p.get(),"capabilities",scopes_);notice_="Opening exact feature session";return Send(Print(p.get()));
}
bool FeatureLink::Message(const std::string& s,uint64_t now){
 auto p=Parse(s);uint32_t version=0;if(!p||!Integer(p.get(),"version",version)||version!=1)return false;
auto type=Str(p.get(),"type");
 if(!feature_socket_){
  if(Str(p.get(),"request_id")!=pair_.Request()||Str(p.get(),"device_nonce")!=pair_.DeviceNonce()||(!SameScopes(p.get(),"feature_capabilities",scopes_)||!SameScopes(p.get(),"grant_capabilities",grant_scopes_)))return false;

  if(type=="pair.challenge"){
   if(!Keys(p.get(),{"type","version","request_id","device_nonce","host_nonce","code","expires_ms","audio_requested","feature_capabilities","grant_capabilities"})||(!cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(p.get(),"audio_requested"))||cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(p.get(),"audio_requested"))!=audio_selected_))return false;

   uint32_t ms=0;return Integer(p.get(),"expires_ms",ms)&&pair_.Challenge(Str(p.get(),"host_nonce"),Str(p.get(),"code"),ms,now,audio_selected_);
  }
  if(type=="pair.grant"){
   if(!Keys(p.get(),{"type","version","request_id","device_nonce","host_nonce","device","uri","credential","ttl_seconds","audio_requested","feature_capabilities","feature_uri","grant_capabilities"})||Str(p.get(),"device")!=Device||Str(p.get(),"uri")!=STACKCHAN_PIN_URI||Str(p.get(),"feature_uri")!=STACKCHAN_PIN_FEATURE_URI||(!cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(p.get(),"audio_requested"))||cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(p.get(),"audio_requested"))!=audio_selected_))return false;

   auto token=Str(p.get(),"credential");uint32_t ttl=0;bool valid=token.size()>=32&&token.size()<=256&&std::all_of(token.begin(),token.end(),[](unsigned char c){return c>=33&&c<=126;});
   if(!valid||!Integer(p.get(),"ttl_seconds",ttl)||!pair_.Grant(pair_.Request(),pair_.DeviceNonce(),Str(p.get(),"host_nonce"),ttl,now,audio_selected_)){Wipe(token);return false;
}credential_=std::move(token);if(audio_selected_)audio_credential_=credential_;granted_=true;
   auto staged=Base("pair.staged",pair_);Text(staged.get(),"host_nonce",pair_.HostNonce());cJSON_AddBoolToObject(staged.get(),"staged",true);AddScopes(staged.get(),"feature_capabilities",scopes_);AddScopes(staged.get(),"grant_capabilities",grant_scopes_);return Send(Print(staged.get()));
  }
  if(type=="pair.ready"){
   if(!Keys(p.get(),{"type","version","request_id","device_nonce","host_nonce","audio_requested","ready","feature_capabilities","grant_capabilities"})||!granted_||Str(p.get(),"host_nonce")!=pair_.HostNonce()||(!cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(p.get(),"audio_requested"))||cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(p.get(),"audio_requested"))!=audio_selected_)||!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(p.get(),"ready")))return false;
host_ready_=true;return OpenFeature();
  }return false;

 }
 if(Str(p.get(),"pair_request_id")!=pair_.Request()||Str(p.get(),"device_nonce")!=pair_.DeviceNonce()||Str(p.get(),"host_nonce")!=pair_.HostNonce())return false;

 if(type=="feature.ready"){
  if(ready_||!Keys(p.get(),{"type","version","pair_request_id","device_nonce","host_nonce","capabilities","feature_session_id","resources"})||!SameScopes(p.get(),"capabilities",scopes_))return false;

  auto resources=cJSON_GetObjectItemCaseSensitive(p.get(),"resources");if(!Keys(resources,{"volumes","wol_targets","mira"})||!ResourceList(resources,"volumes",volumes_)||!ResourceList(resources,"wol_targets",targets_))return false;

  auto mira=cJSON_GetObjectItemCaseSensitive(resources,"mira");if(!cJSON_IsNull(mira)){if(!Keys(mira,{"native_session_ref","epoch"})||!Integer(mira,"epoch",mira_epoch_)||!mira_epoch_)return false;
mira_ref_=Str(mira,"native_session_ref");if(!Label(mira_ref_,128))return false;
}
  ready_=requests_.Open(Str(p.get(),"feature_session_id"),pair_.GrantDeadline(),now);notice_=ready_?"Feature session ready":"Feature session refused";return ready_;
 }
 if(type=="feature.result"){
  if(!Keys(p.get(),{"type","version","request_id","feature_session_id","pair_request_id","device_nonce","host_nonce","sequence","capability","outcome","result"}))return false;

  uint32_t seq=0;if(!Integer(p.get(),"sequence",seq)||!requests_.Match(Str(p.get(),"feature_session_id"),Str(p.get(),"request_id"),Str(p.get(),"capability"),seq,now))return false;

  auto outcome=Str(p.get(),"outcome");auto value=cJSON_GetObjectItemCaseSensitive(p.get(),"result");if(outcome=="outcomeUnknown"){if(!cJSON_IsNull(value))return false;
requests_.Complete(false);result_="Outcome unknown. No retry. Check host.";notice_="Action outcome unknown";return true;}
  if(outcome!="complete"||!cJSON_IsObject(value))return false;
if(requests_.Capability()=="mira.forwarding.metadata"){
   if(!Keys(value,{"mira"}))return false;
   auto m=cJSON_GetObjectItemCaseSensitive(value,"mira");mira_ref_.clear();mira_epoch_=0;
   if(!cJSON_IsNull(m)){uint32_t epoch=0;if(!Keys(m,{"native_session_ref","epoch"})||!Integer(m,"epoch",epoch)||!epoch||!Label(Str(m,"native_session_ref"),128))return false;mira_ref_=Str(m,"native_session_ref");mira_epoch_=epoch;}
  }
  requests_.Complete(true);result_=Print(value);notice_="Host reply received";return true;
 }return false;

}
bool FeatureLink::Request(const std::string& cap,const std::string& arguments,uint64_t now){
 if(!Ready(now)||!std::binary_search(scopes_.begin(),scopes_.end(),cap))return false;
auto a=Parse(arguments);if(!a||!cJSON_IsObject(a.get()))return false;

 bool valid=false;
 if(cap=="desktop.health"){auto id=Str(a.get(),"volume_id");valid=Keys(a.get(),{"volume_id"})&&std::any_of(volumes_.begin(),volumes_.end(),[&](const FeatureResource& v){return v.id==id;});}
 else if(cap=="network.wol"){auto id=Str(a.get(),"target_id");valid=Keys(a.get(),{"target_id","intent"})&&Str(a.get(),"intent")=="send_once"&&std::any_of(targets_.begin(),targets_.end(),[&](const FeatureResource& v){return v.id==id;});}
 else if(cap=="mira.forwarding.metadata"||cap=="apple.now_playing"||cap=="apple.home.scenes")valid=Keys(a.get(),{});
 else if(cap=="apple.home.execute"){auto uuid=[](const std::string& v){if(v.size()!=36)return false;
for(size_t n=0;n<v.size();++n){char c=v[n];if(n==8||n==13||n==18||n==23){if(c!='-')return false;
}else if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;
}return true;};valid=Keys(a.get(),{"homeID","sceneID"})&&uuid(Str(a.get(),"homeID"))&&uuid(Str(a.get(),"sceneID"));}
 else if(cap=="mira.forwarding.status"||cap=="mira.forwarding.interrupt"){uint32_t epoch=0;valid=Keys(a.get(),{"session_reference","epoch"})&&!mira_ref_.empty()&&Str(a.get(),"session_reference")==mira_ref_&&Integer(a.get(),"epoch",epoch)&&epoch==mira_epoch_;}
 if(!valid)return false;
 if(cap=="mira.forwarding.metadata"){mira_ref_.clear();mira_epoch_=0;}

 const auto id=Nonce();if(!requests_.Begin(id,cap,now))return false;

 Json p(cJSON_CreateObject(),Delete);Text(p.get(),"type","feature.request");cJSON_AddNumberToObject(p.get(),"version",1);Text(p.get(),"request_id",id);Text(p.get(),"feature_session_id",requests_.Session());Text(p.get(),"pair_request_id",pair_.Request());Text(p.get(),"device_nonce",pair_.DeviceNonce());Text(p.get(),"host_nonce",pair_.HostNonce());cJSON_AddNumberToObject(p.get(),"sequence",requests_.Sequence());Text(p.get(),"capability",cap);cJSON_AddItemToObject(p.get(),"arguments",a.release());
 if(!Send(Print(p.get()))){requests_.Uncertain();notice_="Send outcome unknown; no retry";result_="Outcome unknown. Check host.";return false;
}result_.clear();notice_="Waiting for host result";return true;
}
void FeatureLink::Tick(uint64_t now){
 if(!socket_)return;

if(fault_||!pair_.Active(now)||pair_.ChallengeExpired(now)||!requests_.Tick(now)){bool uncertain=requests_.Pending()||requests_.Unknown();Close();notice_=uncertain?"Action outcome unknown; closed":"Feature connection expired or failed";if(uncertain)result_="Outcome unknown. No retry. Check host.";return;

}
 for(unsigned n=0;n<2;++n){std::string s;{std::lock_guard<std::mutex> lock(mutex_);if(queue_.empty())break;s=std::move(queue_.front());queue_.pop_front();}const bool ok=Message(s,now);Wipe(s);if(!ok){bool uncertain=requests_.Pending();Close();notice_=uncertain?"Reply refused; outcome unknown":"Feature protocol refused";if(uncertain)result_="Outcome unknown. No retry. Check host.";return;

}}
}
std::optional<FeatureAudioGrant> FeatureLink::TakeAudioGrant(uint64_t now){if(audio_taken_||audio_credential_.empty()||!AudioAuthorityCurrent(now))return std::nullopt;
 audio_taken_=true;return FeatureAudioGrant(std::move(audio_credential_),pair_.Request(),pair_.GrantDeadline(),this);}
void FeatureLink::Close(){ready_=granted_=feature_socket_=host_ready_=audio_selected_=audio_taken_=false;fault_=true;auto s=std::move(socket_);auto pair=std::move(pair_socket_);if(s)s->Close();if(pair)pair->Close();{std::lock_guard<std::mutex> lock(mutex_);for(auto& v:queue_)Wipe(v);queue_.clear();}Wipe(credential_);Wipe(audio_credential_);grant_scopes_.clear();pair_.Cancel();requests_.Close();network_=nullptr;scopes_.clear();volumes_.clear();targets_.clear();mira_ref_.clear();mira_epoch_=0;result_.clear();}
}
