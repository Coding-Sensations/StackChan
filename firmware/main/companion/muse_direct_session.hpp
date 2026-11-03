// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "direct_pairing_api.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
namespace companion::muse {
template<size_t N> class Secret {
    std::array<char,N+1> value_{};
    size_t size_=0;
public:
    Secret()=default;~Secret(){Clear();}
    Secret(const Secret&)=delete;Secret& operator=(const Secret&)=delete;
    bool Set(std::string_view text){Clear();if(text.empty()||text.size()>N)return false;for(unsigned char c:text)if(c<33||c>126)return false;std::memcpy(value_.data(),text.data(),text.size());size_=text.size();return true;}
    void Clear(){for(volatile char& c:value_)c=0;size_=0;}
    const char* Get()const{return value_.data();}
    size_t Size()const{return size_;}
};
inline void Wipe(std::string& text){for(volatile char& c:text)c=0;text.clear();}
inline bool Id(std::string_view value,size_t max=128){if(value.empty()||value.size()>max)return false;for(char c:value)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||c==':'))return false;return true;}
struct Json {
    cJSON* value=nullptr;
    explicit Json(cJSON* p=nullptr):value(p){}
    ~Json(){Clear(value);cJSON_Delete(value);}
    Json(const Json&)=delete;Json& operator=(const Json&)=delete;
    static void Clear(cJSON* p){for(auto* n=p;n;n=n->next){if(n->valuestring){volatile char* s=n->valuestring;const size_t length=std::strlen(n->valuestring);for(size_t i=0;i<length;++i)s[i]=0;}if(n->child)Clear(n->child);}}
};
inline std::string Text(cJSON* root,const char* name){auto* p=cJSON_GetObjectItemCaseSensitive(root,name);return cJSON_IsString(p)&&p->valuestring?std::string(p->valuestring):std::string();}
inline std::string Encode(cJSON* root){char* raw=cJSON_PrintUnformatted(root);if(!raw)return {};std::string result(raw);volatile char* p=raw;for(size_t i=0;i<result.size();++i)p[i]=0;cJSON_free(raw);return result;}
inline cJSON* Parse(std::string_view text){
    if(text.empty()||text.size()>8192||text.find('\0')!=std::string_view::npos||text.find("\\u0000")!=std::string_view::npos)return nullptr;
    // Keep untrusted JSON recursion within the small retained worker stacks.
    unsigned depth=0;bool quoted=false,escaped=false;
    for(char c:text){if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;}else if(c=='"')quoted=true;else if(c=='{'||c=='['){if(++depth>16)return nullptr;}else if(c=='}'||c==']'){if(!depth)return nullptr;--depth;}}
    if(depth||quoted)return nullptr;
    std::string copy(text);const char* end=nullptr;auto* value=cJSON_ParseWithLengthOpts(copy.c_str(),copy.size()+1,&end,true);Wipe(copy);return value;
}
inline bool Unique(cJSON* root){if(!cJSON_IsObject(root))return false;for(auto* a=root->child;a;a=a->next){if(!a->string)return false;for(auto* b=a->next;b;b=b->next)if(b->string&&!std::strcmp(a->string,b->string))return false;}return true;}
struct DeviceRecord {
    Secret<3072> access,refresh;
    std::string node;
    void Clear(){access.Clear();refresh.Clear();node.clear();}
};
enum class StoreResult { NoWrite, Committed, Uncertain };
class CredentialStore {
public:
    virtual ~CredentialStore()=default;
    virtual StoreResult CommitNew(const DeviceRecord&)=0;
};
class DeviceVerifier {
public:
    virtual ~DeviceVerifier()=default;
    // Fixed official origin only; no URL from the phone is a dial target.
    virtual bool Verify(const DeviceRecord&)=0;
};
enum class PairState { Closed, Phone, Physical, Verifying, Stored, Failed };
// Serialized protocol owner, driven by one retained worker. BLE callbacks only
// queue input/revoke the live lease. No radio, NVS init, WiFi, token lookup or
// capture is performed by construction. Live authorization also fences stale
// transport epochs and pending/replacement input during verification.
class PairingSession {
    const stackchan_muse_direct_pairing_api_t* api_;
    CredentialStore& store_;DeviceVerifier& verifier_;
    std::function<bool()> current_;
    std::function<bool(const std::string&)> send_;
    std::function<uint64_t()> clock_;
    Secret<48> token_;
    DeviceRecord record_;
    std::string node_,device_,mac_,version_;
    PairState state_=PairState::Closed;
    uint64_t last_=0,deadline_=0,physical_deadline_=0;
    uint32_t physical_=0,commit_generation_=0;
    StoreResult stored_=StoreResult::NoWrite;
    static std::atomic<PairingSession*>& Active(){static std::atomic<PairingSession*> owner{nullptr};return owner;}
    static PairingSession*& CommitOwner(){static PairingSession* owner=nullptr;return owner;}
    static bool Commit(){auto* p=CommitOwner();if(!p||!p->Current())return false;p->stored_=p->store_.CommitNew(p->record_);return p->stored_==StoreResult::Committed;}
    bool Current(){const auto now=clock_?clock_():0;if((state_==PairState::Closed||state_==PairState::Failed)||!current_||!current_()||now<last_||now>=deadline_)return false;last_=now;return true;}
    bool Send(std::string value){if(value.empty()||!Current()){Wipe(value);return false;}const bool sent=send_&&send_(value);Wipe(value);return sent;}
    bool Status(const char* status,uint32_t generation=0){uint32_t record=0;char* raw=api_->encrypted_status(status,generation,&record);if(!raw)return false;std::string value(raw);volatile char* p=raw;for(size_t n=0;n<value.size();++n)p[n]=0;cJSON_free(raw);return Send(std::move(value));}
    bool ReceiveObject(cJSON* root,bool encrypted){
        if(!Unique(root))return false;
        auto action=Text(root,"action");
        if(!encrypted&&action=="get_device_info"){
            Json info(cJSON_CreateObject());cJSON_AddStringToObject(info.value,"type","device_info");cJSON_AddStringToObject(info.value,"node_id",node_.c_str());cJSON_AddStringToObject(info.value,"version",version_.c_str());cJSON_AddBoolToObject(info.value,"network_ready",true);api_->device_info(info.value);return Send(Encode(info.value));
        }
        if(!encrypted&&action=="pairing_client_hello"){
            physical_=0;state_=PairState::Phone;char* ready=nullptr;const char* error=api_->hello(root,&ready);
            if(error||!ready)return false;
            std::string value(ready);cJSON_free(ready);return Send(std::move(value));
        }
        if(!encrypted&&action=="pairing_encrypted"){
            char* raw=nullptr;if(api_->decrypt(root,&raw)||!raw)return false;
            std::string value(raw);volatile char* p=raw;for(size_t i=0;i<value.size();++i)p[i]=0;cJSON_free(raw);
            Json command(Parse(value));Wipe(value);return ReceiveObject(command.value,true);
        }
        if(!encrypted)return false;
        if(action=="pairing_client_finished"){
            if(cJSON_GetArraySize(root)!=1)return false;
            const auto generation=api_->finished();if(!generation||!Status("pairing_confirm_required",generation)||!api_->arm_press(generation))return false;
            physical_=generation;physical_deadline_=clock_()+60000;state_=PairState::Physical;return true;
        }
        if(action=="wifi_scan"){
            if(physical_||state_!=PairState::Phone)return false;
            Json response(cJSON_CreateObject());cJSON_AddStringToObject(response.value,"type","wifi_scan_result");auto* list=cJSON_AddArrayToObject(response.value,"networks");auto* entry=cJSON_CreateObject();cJSON_AddStringToObject(entry,"ssid","Use current connection");cJSON_AddNumberToObject(entry,"rssi",-40);cJSON_AddBoolToObject(entry,"secure",false);cJSON_AddItemToArray(list,entry);
            uint32_t record=0;auto value=Encode(response.value);char* raw=api_->encrypted_json(value.c_str(),0,&record);Wipe(value);if(!raw)return false;std::string wire(raw);cJSON_free(raw);return Send(std::move(wire));
        }
        if(action!="provision_v2"||physical_||state_!=PairState::Phone)return false;
        if(Text(root,"token_type")!="device"||!Text(root,"ota_url").empty()||cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root,"ota_force")))return false;
        // WiFi fields are never applied or stored. This first direct path
        // requires the existing independently approved connected network.
        auto api=Text(root,"api_url_v2"),host=Text(root,"noise_host");
        if((!api.empty()&&api!="https://api.muse.ai"&&api!="https://api.muse.ai/")||(!host.empty()&&host!="hatch.metaaivm.com"))return false;
        auto access=Text(root,"access_token"),refresh=Text(root,"refresh_token");
        const bool valid=record_.access.Set(access)&&record_.refresh.Set(refresh);Wipe(access);Wipe(refresh);if(!valid)return false;
        record_.node=node_;commit_generation_=api_->provisioning();if(!commit_generation_)return false;
        state_=PairState::Verifying;stored_=StoreResult::NoWrite;
        if(!Status("wifi_connected",commit_generation_)||!verifier_.Verify(record_)||!Current()){record_.Clear();return false;}
        CommitOwner()=this;const bool committed=api_->commit(commit_generation_,Commit);CommitOwner()=nullptr;record_.Clear();
        if(!committed)return false;
        state_=PairState::Stored;return Status("auth_ok",commit_generation_);
    }
