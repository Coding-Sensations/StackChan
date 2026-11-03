#pragma once
#include "camera_session_bridge.hpp"
#include <set>
namespace companion {
struct CameraPermissionProposal {
    std::string request,nonce,device,conversation,turn,mode;
    uint64_t turn_generation=0,connection=0,deadline_ms=0;
    uint32_t audio_epoch=0,ttl_seconds=0;
    uint64_t revision=0;
    bool Same(const CameraPermissionProposal& b) const {
        return request==b.request&&nonce==b.nonce&&device==b.device&&conversation==b.conversation&&
            turn==b.turn&&mode==b.mode&&turn_generation==b.turn_generation&&connection==b.connection&&
            audio_epoch==b.audio_epoch&&ttl_seconds==b.ttl_seconds;
    }
};
class CameraLocalPermissionOwner;
// Only the real published local-control dispatcher can mint this choice.
class CapturedCameraChoice {
    CameraPermissionProposal proposal_{};bool allow_=false,valid_=false;
    friend class CameraLocalPermissionOwner;
public:
    CapturedCameraChoice()=default;
    explicit operator bool()const{return valid_;}
    const CameraPermissionProposal& proposal()const{return proposal_;}
    bool allow()const{return valid_&&allow_;}
};
class CameraLocalPermissionOwner {
    mutable std::mutex mutex_;
    const void* allow_=nullptr;const void* decline_=nullptr;uint64_t view_=0,last_now_=0;
    CameraPermissionProposal pending_{};std::string shown_nonce_;
    std::set<std::string> nonces_;bool pending_valid_=false;
    bool Now(uint64_t now){if(!now||now<last_now_){pending_valid_=false;shown_nonce_.clear();return false;}last_now_=now;return true;}
public:
    void PublishControl(const void* allow,const void* decline,uint64_t view){
        std::lock_guard<std::mutex> l(mutex_);
        if(allow!=allow_||decline!=decline_||view!=view_){shown_nonce_.clear();pending_valid_=false;}
        allow_=allow;decline_=decline;view_=view;
    }
    bool Propose(const CameraPermissionProposal& p,uint64_t now){
        std::lock_guard<std::mutex> l(mutex_);
        if(!Now(now)||pending_valid_||!allow_||!decline_||!view_||!p.connection||!p.audio_epoch||
           !p.turn_generation||!CameraBridgeAscii(p.request,64)||!CameraBridgeHex(p.nonce)||
           !CameraBridgeAscii(p.device,64)||!CameraBridgeAscii(p.conversation,128)||!CameraBridgeAscii(p.turn,128)||
           (p.mode!="webcam"&&p.mode!="gestures")||!p.ttl_seconds||p.ttl_seconds>60||
           p.deadline_ms<=now||p.deadline_ms-now>uint64_t(p.ttl_seconds)*1000||
           nonces_.size()>=64||nonces_.count(p.nonce))return false;
        nonces_.insert(p.nonce);pending_=p;pending_valid_=true;shown_nonce_.clear();return true;
    }
    bool Show(uint64_t connection,uint32_t epoch,uint64_t now,CameraPermissionProposal& out){
        std::lock_guard<std::mutex> l(mutex_);out={};
        if(!Now(now)||!pending_valid_||!view_||connection!=pending_.connection||epoch!=pending_.audio_epoch||now>=pending_.deadline_ms)return false;
        shown_nonce_=pending_.nonce;out=pending_;return true;
    }
    CapturedCameraChoice Capture(const void* control,uint64_t view,uint64_t connection,uint32_t epoch,uint64_t now){
        std::lock_guard<std::mutex> l(mutex_);CapturedCameraChoice out;
        if(!Now(now)||!pending_valid_||!view||view!=view_||(control!=allow_&&control!=decline_)||
           connection!=pending_.connection||epoch!=pending_.audio_epoch||now>=pending_.deadline_ms||
           shown_nonce_!=pending_.nonce)return out;
        out.proposal_=pending_;out.allow_=control==allow_;out.valid_=true;
        pending_valid_=false;shown_nonce_.clear();return out;
    }
    bool Cancel(const CameraPermissionProposal& p){
        std::lock_guard<std::mutex> l(mutex_);
        if(!pending_valid_||!pending_.Same(p))return false;
        pending_valid_=false;shown_nonce_.clear();return true;
    }
    void Hide(){std::lock_guard<std::mutex> l(mutex_);shown_nonce_.clear();}
    void Invalidate(){std::lock_guard<std::mutex> l(mutex_);pending_valid_=false;shown_nonce_.clear();}
};
}
