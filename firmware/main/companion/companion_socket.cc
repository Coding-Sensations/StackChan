#include "companion_socket.h"
#include "diagnostics.h"
#include "hal/board/hal_bridge.h"
#include <cJSON.h>
#include <esp_timer.h>
#include <cmath>
#include <set>
namespace companion {
static uint64_t ms(){return esp_timer_get_time()/1000;}
static bool bounded(const std::string& text,std::size_t max){if(text.empty() || text.size()>max)return false;for(auto c:text)if(c<32 || c==127)return false;return true;}
bool CompanionSocket::Text(const std::string& text){CameraMediaPriority::Critical priority(media_priority_);std::shared_ptr<BoundedWebSocket> ws;{std::lock_guard<std::mutex> lock(mutex_);ws=socket_;}return ws && ws->IsConnected() && ws->Send(text);}
bool CompanionSocket::SendAudio(const uint8_t* data,std::size_t bytes){
    CameraMediaPriority::Critical priority(media_priority_);
    WireFrame frame{};if(!decode_wire(data,bytes,frame) || frame.speaker || frame.epoch!=epoch_)return false;
    std::shared_ptr<BoundedWebSocket> ws;{std::lock_guard<std::mutex> lock(mutex_);ws=socket_;}return ws && ws->IsConnected() && ws->Send(data,bytes,true);
}
bool CompanionSocket::Connect(NetworkInterface& network,const std::string& uri,const std::string& device,const std::string& credential,const std::string& request){
    if(uri.rfind("wss://",0)!=0 || uri.size()>256 || uri.find_first_of("\r\n@?#")!=std::string::npos || uri.size()<22 || uri.substr(uri.size()-16)!="/companion/audio" || !bounded(device,100) || !bounded(credential,256) || !bounded(request,128))return false;
    if(connection_exhausted_.load())return false;
    Close();if(!tasks_.ResetStoppedConnection())return false;request_id_=request;{std::lock_guard<std::mutex> lock(mutex_);camera_device_=device;}consent_epoch_=0;auth_rejected_=false;
    const uint64_t callback_generation=connection_generation_.fetch_add(1)+1;
    if(!callback_generation){connection_exhausted_=true;return false;}
    std::shared_ptr<BoundedWebSocket> ws=std::make_shared<BoundedWebSocket>(&network,2);if(!ws)return false;
    ws->SetReceiveBufferSize(4112);ws->SetHeader("Authorization",("Bearer "+credential).c_str());ws->SetHeader("X-Companion-Device",device.c_str());
    ws->OnData([this,callback_generation](const char* data,size_t bytes,bool binary){Data(data,bytes,binary,callback_generation);});
    ws->OnDisconnected([this,callback_generation]{if(connection_generation_.load()!=callback_generation)return;RevokeCameraSession();transport_current_=false;camera_control_.Disconnect();state_=State::Disconnected;consent_epoch_=0;tasks_.Interrupt(epoch_);});
    ws->OnError([this,callback_generation](int){if(connection_generation_.load()!=callback_generation)return;RevokeCameraSession();transport_current_=false;camera_control_.Disconnect();state_=State::Fault;consent_epoch_=0;tasks_.Interrupt(epoch_);});
    {std::lock_guard<std::mutex> lock(mutex_);socket_=ws;}
    state_=State::AwaitingBegin;transport_current_=true;
    if(!ws->Connect(uri.c_str())){auto status=ws->HandshakeStatus();auth_rejected_=status==401||status==403;Close();return false;}
    cJSON* root=cJSON_CreateObject();cJSON_AddStringToObject(root,"type","begin");cJSON_AddStringToObject(root,"request_id",request.c_str());
    char* encoded=cJSON_PrintUnformatted(root);bool ok=encoded && Text(encoded);cJSON_free(encoded);cJSON_Delete(root);return ok;
}
void CompanionSocket::InterruptImmediately(){RevokeCameraSession();camera_control_.Disconnect();consent_epoch_=0;state_=State::Muted;tasks_.Interrupt(epoch_);}
bool CompanionSocket::PhysicalConsent(){return PhysicalConsent(epoch_.load());}
bool CompanionSocket::PhysicalConsent(uint32_t expected_epoch){
 if(!expected_epoch||epoch_.load()!=expected_epoch||state_!=State::ConsentRequired||(consent_guard_&&!consent_guard_()))return false;
 consent_deadline_ms_=ms()+5000;
 consent_epoch_=expected_epoch;state_=State::Arming;
 // Always send the captured epoch. A simultaneous new begin resets consent;
 // its armed ACK cannot borrow authorization from this old physical gesture.
 bool sent=Text("{\"type\":\"consent\",\"epoch\":"+std::to_string(expected_epoch)+",\"physical\":true}");
 if(!sent){uint32_t expected=expected_epoch;consent_epoch_.compare_exchange_strong(expected,0);}
 return sent;
}
void CompanionSocket::Tick(){
    auto held=std::atomic_load(&camera_confirmed_);
    if(held&&(ms()>=held->expires_ms||!transport_current_))RevokeCameraSession();
    PollCameraStop();if(!epoch_)return;if((state_==State::Listening || state_==State::Speaking) && !tasks_.IsActive()){state_=State::Muted;consent_epoch_=0;}auto now=ms();if(now-last_heartbeat_ms_>=500){last_heartbeat_ms_=now;diagnostic::Stage(diagnostic::Task::Main,diagnostic::StageCode::HeartbeatEnter);bool sent=Text("{\"type\":\"heartbeat\",\"epoch\":"+std::to_string(epoch_.load())+"}");diagnostic::CountHeartbeat(sent);if(!sent){diagnostic::Fault(diagnostic::Task::Main,diagnostic::FaultCode::HeartbeatWrite);tasks_.Interrupt(epoch_);}diagnostic::Stage(diagnostic::Task::Main,diagnostic::StageCode::HeartbeatExit);}}
void CompanionSocket::Close(){
    RevokeCameraSession();transport_current_=false;
    camera_control_.Disconnect();
    state_=State::Disconnected;consent_epoch_=0;tasks_.Interrupt(epoch_);epoch_=0;
    std::shared_ptr<BoundedWebSocket> ws;{std::lock_guard<std::mutex> lock(mutex_);ws=std::move(socket_);}if(ws)ws->Close();
}
void CompanionSocket::Data(const char* data,std::size_t bytes,bool binary,uint64_t callback_generation){
    if(!transport_current_.load()||callback_generation!=connection_generation_.load())return;
    if(binary&&data&&bytes>=4&&std::memcmp(data,"WCV1",4)==0){RevokeCameraSession();camera_control_.Disconnect();return;} // unauthorized inbound video is isolated from SCAV
    if(binary){if(!tasks_.Receive(reinterpret_cast<const uint8_t*>(data),bytes))tasks_.Interrupt(epoch_);return;}
    if(!data || !bytes || bytes>2048){tasks_.Interrupt(epoch_);return;}
    auto root=cJSON_ParseWithLength(data,bytes);if(!root || !cJSON_IsObject(root)){cJSON_Delete(root);tasks_.Interrupt(epoch_);return;}
    std::set<std::string> keys;bool duplicate=false;for(auto item=root->child;item;item=item->next){if(!item->string || !keys.insert(item->string).second)duplicate=true;}
    auto type=cJSON_GetObjectItemCaseSensitive(root,"type"),ep=cJSON_GetObjectItemCaseSensitive(root,"epoch");
    if(cJSON_IsString(type)&&std::string(type->valuestring).rfind("camera_",0)==0&&duplicate){cJSON_Delete(root);return;}
    if(!duplicate&&cJSON_IsString(type)&&(std::string(type->valuestring)=="camera_clock_probe"||std::string(type->valuestring)=="camera_stop_challenge"||std::string(type->valuestring)=="camera_stop_ack")){CameraProtocolData(root,callback_generation);cJSON_Delete(root);return;}
    if(!duplicate&&cJSON_IsString(type)&&(std::string(type->valuestring)=="camera_permission_proposal"||std::string(type->valuestring)=="camera_permission_cancel"||std::string(type->valuestring)=="camera_permission_commit")){CameraPermissionProtocol(root,callback_generation);cJSON_Delete(root);return;}
    if(bytes>1024){cJSON_Delete(root);return;}
    bool valid=!duplicate && cJSON_IsString(type) && cJSON_IsNumber(ep) && ep->valuedouble>=1 && ep->valuedouble<=UINT32_MAX && std::floor(ep->valuedouble)==ep->valuedouble;
    auto epoch=valid?static_cast<uint32_t>(ep->valuedouble):0;
    if(valid && (std::string(type->valuestring)=="camera_on"||std::string(type->valuestring)=="camera_off"||std::string(type->valuestring)=="camera_status")){
        CameraControlData(root,callback_generation);cJSON_Delete(root);return;
    }
    if(valid && std::string(type->valuestring)=="begin"){
        auto audio=cJSON_GetObjectItemCaseSensitive(root,"audio"),fixture=cJSON_GetObjectItemCaseSensitive(root,"fixture");
        auto number=[audio](const char* key,double expected){auto value=cJSON_GetObjectItemCaseSensitive(audio,key);return cJSON_IsNumber(value) && value->valuedouble==expected;};
        valid=cJSON_IsObject(audio) && number("uplink_rate",16000) && number("downlink_rate",24000) && number("channels",1) && number("frame_ms",60) && cJSON_IsFalse(fixture);
        if(valid){
            auto conversation=cJSON_GetObjectItemCaseSensitive(root,"conversation_id"),turn=cJSON_GetObjectItemCaseSensitive(root,"turn_id"),tg=cJSON_GetObjectItemCaseSensitive(root,"turn_generation");
            uint64_t generation=0;
            if(!cJSON_IsString(conversation)||!cJSON_IsString(turn)||!cJSON_IsString(tg)||!tg->valuestring)valid=false;
            else{const std::string digits=tg->valuestring;for(char ch:digits){if(ch<'0'||ch>'9'||generation>(UINT64_MAX-uint64_t(ch-'0'))/10){valid=false;break;}generation=generation*10+uint64_t(ch-'0');}
                valid=valid&&generation&&digits.size()<=20&&digits[0]!='0'&&CameraBridgeAscii(conversation->valuestring,128)&&CameraBridgeAscii(turn->valuestring,128);
                if(valid){std::lock_guard<std::mutex> lock(mutex_);camera_conversation_=conversation->valuestring;camera_turn_=turn->valuestring;camera_turn_generation_=generation;}}
        }
        if(valid)RevokeCameraSession();
        if(valid && tasks_.Begin(epoch) && tasks_.Acknowledge(epoch)){epoch_=epoch;camera_control_.Begin(callback_generation,epoch);state_=State::ConsentRequired;consent_epoch_=0;last_heartbeat_ms_=ms();valid=Text("{\"type\":\"ack\",\"epoch\":"+std::to_string(epoch)+"}");}else valid=false;
    }else if(valid && epoch==epoch_){
        auto kind=std::string(type->valuestring);
        if(kind=="armed"){valid=tasks_.Consent(epoch,true,[this,epoch]{return consent_epoch_.load()==epoch && epoch_.load()==epoch && ms()<consent_deadline_ms_ && (!consent_guard_||consent_guard_());});if(valid)state_=State::Listening;}
        else if(kind=="heartbeat")valid=tasks_.Heartbeat(epoch);
        else if(kind=="playing"){auto value=cJSON_GetObjectItemCaseSensitive(root,"value");valid=cJSON_IsBool(value) && tasks_.Playing(epoch,cJSON_IsTrue(value));if(valid)state_=cJSON_IsTrue(value)?State::Speaking:State::Listening;}
        else if(kind=="interrupt"){RevokeCameraSession();camera_control_.Disconnect();state_=State::Muted;consent_epoch_=0;tasks_.Interrupt(epoch);}
        else if(kind!="acknowledged")valid=false;
    }else valid=false;
    cJSON_Delete(root);if(!valid){camera_control_.Disconnect();state_=State::Fault;consent_epoch_=0;tasks_.Interrupt(epoch_);}
}
static bool camera_sequence(const cJSON* value,uint64_t& result){
    result=0;if(!cJSON_IsString(value)||!value->valuestring||value->valuestring[0]<'1'||value->valuestring[0]>'9')return false;
    const char* p=value->valuestring;size_t count=0;
    for(;*p;++p){if(++count>20||*p<'0'||*p>'9'||result>(UINT64_MAX-uint64_t(*p-'0'))/10)return false;result=result*10+uint64_t(*p-'0');}
    return result!=0;
}
bool CompanionSocket::CameraControlData(void* value,uint64_t callback_generation){
    auto root=static_cast<cJSON*>(value);auto kind=cJSON_GetObjectItemCaseSensitive(root,"type");
    auto ep=cJSON_GetObjectItemCaseSensitive(root,"epoch"),seq=cJSON_GetObjectItemCaseSensitive(root,"sequence");
    auto request=cJSON_GetObjectItemCaseSensitive(root,"request_id"),token=cJSON_GetObjectItemCaseSensitive(root,"token");
    uint64_t sequence=0;
    if(!camera_sequence(seq,sequence)||!cJSON_IsString(request)||!bounded(request->valuestring,32)||
       (token&&(!cJSON_IsString(token)||!bounded(token->valuestring,128))))return false;
    if(std::string(kind->valuestring)=="camera_status"){
        auto ready=cJSON_GetObjectItemCaseSensitive(root,"ready"),enabled=cJSON_GetObjectItemCaseSensitive(root,"enabled"),physical_off=cJSON_GetObjectItemCaseSensitive(root,"physical_off");
        auto status=cJSON_GetObjectItemCaseSensitive(root,"state"),reason=cJSON_GetObjectItemCaseSensitive(root,"reason");
        const bool safe=static_cast<uint32_t>(ep->valuedouble)==epoch_.load()&&cJSON_IsFalse(ready)&&cJSON_IsFalse(enabled)&&cJSON_IsFalse(physical_off)&&
            cJSON_IsString(status)&&(std::string(status->valuestring)=="unavailable"||std::string(status->valuestring)=="stopping")&&cJSON_IsString(reason)&&bounded(reason->valuestring,128);
        if(!safe)camera_control_.Disconnect(); // peer booleans never become camera/closure authority
        return safe;
    }
    for(auto item=root->child;item;item=item->next){const std::string key=item->string?item->string:"";if(key!="type"&&key!="epoch"&&key!="sequence"&&key!="request_id"&&key!="token")return false;}
    const uint32_t epoch=static_cast<uint32_t>(ep->valuedouble);
    const bool off=std::string(kind->valuestring)=="camera_off";
    auto result=off?camera_control_.Off(callback_generation,epoch,sequence):
        camera_control_.On(callback_generation,epoch,sequence,token?token->valuestring:"");
    if(!off&&result==CameraControlResult::CameraPurposeGrantMissing&&CameraPurposeCurrent(callback_generation,epoch))result=CameraControlResult::DriverUnqualified;
    if(off&&result==CameraControlResult::Stopping)RevokeCameraSession();
    if(!off&&camera_restart_.Pending())result=CameraControlResult::Stopping;
    cJSON* reply=cJSON_CreateObject();cJSON_AddStringToObject(reply,"type","camera_status");cJSON_AddNumberToObject(reply,"epoch",epoch);
    cJSON_AddStringToObject(reply,"sequence",seq->valuestring);cJSON_AddStringToObject(reply,"request_id",request->valuestring);
    cJSON_AddBoolToObject(reply,"ready",false);cJSON_AddBoolToObject(reply,"enabled",false);cJSON_AddBoolToObject(reply,"physical_off",false);
    cJSON_AddStringToObject(reply,"state",result==CameraControlResult::Stopping?"stopping":"unavailable");
    cJSON_AddStringToObject(reply,"reason",CameraControlReason(result));
    char* encoded=cJSON_PrintUnformatted(reply);const bool sent=encoded&&Text(encoded);cJSON_free(encoded);cJSON_Delete(reply);return sent;
}
bool CompanionSocket::PhysicalCameraOptIn(uint32_t expected_epoch){
    if(!expected_epoch||expected_epoch!=epoch_.load()||state_==State::Disconnected||state_==State::Fault)return false;
    return camera_control_.DeviceOptIn(connection_generation_.load(),expected_epoch);
}
bool CompanionSocket::SendCameraFragment(const uint8_t* data,size_t bytes,uint64_t connection,uint32_t epoch){
    auto purpose=std::atomic_load(&camera_confirmed_proposal_);
    if(!purpose||purpose->mode!="webcam")return false;
    if(!hal_bridge::board_camera_readiness().available()||!data||bytes<CameraWireHeader||bytes>CameraWireMax||std::memcmp(data,"WCV1",4)||
       connection!=connection_generation_.load()||epoch!=epoch_.load()||!CameraPurposeCurrent(connection,epoch))return false;
    std::unique_lock<std::mutex> priority(media_priority_.writer,std::try_to_lock);
    if(!priority.owns_lock()||!media_priority_.VideoAvailable())return false;
    std::shared_ptr<BoundedWebSocket> ws;{std::lock_guard<std::mutex> lock(mutex_);ws=socket_;}
    return ws&&ws->IsConnected()&&CameraPurposeCurrent(connection,epoch)&&ws->Send(data,bytes,true);
}
bool CompanionSocket::SendCameraFrame(desk::CameraBinding lease,CameraVideoIdentity identity,uint8_t* bgra,size_t capacity,uint8_t* fragment,size_t fragment_capacity){
    const uint64_t connection=connection_generation_.load();const uint32_t epoch=epoch_.load();
    auto wipe=[&](){if(bgra){volatile uint8_t* p=bgra;const size_t n=capacity>CameraBGRABytes?CameraBGRABytes:capacity;for(size_t i=0;i<n;++i)p[i]=0;}if(fragment){volatile uint8_t* p=fragment;const size_t n=fragment_capacity>CameraWireMax?CameraWireMax:fragment_capacity;for(size_t i=0;i<n;++i)p[i]=0;}};
    const auto purpose=std::atomic_load(&camera_confirmed_proposal_);
    if(!purpose||purpose->mode!="webcam"||capacity!=CameraBGRABytes||!bgra||!fragment||fragment_capacity!=CameraWireMax||!hal_bridge::board_camera_readiness().available()||!CameraFrameContextCurrent(connection,epoch,lease,identity)||!hal_bridge::board_camera_current(lease)){wipe();return false;}
    if(!hal_bridge::board_camera_capture(lease)){wipe();return false;}
    uint64_t sequence=0,captured_ns=0;
    if(!hal_bridge::board_camera_copy_bgra(lease,bgra,capacity,sequence,captured_ns)){wipe();return false;}
    size_t offset=0;uint64_t last_ns=captured_ns;
    while(offset<CameraBGRABytes){
        const uint64_t now_ns=uint64_t(esp_timer_get_time())*1000;
        if(now_ns<last_ns||captured_ns>now_ns||now_ns-captured_ns>=250000000){wipe();return false;}last_ns=now_ns;
        size_t count=0;const size_t n=EncodeCameraFragment(identity,sequence,captured_ns,bgra,capacity,offset,fragment,fragment_capacity,count);
        if(!n||!CameraFrameContextCurrent(connection,epoch,lease,identity)||!hal_bridge::board_camera_current(lease)||!SendCameraFragment(fragment,n,connection,epoch)){wipe();return false;}offset+=count;}
    wipe();return true;
}

}