public:
    PairingSession(CredentialStore& store,DeviceVerifier& verifier,const stackchan_muse_direct_pairing_api_t* api=stackchan_muse_direct_pairing()):api_(api),store_(store),verifier_(verifier){}
    ~PairingSession(){Close();}
    bool Open(std::string node,std::string device,std::string mac,std::string version,std::string_view token,uint64_t deadline,
        std::function<uint64_t()> clock,std::function<bool()> current,std::function<bool(const std::string&)> send){
        if(state_!=PairState::Closed||!clock||!current||!current()||!send||!Id(node)||!Id(device)||!Id(mac,17)||!Id(version,32)||token.size()!=48||token.substr(0,5)!="mgst_")return false;
        for(char c:token.substr(5))if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'))return false;
        if(std::string_view("AEIMQUYcgkosw048").find(token.back())==std::string_view::npos)return false;
        const auto now=clock();if(deadline<=now||deadline-now>600000||!token_.Set(token))return false;
        PairingSession* empty=nullptr;if(!Active().compare_exchange_strong(empty,this)){token_.Clear();return false;}
        node_=std::move(node);device_=std::move(device);mac_=std::move(mac);version_=std::move(version);clock_=std::move(clock);current_=std::move(current);send_=std::move(send);last_=now;deadline_=deadline;
        api_->init(node_.c_str(),device_.c_str(),mac_.c_str(),version_.c_str(),token_.Get());state_=PairState::Phone;return true;
    }
    bool Receive(const std::string& message){
        if(!Current()||message.empty()||message.size()>8192){Close();return false;}
        Json root(Parse(message));const bool okay=ReceiveObject(root.value,false);if(!okay){state_=PairState::Failed;api_->reset();physical_=0;record_.Clear();}return okay;
    }
    uint32_t PhysicalTicket(){return Current()&&clock_()<physical_deadline_?physical_:0;}
    bool PhysicalPress(uint32_t captured){
        if(!Current()||clock_()>=physical_deadline_||state_!=PairState::Physical||!captured||captured!=physical_)return false;
        const auto generation=api_->physical_press();physical_=0;if(!generation)return false;state_=PairState::Phone;return Status("pairing_confirmed",generation);
    }
    bool Tick(){if(Current()&&(state_!=PairState::Physical||clock_()<physical_deadline_))return true;Close();return false;}
    void Close(){if(Active().load()==this){api_->shutdown();Active()=nullptr;}state_=PairState::Closed;physical_=commit_generation_=0;record_.Clear();token_.Clear();node_.clear();device_.clear();mac_.clear();version_.clear();current_={};send_={};clock_={};}
    PairState State()const{return state_;}
    StoreResult Stored()const{return stored_;}
};
}
