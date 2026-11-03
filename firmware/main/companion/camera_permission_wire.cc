#include "companion_socket.h"
#include "hal/board/hal_bridge.h"
#include <cJSON.h>
#include <esp_timer.h>
#include <cmath>
#include <set>
#include <algorithm>
namespace companion {
namespace {
uint64_t PurposeNow(){const auto n=esp_timer_get_time();return n>0?uint64_t(n)/1000:0;}
std::string PurposeString(cJSON* r,const char* k){auto v=cJSON_GetObjectItemCaseSensitive(r,k);return cJSON_IsString(v)&&v->valuestring?v->valuestring:"";}
bool PurposeUnsigned(cJSON* r,const char* k,uint64_t& out){
    out=0;const auto s=PurposeString(r,k);
    if(s.empty()||s.size()>20||s[0]<'1'||s[0]>'9')return false;
    for(char c:s){if(c<'0'||c>'9'||out>(UINT64_MAX-uint64_t(c-'0'))/10)return false;out=out*10+uint64_t(c-'0');}return out!=0;
}
bool PurposeKeys(cJSON* r,bool commit=false){
    std::set<std::string> keys;for(auto v=r->child;v;v=v->next)if(!v->string||!keys.insert(v->string).second)return false;
    auto ver=cJSON_GetObjectItemCaseSensitive(r,"version"),ttl=cJSON_GetObjectItemCaseSensitive(r,"ttl_seconds");
    std::set<std::string> expected{"type","version","request_id","nonce","device_id","conversation_id","turn_id","turn_generation","audio_epoch","mode","ttl_seconds"};
    if(commit)for(const auto* key:{"generation","epoch","authority_id","boot_id","expires_ms","remaining_seconds"})expected.insert(key);
    return keys==expected&&
        cJSON_IsNumber(ver)&&ver->valuedouble==1&&cJSON_IsNumber(ttl)&&ttl->valuedouble>=1&&ttl->valuedouble<=60&&std::floor(ttl->valuedouble)==ttl->valuedouble;
}
bool SamePurpose(const CameraAuthorityContext& a,const CameraAuthorityContext& b){
    return a.device==b.device&&a.conversation==b.conversation&&a.turn==b.turn&&a.purpose_id==b.purpose_id&&a.boot==b.boot&&
        a.turn_generation==b.turn_generation&&a.expires_ms==b.expires_ms&&a.authority_id==b.authority_id&&a.binding==b.binding;
}
void PurposeDecimal(cJSON* r,const char* k,uint64_t n){cJSON_AddStringToObject(r,k,std::to_string(n).c_str());}
}
CompanionSocket::CompanionSocket(CodecTasks& tasks):tasks_(tasks){
    auto current=[this](uint64_t connection,uint32_t epoch,const CameraAuthorityContext& c){
        auto held=std::atomic_load(&camera_confirmed_);
        return held&&camera_permission_commit_consumed_&&!camera_purpose_revoked_&&camera_confirmed_revision_==camera_permission_revision_&&
            transport_current_&&connection==connection_generation_&&epoch==epoch_&&SamePurpose(*held,c);
    };
    auto physical_choice=[this](uint64_t connection,uint32_t epoch,const CameraAuthorityContext&){return camera_control_.DeviceOptInCurrent(connection,epoch);};
    auto cleanup=[this](uint64_t connection,uint32_t epoch,const CameraAuthorityContext& c){
        auto held=std::atomic_load(&camera_confirmed_);
        std::lock_guard<std::mutex> l(mutex_);
        if(!held)return false;
        auto identity=*held;identity.expires_ms=c.expires_ms;
        // Cleanup identity survives bounded commit expiry adjustment/failure.
        return transport_current_&&connection==connection_generation_&&epoch==epoch_&&camera_device_==c.device&&SamePurpose(identity,c);
    };
    auto checked_ack=[this](const CameraAuthorityContext& c,const desk::BoardCameraStopReport& r){
        // Called only under camera_protocol_mutex_ by the exact ACK branch.
        // Readiness comes from the retained joined worker and sealed board report,
        // never a peer boolean, permission grant, or an ordinary Stop return.
        return SamePurpose(c,camera_stop_challenge_.owner)&&
            r.driver().report_serial()==camera_stop_report_.driver().report_serial()&&
            camera_restart_.ReadyAck(camera_stop_challenge_,r.driver().report_serial());
    };
    camera_purpose_dispatcher_=std::make_shared<BoundedCameraSessionDispatcher>(current,physical_choice,cleanup,checked_ack);
    SetTrustedCameraDispatcher(camera_purpose_dispatcher_);
}
void CompanionSocket::PublishCameraPhysicalControls(const void* allow,const void* decline,uint64_t view){
    if(camera_view_generation_.exchange(view)!=view)RevokeCameraSession();
    camera_local_permission_.PublishControl(allow,decline,view);
}
std::string CompanionSocket::ShowCameraPermission(){
    CameraPermissionProposal p;
    if(!transport_current_||(state_!=State::Listening&&state_!=State::Speaking)||
       !camera_local_permission_.Show(connection_generation_,epoch_,PurposeNow(),p))return {};
    return "Camera permission: "+p.mode+"\nThis turn, "+std::to_string(p.ttl_seconds)+" seconds\nCapture remains unqualified";
}
bool CompanionSocket::CaptureCameraLocalChoice(const void* control,uint64_t view){
    if(!transport_current_||(state_!=State::Listening&&state_!=State::Speaking))return false;
    const auto choice=camera_local_permission_.Capture(control,view,connection_generation_,epoch_,PurposeNow());
    return choice&&DeviceCameraChoice(choice);
}
bool CompanionSocket::CameraPermissionReply(const CameraPermissionProposal& p,const char* state,const char* reason,const CameraAuthorityContext* a){
    cJSON* r=cJSON_CreateObject();if(!r)return false;
    cJSON_AddStringToObject(r,"type","camera_permission_confirmation");cJSON_AddNumberToObject(r,"version",1);
    cJSON_AddStringToObject(r,"request_id",p.request.c_str());cJSON_AddStringToObject(r,"nonce",p.nonce.c_str());
    cJSON_AddStringToObject(r,"device_id",p.device.c_str());cJSON_AddStringToObject(r,"conversation_id",p.conversation.c_str());cJSON_AddStringToObject(r,"turn_id",p.turn.c_str());
    PurposeDecimal(r,"turn_generation",p.turn_generation);PurposeDecimal(r,"audio_epoch",p.audio_epoch);
    cJSON_AddStringToObject(r,"mode",p.mode.c_str());cJSON_AddNumberToObject(r,"ttl_seconds",p.ttl_seconds);cJSON_AddStringToObject(r,"state",state);
    if(a){PurposeDecimal(r,"generation",a->binding.generation);PurposeDecimal(r,"epoch",a->binding.epoch);PurposeDecimal(r,"authority_id",a->authority_id);PurposeDecimal(r,"expires_ms",a->expires_ms);cJSON_AddStringToObject(r,"boot_id",a->boot.c_str());}
    else cJSON_AddStringToObject(r,"reason",reason);
    char* wire=cJSON_PrintUnformatted(r);const bool sent=wire&&CameraText(wire,p.connection,p.audio_epoch);cJSON_free(wire);cJSON_Delete(r);return sent;
}
bool CompanionSocket::CameraPermissionProtocol(void* value,uint64_t connection){
    std::lock_guard<std::mutex> choice_lock(camera_choice_mutex_);
    auto r=static_cast<cJSON*>(value);const auto now=PurposeNow();uint64_t tg=0,epoch=0;
    const bool commit=PurposeString(r,"type")=="camera_permission_commit";
    if(!PurposeKeys(r,commit)||!PurposeUnsigned(r,"turn_generation",tg)||!PurposeUnsigned(r,"audio_epoch",epoch)||epoch>UINT32_MAX||
       !transport_current_||connection!=connection_generation_||epoch!=epoch_||
       (state_!=State::Listening&&state_!=State::Speaking))return false;
    auto ttl=cJSON_GetObjectItemCaseSensitive(r,"ttl_seconds");
    CameraPermissionProposal p{PurposeString(r,"request_id"),PurposeString(r,"nonce"),PurposeString(r,"device_id"),PurposeString(r,"conversation_id"),PurposeString(r,"turn_id"),PurposeString(r,"mode"),tg,connection,now+uint64_t(ttl->valuedouble)*1000,uint32_t(epoch),uint32_t(ttl->valuedouble),camera_permission_revision_.load()};
    {std::lock_guard<std::mutex> l(mutex_);if(p.device!=camera_device_||p.conversation!=camera_conversation_||p.turn!=camera_turn_||p.turn_generation!=camera_turn_generation_)return false;}
    if(PurposeString(r,"type")=="camera_permission_cancel"){
        const auto held=std::atomic_load(&camera_confirmed_proposal_);
        if(!camera_local_permission_.Cancel(p)&&(!held||!held->Same(p)))return false;
        RevokeCameraSession();return true;
    }
    if(commit){
        auto held=std::atomic_load(&camera_confirmed_);const auto local=std::atomic_load(&camera_confirmed_proposal_);
        uint64_t g=0,e=0,id=0,expires=0;auto remaining=cJSON_GetObjectItemCaseSensitive(r,"remaining_seconds");
        if(!held||!local||!local->Same(p)||local->revision!=camera_permission_revision_||camera_permission_commit_consumed_||
           now>=held->expires_ms||!cJSON_IsNumber(remaining)||remaining->valuedouble<1||remaining->valuedouble>p.ttl_seconds||std::floor(remaining->valuedouble)!=remaining->valuedouble||
           !PurposeUnsigned(r,"generation",g)||!PurposeUnsigned(r,"epoch",e)||!PurposeUnsigned(r,"authority_id",id)||!PurposeUnsigned(r,"expires_ms",expires)||
           g!=held->binding.generation||e!=held->binding.epoch||id!=held->authority_id||expires!=held->expires_ms||PurposeString(r,"boot_id")!=held->boot||
           !camera_control_.DeviceOptInCurrent(connection,uint32_t(epoch)))return false;
        auto adopted=*held;adopted.expires_ms=std::min(adopted.expires_ms,now+uint64_t(remaining->valuedouble)*1000);
        std::atomic_store(&camera_confirmed_,std::make_shared<const CameraAuthorityContext>(adopted));
        camera_permission_commit_consumed_=true;camera_purpose_revoked_=false;
        if(!camera_purpose_dispatcher_->InstallFromTrustedOwner(connection,uint32_t(epoch),now,adopted)){
            RevokeCameraSession();return false;
        }
        return true;
    }
    // An unresolved board/Stop owner cannot be replaced by a new UI proposal.
    if(std::atomic_load(&camera_confirmed_)||camera_restart_.Pending()||!camera_local_permission_.Propose(p,now))
        return CameraPermissionReply(p,"unavailable","camera_owner_busy");
    return true;
}
bool CompanionSocket::DeviceCameraChoice(const CapturedCameraChoice& choice){
    std::lock_guard<std::mutex> choice_lock(camera_choice_mutex_);
    const auto& p=choice.proposal();const auto now=PurposeNow();
    if(!transport_current_||p.connection!=connection_generation_||p.audio_epoch!=epoch_||p.revision!=camera_permission_revision_||now>=p.deadline_ms)
        return CameraPermissionReply(p,"unavailable","device_permission_expired");
    if(!choice.allow())return CameraPermissionReply(p,"unavailable","device_declined");
    desk::CameraBinding lease;
    // Actual board owner enforces real readiness. No synthetic qualification,
    // driver start, Capture, stream or camera bridge activation occurs here.
    const auto result=hal_bridge::board_camera_acquire(p.audio_epoch,true,lease);
    if(result!=desk::CameraSessionResult::Reserved)return CameraPermissionReply(p,"unavailable",result==desk::CameraSessionResult::Unavailable?"camera_driver_unqualified":"camera_owner_busy");
    CameraAuthorityContext c{p.device,p.conversation,p.turn,p.nonce,CameraProducerBoot(),p.turn_generation,p.deadline_ms,hal_bridge::board_camera_authority_id(lease),lease};
    // Keep exact acquired ownership even if disconnect/expiry races adoption.
    std::atomic_store(&camera_confirmed_,std::make_shared<const CameraAuthorityContext>(c));
    std::atomic_store(&camera_confirmed_proposal_,std::make_shared<const CameraPermissionProposal>(p));
    camera_confirmed_revision_=p.revision;camera_purpose_revoked_=true;
    // Retain the real lease before any fallible opt-in or transport operation.
    if(!camera_purpose_dispatcher_->RetainReservationFromTrustedOwner(p.connection,p.audio_epoch,now,c)){
        hal_bridge::board_camera_revoke(lease);
        return CameraPermissionReply(p,"unavailable","camera_owner_busy");
    }
    if(!c.authority_id||p.revision==UINT64_MAX||p.revision!=camera_permission_revision_||
       !PhysicalCameraOptIn(p.audio_epoch)){
        camera_purpose_revoked_=true;hal_bridge::board_camera_revoke(lease);
        return CameraPermissionReply(p,"unavailable","camera_owner_busy");
    }
    const bool sent=CameraPermissionReply(p,"confirmed",nullptr,&c);
    if(!sent)RevokeCameraSession();
    return sent;
}
}
