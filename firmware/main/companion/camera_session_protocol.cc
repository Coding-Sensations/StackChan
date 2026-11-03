#include "camera_authority_ledger.hpp" // Source implementation compiled, runtime attachment remains explicit.
#include "companion_socket.h"
#include "hal/board/hal_bridge.h"
#include <cJSON.h>
#include <esp_timer.h>
#include "camera_boot_context.hpp"
#include <set>
namespace companion {
namespace {
uint64_t now_ms(){const auto n=esp_timer_get_time();return n>0?uint64_t(n)/1000:0;}
bool number(const cJSON* r,const char* k,uint64_t& out){
    out=0;const auto v=cJSON_GetObjectItemCaseSensitive(r,k);
    if(!cJSON_IsString(v)||!v->valuestring)return false;
    const std::string s=v->valuestring;if(s.empty()||s.size()>20||s[0]<'1'||s[0]>'9')return false;
    for(char c:s){if(c<'0'||c>'9'||out>(UINT64_MAX-uint64_t(c-'0'))/10)return false;out=out*10+uint64_t(c-'0');}return out!=0;
}
std::string str(const cJSON* r,const char* k){auto v=cJSON_GetObjectItemCaseSensitive(r,k);return cJSON_IsString(v)&&v->valuestring?v->valuestring:"";}
bool keys(const cJSON* r,const std::set<std::string>& expected){
    std::set<std::string> found;for(auto v=r->child;v;v=v->next)if(!v->string||!found.insert(v->string).second)return false;
    auto ver=cJSON_GetObjectItemCaseSensitive(r,"version");return found==expected&&cJSON_IsNumber(ver)&&ver->valuedouble==1;
}
void dec(cJSON* r,const char* k,uint64_t v){cJSON_AddStringToObject(r,k,std::to_string(v).c_str());}
void stop_tuple(cJSON* r,const CameraStopChallenge& c){
    cJSON_AddNumberToObject(r,"version",1);cJSON_AddStringToObject(r,"device",c.owner.device.c_str());
    cJSON_AddStringToObject(r,"conversation",c.owner.conversation.c_str());cJSON_AddStringToObject(r,"turn",c.owner.turn.c_str());
    dec(r,"generation",c.owner.binding.generation);dec(r,"epoch",c.owner.binding.epoch);
    cJSON_AddStringToObject(r,"boot",c.owner.boot.c_str());dec(r,"authority_id",c.owner.authority_id);
    cJSON_AddStringToObject(r,"challenge",c.nonce.c_str());dec(r,"stop_sequence",c.sequence);
}
bool stop_matches(const cJSON* r,const CameraAuthorityContext& c,CameraStopChallenge& out){
    uint64_t g=0,e=0,a=0,q=0;
    if(!number(r,"generation",g)||!number(r,"epoch",e)||!number(r,"authority_id",a)||!number(r,"stop_sequence",q)||
       c.device!=str(r,"device")||c.conversation!=str(r,"conversation")||c.turn!=str(r,"turn")||c.boot!=str(r,"boot")||
       c.binding.generation!=g||c.binding.epoch!=e||c.authority_id!=a||!CameraBridgeHex(str(r,"challenge")))return false;
    out={c,str(r,"challenge"),q};return true;
}
}
void CompanionSocket::RevokeCameraSession(){
    auto revision=camera_permission_revision_.load();
    while(revision!=UINT64_MAX&&!camera_permission_revision_.compare_exchange_weak(revision,revision+1)){}
    camera_local_permission_.Invalidate();camera_purpose_revoked_=true;
    if(camera_purpose_dispatcher_)camera_purpose_dispatcher_->RevokeFromTrustedOwner();
    // A real reservation may exist before host issuer consumption/commit.
    // Its exact board owner must be revoked even before ledger installation.
    auto reserved=std::atomic_load(&camera_confirmed_);
    if(reserved)hal_bridge::board_camera_revoke(reserved->binding);
    std::shared_ptr<TrustedCameraSessionDispatcher> d;std::string boot;
    {std::lock_guard<std::mutex> l(camera_protocol_mutex_);d=camera_dispatcher_;boot=camera_boot_;}
    const auto connection=connection_generation_.load();const auto epoch=epoch_.load();CameraAuthorityContext c;
    if(d&&epoch&&d->RetainedSnapshot(connection,epoch,c)&&c.boot==boot&&d->RetainedStopCurrent(connection,epoch,c))
        hal_bridge::board_camera_revoke(c.binding);
}
bool CompanionSocket::CameraPurposeCurrent(uint64_t connection,uint32_t epoch){
    if(camera_restart_.Pending()||!camera_control_.DeviceOptInCurrent(connection,epoch))return false;
    std::shared_ptr<TrustedCameraSessionDispatcher> d;std::string boot;
    {std::lock_guard<std::mutex> l(camera_protocol_mutex_);d=camera_dispatcher_;boot=camera_boot_;}
    CameraAuthorityContext c;const auto now=now_ms();
    return d&&transport_current_&&connection==connection_generation_&&epoch==epoch_&&d->Snapshot(connection,epoch,now,c)&&
        CameraBridgeContextValid(c,now)&&c.boot==boot&&d->Current(connection,epoch,c,now);
}
bool CompanionSocket::CameraFrameContextCurrent(uint64_t connection,uint32_t epoch,desk::CameraBinding lease,CameraVideoIdentity id){
    if(!CameraPurposeCurrent(connection,epoch)||!CameraIdentifierLength(id.device,64)||!CameraIdentifierLength(id.conversation,128)||!CameraIdentifierLength(id.turn,128))return false;
    std::shared_ptr<TrustedCameraSessionDispatcher> d;
    {std::lock_guard<std::mutex> l(camera_protocol_mutex_);d=camera_dispatcher_;}
    CameraAuthorityContext c;const auto now=now_ms();
    return d&&d->Snapshot(connection,epoch,now,c)&&c.binding==lease&&c.binding.generation==id.generation&&c.binding.epoch==id.epoch&&
        c.device==id.device&&c.conversation==id.conversation&&c.turn==id.turn&&d->Current(connection,epoch,c,now);
}
void CompanionSocket::SetTrustedCameraDispatcher(std::shared_ptr<TrustedCameraSessionDispatcher> d){
    std::lock_guard<std::mutex> l(camera_protocol_mutex_);
    // Replacement cannot erase retained cleanup custody.
    if(camera_dispatcher_||camera_restart_.Pending()||camera_stop_worker_.joinable())return;
    if(camera_boot_.empty())camera_boot_=desk::CameraProducerBootHex();
    camera_dispatcher_=std::move(d);
}
std::string CompanionSocket::CameraProducerBoot() const {std::lock_guard<std::mutex> l(camera_protocol_mutex_);return camera_boot_;}
bool CompanionSocket::CameraText(const std::string& wire,uint64_t generation,uint32_t audio_epoch){
    CameraMediaPriority::Critical priority(media_priority_);
    std::shared_ptr<BoundedWebSocket> ws;
    {std::lock_guard<std::mutex> l(mutex_);if(!transport_current_||generation!=connection_generation_||audio_epoch!=epoch_)return false;ws=socket_;}
    return wire.size()<=2048&&ws&&ws->IsConnected()&&ws->Send(wire);
}
bool CompanionSocket::CameraProtocolData(void* value,uint64_t generation){
    const auto r=static_cast<cJSON*>(value);const auto type=str(r,"type");
    std::shared_ptr<TrustedCameraSessionDispatcher> dispatcher;std::string boot;
    {std::lock_guard<std::mutex> l(camera_protocol_mutex_);dispatcher=camera_dispatcher_;boot=camera_boot_;}
    const uint32_t audio_epoch=epoch_.load();
    if(!dispatcher||!transport_current_||generation!=connection_generation_||!audio_epoch)return false;
    if(type=="camera_clock_probe"){
        const uint64_t receive=now_ms();CameraAuthorityContext c;uint64_t tg=0,g=0,e=0;
        if(camera_restart_.Pending()||!keys(r,{"type","version","device_id","conversation_id","turn_id","turn_generation","generation","epoch","purpose_id","nonce","boot_id"})||
           !dispatcher->Snapshot(generation,audio_epoch,receive,c)||!CameraBridgeContextValid(c,receive)||c.boot!=boot||
           !number(r,"turn_generation",tg)||!number(r,"generation",g)||!number(r,"epoch",e)||
           c.device!=str(r,"device_id")||c.conversation!=str(r,"conversation_id")||c.turn!=str(r,"turn_id")||
           c.turn_generation!=tg||c.binding.generation!=g||c.binding.epoch!=e||c.purpose_id!=str(r,"purpose_id")||c.boot!=str(r,"boot_id")||!CameraBridgeHex(str(r,"nonce")))return false;
        {
            std::lock_guard<std::mutex> l(camera_protocol_mutex_);
            const auto scope=c.boot+":"+c.purpose_id+":"+std::to_string(g)+":"+std::to_string(e);
            if(camera_clock_scope_!=scope){camera_clock_scope_=scope;for(auto& old:camera_clock_nonces_)old.clear();camera_clock_nonce_count_=0;}
            const auto nonce=str(r,"nonce");
            for(size_t i=0;i<camera_clock_nonce_count_;++i)if(nonce==camera_clock_nonces_[i])return false;
            if(camera_clock_nonce_count_==64)return false;
            camera_clock_nonces_[camera_clock_nonce_count_++]=nonce;
        }
        // Bounded 64 nonce ledger per expiring purpose; host retains challenge custody.
        const auto send=now_ms();if(!receive||send<receive||!dispatcher->Current(generation,audio_epoch,c,send))return false;
        cJSON* reply=cJSON_Duplicate(r,true);if(!reply)return false;
        cJSON_ReplaceItemInObjectCaseSensitive(reply,"type",cJSON_CreateString("camera_clock_sample"));
        cJSON_AddStringToObject(reply,"clock_domain","esp-monotonic-ms-v1");dec(reply,"producer_receive_ms",receive);dec(reply,"producer_send_ms",send);
        char* text=cJSON_PrintUnformatted(reply);bool sent=text&&CameraText(text,generation,audio_epoch);cJSON_free(text);cJSON_Delete(reply);return sent;
    }
    const std::set<std::string> stopkeys={"type","version","device","conversation","turn","generation","epoch","boot","authority_id","challenge","stop_sequence"};
    if(type=="camera_stop_ack"){
        auto ackkeys=stopkeys;ackkeys.insert("report_serial");CameraStopChallenge c;uint64_t serial=0;
        {
            // Serialize release with proposal/commit/device-choice adoption.
            std::lock_guard<std::mutex> choice_lock(camera_choice_mutex_);
            std::lock_guard<std::mutex> l(camera_protocol_mutex_);
            if(dispatcher!=camera_purpose_dispatcher_||!keys(r,ackkeys)||!number(r,"report_serial",serial)||
               !stop_matches(r,camera_stop_challenge_.owner,c)||c.nonce!=camera_stop_challenge_.nonce||
               c.sequence!=camera_stop_challenge_.sequence||serial!=camera_stop_report_.driver().report_serial())return false;
            // A consumed exact ACK can be confirmed again after allocation/send
            // failure or reconnect. It never releases or grants a newer purpose.
            if(!camera_stop_ack_consumed_){
                if(!dispatcher->RetainedStopCurrent(generation,audio_epoch,c.owner)||
                   !dispatcher->RevokedNoNewerStream(c.owner,camera_stop_report_)||!camera_restart_.ReadyAck(c,serial)||
                   !hal_bridge::board_camera_release_restart(camera_stop_report_)||
                   !camera_purpose_dispatcher_->ReleaseAfterVerifiedAck(c.owner,camera_stop_report_)||!camera_restart_.Acknowledge(c,serial))return false;
                camera_stop_ack_consumed_=true;
                // Only exact verified cleanup clears purpose custody.
                std::atomic_store(&camera_confirmed_,std::shared_ptr<const CameraAuthorityContext>{});
                std::atomic_store(&camera_confirmed_proposal_,std::shared_ptr<const CameraPermissionProposal>{});
                camera_permission_commit_consumed_=false;camera_confirmed_revision_=0;camera_purpose_revoked_=true;
            }else{
                std::lock_guard<std::mutex> transport_lock(mutex_);
                if(camera_device_!=c.owner.device)return false;
            }
        }
        cJSON* reply=cJSON_Duplicate(r,true);if(!reply)return false;
        cJSON_ReplaceItemInObjectCaseSensitive(reply,"type",cJSON_CreateString("camera_stop_acknowledged"));
        char* text=cJSON_PrintUnformatted(reply);const bool sent=text&&CameraText(text,generation,audio_epoch);cJSON_free(text);cJSON_Delete(reply);return sent;
    }
    if(type!="camera_stop_challenge"||!keys(r,stopkeys))return false;
    CameraAuthorityContext c;CameraStopChallenge challenge;
    // Cleanup uses retained authority after expiry/revocation, never extends capture.
    if(!dispatcher->RetainedSnapshot(generation,audio_epoch,c)||c.boot!=boot||!stop_matches(r,c,challenge)||
       !dispatcher->RetainedStopCurrent(generation,audio_epoch,c))return false;
    // A valid retained Stop request itself revokes capture, regardless of
    // whether cancellation was delivered first on this admitted transport.
    RevokeCameraSession();
    std::lock_guard<std::mutex> l(camera_protocol_mutex_);
    if(!camera_restart_.Begin(challenge)||!hal_bridge::board_camera_hold_restart(c.binding))return false;
    camera_stop_challenge_=challenge;camera_stop_ack_consumed_=false;
    // A validated retained cleanup retry may arrive on a replacement transport.
    // Refresh cleanup routing before retaining the existing worker; this neither
    // extends capture authority nor creates a new camera purpose.
    camera_stop_connection_=generation;camera_stop_audio_epoch_=audio_epoch;
    if(camera_stop_worker_.joinable())return true;
    camera_worker_finished_=false;
    camera_stop_worker_=std::thread([this,challenge]{
        hal_bridge::board_camera_revoke(challenge.owner.binding);
        auto report=hal_bridge::board_camera_stop_checked(challenge.owner.binding);
        {std::lock_guard<std::mutex> guard(camera_protocol_mutex_);camera_stop_report_=report;}
        camera_worker_finished_=true;
    });return true;
}
void CompanionSocket::PollCameraStop(){
    if(!camera_worker_finished_.load())return;
    std::shared_ptr<TrustedCameraSessionDispatcher> dispatcher;CameraStopChallenge c;desk::BoardCameraStopReport report;uint64_t generation;uint32_t epoch;
    {
        std::lock_guard<std::mutex> l(camera_protocol_mutex_);
        if(!camera_stop_worker_.joinable())return;
        camera_stop_worker_.join();camera_worker_finished_=false;
        dispatcher=camera_dispatcher_;c=camera_stop_challenge_;report=camera_stop_report_;generation=camera_stop_connection_;epoch=camera_stop_audio_epoch_;
    }
    if(!dispatcher||!dispatcher->RetainedStopCurrent(generation,epoch,c.owner)||
       !camera_restart_.Completed(report,true,dispatcher->RevokedNoNewerStream(c.owner,report)))return;
    const auto& d=report.driver();cJSON* r=cJSON_CreateObject();if(!r)return;
    cJSON_AddStringToObject(r,"type","camera_driver_stop");stop_tuple(r,c);dec(r,"report_serial",d.report_serial());
    cJSON_AddStringToObject(r,"outcome",d.owned_driver_closed()?"DriverClosed":"NotStarted");
    cJSON_AddNumberToObject(r,"required",d.stages().required);cJSON_AddNumberToObject(r,"completed",d.stages().completed);
    cJSON_AddBoolToObject(r,"revoked",d.authorization_revoked());cJSON_AddBoolToObject(r,"joined",true);
    cJSON_AddBoolToObject(r,"wiped",d.raw_frame_wiped());cJSON_AddBoolToObject(r,"custody_released",report.custody_released());
    cJSON_AddBoolToObject(r,"close_uncertain",d.close_uncertain());cJSON_AddBoolToObject(r,"ever_started",d.stages().ever_started);
    char* text=cJSON_PrintUnformatted(r);if(text&&std::char_traits<char>::length(text)<=1024)CameraText(text,generation,epoch);
    cJSON_free(text);cJSON_Delete(r);
}
}
