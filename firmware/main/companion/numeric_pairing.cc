#include "companion/local_trust_binding.hpp"
#include "numeric_pairing.h"
#include "diagnostics.h"
#include "enrollment_observation.hpp"
#include "bounded_web_socket.h"
extern "C" const char* companion_local_ca_pem();
extern "C" const char* companion_local_tls_host();
#include <cJSON.h>
#include <esp_random.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <set>
namespace companion {
namespace {
constexpr const char* device=STACKCHAN_PIN_DEVICE;
std::string fresh_nonce(){uint8_t bytes[16];esp_fill_random(bytes,sizeof(bytes));const char* hex="0123456789abcdef";std::string s;for(auto b:bytes){s.push_back(hex[b>>4]);s.push_back(hex[b&15]);}return s;}
void wipe(std::string& s){volatile char* p=s.empty()?nullptr:&s[0];for(size_t i=0;i<s.size();++i)p[i]=0;s.clear();}
void delete_json(cJSON* root){if(!root)return;auto credential=cJSON_GetObjectItemCaseSensitive(root,"credential");if(cJSON_IsString(credential)&&credential->valuestring){volatile char* p=credential->valuestring;size_t length=std::strlen(credential->valuestring);for(size_t i=0;i<length;++i)p[i]=0;}cJSON_Delete(root);}
bool keys(cJSON* root,std::initializer_list<const char*> expected){std::set<std::string> actual;for(auto item=root->child;item;item=item->next){if(!item->string||!actual.insert(item->string).second)return false;}std::set<std::string> wanted;for(auto key:expected)wanted.insert(key);return actual==wanted;}
std::string string(cJSON* root,const char* key){auto p=cJSON_GetObjectItemCaseSensitive(root,key);return cJSON_IsString(p)&&p->valuestring?p->valuestring:"";}
uint32_t number(cJSON* root,const char* key,uint32_t limit){auto p=cJSON_GetObjectItemCaseSensitive(root,key);if(!cJSON_IsNumber(p)||!std::isfinite(p->valuedouble)||p->valuedouble<1||p->valuedouble>limit)return 0;auto n=static_cast<uint32_t>(p->valuedouble);return p->valuedouble==n?n:0;}
}
NumericPairing::~NumericPairing(){Cancel();}
bool NumericPairing::Send(const std::string& text){return socket_&&socket_->IsConnected()&&socket_->Send(text);}
bool NumericPairing::StartWireless(NetworkInterface& network,uint32_t boot,uint64_t now){return Start(network,fresh_nonce(),boot,now);}
bool NumericPairing::StartRemembered(NetworkInterface& network,uint32_t boot,uint64_t now){return StartMode(network,fresh_nonce(),boot,now,true);}
bool NumericPairing::Start(NetworkInterface& network,const std::string& request,uint32_t boot,uint64_t now){return StartMode(network,request,boot,now,false);}
bool NumericPairing::StartMode(NetworkInterface& network,const std::string& request,uint32_t boot,uint64_t now,bool remember){
 Cancel();remembered_=remember;
 auto observe=[&](enrollment_observation::PairStage stage){if(remember)enrollment_observation::State().Pair(stage);};
 if(remember){if(!policy_||!save_){observe(enrollment_observation::PairStage::PolicyRejected);return false;}remember_policy_=policy_();PairRecord probe;bool valid=MakePairRecord(probe,remember_policy_,std::string(32,'x'));ClearPairRecord(probe);if(!valid){observe(enrollment_observation::PairStage::PolicyRejected);return false;}}
 const char* ca=companion_local_ca_pem();const char* host=companion_local_tls_host();
 if(!companion::LocalTrustBindingMatches()||!ca||!*ca||!host||std::strcmp(host,STACKCHAN_PIN_HOST)!=0||!context_.Start(request,fresh_nonce(),boot,now)){observe(enrollment_observation::PairStage::RequestRejected);return false;}
 if(remember)enrollment_observation::State().EnableTransport(true);
 fault_=false;started_=true;socket_=std::make_shared<BoundedWebSocket>(&network,3);
 socket_->SetReceiveBufferSize(2048);
 socket_->OnData([this](const char* bytes,size_t n,bool binary){
  if(binary||!bytes||!n||n>2048){fault_=true;return;}
  std::lock_guard<std::mutex> lock(queue_mutex_);if(queue_.size()>=2){fault_=true;return;}queue_.emplace_back(bytes,n);
 });
 socket_->OnDisconnected([this]{fault_=true;});socket_->OnError([this](int){fault_=true;});
 if(!socket_->Connect(STACKCHAN_PIN_PAIR_URI)){observe(enrollment_observation::PairStage::WebSocketFailed);Cancel();return false;}
 auto hello=std::string("{\"type\":\"")+(remembered_?"pair.remember.hello":"pair.hello")+"\",\"version\":1,\"request_id\":\""+context_.Request()+"\",\"device\":\""+device+"\",\"device_nonce\":\""+context_.DeviceNonce()+"\",\"boot_nonce\":"+std::to_string(boot)+",\"capabilities\":[\"microphone\",\"speaker\"]}";
 if(!Send(hello)){observe(enrollment_observation::PairStage::HelloWriteFailed);Cancel();return false;}observe(enrollment_observation::PairStage::HelloSent);return true;
}
bool NumericPairing::Message(const std::string& text,uint64_t now){
 if(text.find('\0')!=std::string::npos||text.find("\\u0000")!=std::string::npos)return false;
 bool quoted=false,escaped=false;unsigned depth=0;
 for(char c:text){if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='\"')quoted=false;}else if(c=='\"')quoted=true;else if(c=='[')return false;else if(c=='{'){if(++depth>1)return false;}else if(c=='}'){if(!depth)return false;--depth;}}
 if(quoted||depth)return false;
 std::unique_ptr<cJSON,decltype(&delete_json)> root(cJSON_ParseWithLengthOpts(text.c_str(),text.size()+1,nullptr,true),delete_json);
 if(!root||!cJSON_IsObject(root.get())||number(root.get(),"version",1)!=1||string(root.get(),"request_id")!=context_.Request()||string(root.get(),"device_nonce")!=context_.DeviceNonce())return false;
 auto type=string(root.get(),"type");
 if(remembered_){
  auto audio=cJSON_GetObjectItemCaseSensitive(root.get(),"audio_requested");
  if(!cJSON_IsFalse(audio))return false;
  if(type=="pair.remember.challenge"||type=="pair.remember.grant"){
   if(!policy_)return false;
   auto current=policy_();
   if(current.ca!=remember_policy_.ca||current.expiry!=remember_policy_.expiry||!current.now||current.now>=current.expiry||string(root.get(),"host_ca_sha256")!=current.ca||number(root.get(),"remember_until_unix",UINT32_MAX)!=current.expiry)return false;
  }
  if(type=="pair.remember.challenge"){
   if(!keys(root.get(),{"type","version","request_id","device_nonce","host_nonce","code","expires_ms","audio_requested","host_ca_sha256","remember_until_unix"}))return false;
   return context_.Challenge(string(root.get(),"host_nonce"),string(root.get(),"code"),number(root.get(),"expires_ms",60000),now,false);
  }
  if(type=="pair.remember.grant"){
   if(!keys(root.get(),{"type","version","request_id","device_nonce","host_nonce","device","uri","credential","ttl_seconds","audio_requested","host_ca_sha256","remember_until_unix"})||string(root.get(),"device")!=device||string(root.get(),"uri")!=PairUri)return false;
   auto secret=string(root.get(),"credential");PairRecord record;
   bool ok=MakePairRecord(record,policy_(),secret);wipe(secret);
   if(ok)ok=context_.Grant(string(root.get(),"request_id"),string(root.get(),"device_nonce"),string(root.get(),"host_nonce"),number(root.get(),"ttl_seconds",120),now,false);
   if(ok)ok=save_&&save_(record);
   ClearPairRecord(record);
   return ok&&Send("{\"type\":\"pair.remember.stored\",\"version\":1,\"request_id\":\""+context_.Request()+"\",\"device_nonce\":\""+context_.DeviceNonce()+"\",\"host_nonce\":\""+context_.HostNonce()+"\",\"stored\":true}");
  }
  if(type=="pair.remember.ready"){
   if(!keys(root.get(),{"type","version","request_id","device_nonce","host_nonce","ready","audio_requested"})||!context_.Granted()||remembered_ready_||string(root.get(),"host_nonce")!=context_.HostNonce()||!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root.get(),"ready")))return false;
   remembered_ready_=true;return true;
  }
  return false;
 }

 if(type=="pair.challenge"){
  if(!keys(root.get(),{"type","version","request_id","device_nonce","host_nonce","code","expires_ms","audio_requested"}))return false;
  auto audio=cJSON_GetObjectItemCaseSensitive(root.get(),"audio_requested");if(!cJSON_IsBool(audio))return false;
  return context_.Challenge(string(root.get(),"host_nonce"),string(root.get(),"code"),number(root.get(),"expires_ms",60000),now,cJSON_IsTrue(audio));
 }
 if(type=="pair.grant"){
  if(!keys(root.get(),{"type","version","request_id","device_nonce","host_nonce","device","uri","credential","ttl_seconds","audio_requested"})||string(root.get(),"device")!=device||string(root.get(),"uri")!=STACKCHAN_PIN_URI)return false;
  auto audio=cJSON_GetObjectItemCaseSensitive(root.get(),"audio_requested");if(!cJSON_IsBool(audio))return false;
  std::string credential=string(root.get(),"credential");bool valid=credential.size()>=32&&credential.size()<=256;
  for(unsigned char c:credential)if(c<33||c>126)valid=false;
  if(!valid||!context_.Grant(string(root.get(),"request_id"),string(root.get(),"device_nonce"),string(root.get(),"host_nonce"),number(root.get(),"ttl_seconds",120),now,cJSON_IsTrue(audio))){wipe(credential);return false;}
  grant_={std::move(credential),context_.Request(),context_.GrantDeadline(),context_.AudioRequested()};grant_ready_=true;return true;
 }
 if(type=="pair.ready"){
  if(!keys(root.get(),{"type","version","request_id","device_nonce","host_nonce","audio_requested","ready"})||!context_.Granted()||string(root.get(),"host_nonce")!=context_.HostNonce())return false;
  auto audio=cJSON_GetObjectItemCaseSensitive(root.get(),"audio_requested");auto ready=cJSON_GetObjectItemCaseSensitive(root.get(),"ready");
  if(!cJSON_IsBool(audio)||cJSON_IsTrue(audio)!=context_.AudioRequested()||!cJSON_IsTrue(ready))return false;
  host_ready_=true;return true;
 }
 return false; // Unknown control, cancellation and context changes fail closed.
}
void NumericPairing::Tick(uint64_t now){
 if(!started_)return;
 if(fault_.load()||!context_.Active(now)||context_.ChallengeExpired(now)){Cancel();return;}
 for(unsigned i=0;i<2;++i){std::string message;{std::lock_guard<std::mutex> lock(queue_mutex_);if(queue_.empty())break;message=std::move(queue_.front());queue_.pop_front();}bool ok=Message(message,now);wipe(message);if(!ok){Cancel();return;}}
}
bool NumericPairing::Confirm(uint64_t now){
 if(!Active(now))return false;
 if(context_.Confirmed())return true; // A queued duplicate tap cannot replay authorization.
 if(!context_.Confirm(now))return false;
 return Send(std::string("{\"type\":\"")+(remembered_?"pair.remember.confirm":"pair.confirm")+"\",\"version\":1,\"request_id\":\""+context_.Request()+"\",\"device_nonce\":\""+context_.DeviceNonce()+"\",\"host_nonce\":\""+context_.HostNonce()+"\",\"physical\":true"+(remembered_?",\"remember\":true}":"}"));
}
bool NumericPairing::Active(uint64_t now)const{return started_&&!fault_.load()&&context_.Active(now)&&!context_.ChallengeExpired(now);}
bool NumericPairing::TakeGrant(NumericGrant& out){if(!grant_ready_)return false;out=std::move(grant_);grant_ready_=false;return true;}
bool NumericPairing::Staged(){return Send("{\"type\":\"pair.staged\",\"version\":1,\"request_id\":\""+context_.Request()+"\",\"device_nonce\":\""+context_.DeviceNonce()+"\",\"host_nonce\":\""+context_.HostNonce()+"\",\"staged\":true}");}
void NumericPairing::Cancel(){
 enrollment_observation::State().EnableTransport(false); // retain last public result
 diagnostic::Stage(diagnostic::Task::Main,diagnostic::StageCode::NumericCloseEnter);
 started_=false;host_ready_=false;remembered_=remembered_ready_=false;remember_policy_={};fault_=true;auto socket=std::move(socket_);if(socket)socket->Close(); // Join before callback state is cleared.
 {std::lock_guard<std::mutex> lock(queue_mutex_);for(auto& item:queue_)wipe(item);queue_.clear();}
 wipe(grant_.credential);grant_={};grant_ready_=false;context_.Cancel();
 diagnostic::Sample(diagnostic::Task::Main);diagnostic::Stage(diagnostic::Task::Main,diagnostic::StageCode::NumericCloseExit);
}
}
