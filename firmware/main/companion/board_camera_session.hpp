#pragma once
#include "desk_camera_adapter.hpp"
#include "camera_stop_receipt.hpp"
#include <mutex>
namespace companion::desk {
enum class CameraSessionResult { Denied, Unavailable, Busy, Reserved, Started, Stopping, Closed, Stale, DriverFailure };
struct CameraReadiness {
    bool capture_clock_qualified=false,driver_stop_qualified=false;
    bool available() const noexcept {return capture_clock_qualified&&driver_stop_qualified;}
    const char* reason() const noexcept {
        if(!capture_clock_qualified)return "Camera unavailable: capture clock and checked driver stop need qualification";
        if(!driver_stop_qualified)return "Camera unavailable: checked driver stop needs qualification";
        return "Camera ready for explicit local opt-in";
    }
};
// Session state is distinct from hardware closure custody. Driver calls run
// outside this short state lock, on the caller's dedicated camera worker.
class BoardCameraSession {
    std::mutex mutex_;
    CameraBinding lease_{};
    uint64_t generation_=0;
    bool custody_=false,opt_in_=false,start_pending_=false,running_=false,start_attempted_=false;
    const uint64_t authority_=NextCameraAuthorityId();
    uint64_t stop_serial_=0;
    bool restart_held_=false;CameraBinding restart_binding_{},last_closed_binding_{};
public:
    CameraSessionResult Acquire(uint64_t epoch,bool opt_in,CameraReadiness ready,CameraBinding& result) {
        result={};std::lock_guard<std::mutex> lock(mutex_);
        if(!epoch||!opt_in)return CameraSessionResult::Denied;
        if(!ready.available())return CameraSessionResult::Unavailable;
        if(restart_held_||custody_||generation_==UINT64_MAX)return CameraSessionResult::Busy;
        lease_={++generation_,epoch};custody_=true;opt_in_=true;start_attempted_=false;result=lease_;
        return CameraSessionResult::Reserved;
    }
    CameraSessionResult BeginStart(CameraBinding lease,CameraReadiness ready) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!custody_||!(lease==lease_))return CameraSessionResult::Stale;
        if(!opt_in_)return CameraSessionResult::Stopping;
        if(!ready.available())return CameraSessionResult::Unavailable;
        if(start_pending_||running_)return CameraSessionResult::Busy;
        start_pending_=start_attempted_=true;return CameraSessionResult::Reserved;
    }
    CameraSessionResult FinishStart(CameraBinding lease,bool driver_started) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!custody_||!(lease==lease_))return CameraSessionResult::Stale;
        if(!start_pending_)return CameraSessionResult::Stale;
        start_pending_=false;running_=driver_started;
        if(!driver_started){opt_in_=false;return CameraSessionResult::DriverFailure;}
        return opt_in_?CameraSessionResult::Started:CameraSessionResult::Stopping;
    }
    bool RetainedNoNewer(CameraBinding b) {std::lock_guard<std::mutex> l(mutex_);return !custody_&&!start_pending_&&generation_==b.generation&&b==last_closed_binding_;}
    bool HoldRestart(CameraBinding b) {
        std::lock_guard<std::mutex> l(mutex_);
        if(restart_held_)return restart_binding_==b;
        if(!((custody_&&lease_==b)||(!custody_&&b==last_closed_binding_&&generation_==b.generation)))return false;
        restart_held_=true;restart_binding_=b;return true;
    }
    bool ReleaseRestart(const CameraDriverStopReceipt& r) {
        std::lock_guard<std::mutex> l(mutex_);
        if(!restart_held_)return !custody_&&!start_pending_&&generation_==r.binding().generation&&r.binding()==last_closed_binding_&&(r.owned_driver_closed()||r.owned_never_started());
        if(custody_||start_pending_||generation_!=restart_binding_.generation||!(r.binding()==restart_binding_)||
           (!r.owned_driver_closed()&&!r.owned_never_started()))return false;
        restart_held_=false;restart_binding_={};return true;
    }
    bool RefreshNeverStarted(CameraBinding b,uint64_t& authority,uint64_t& serial) {
        authority=serial=0;std::lock_guard<std::mutex> l(mutex_);
        if(custody_||start_pending_||start_attempted_||generation_!=b.generation||!(b==last_closed_binding_)||stop_serial_==UINT64_MAX||!authority_)return false;
        authority=authority_;serial=++stop_serial_;return true;
    }
    uint64_t NeverStartedAuthority(CameraBinding lease) {
        std::lock_guard<std::mutex> lock(mutex_);
        return custody_&&lease==lease_&&!start_attempted_?authority_:0;
    }
    bool Revoke(CameraBinding lease) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!custody_||!(lease==lease_))return false;
        opt_in_=false;return true;
    }
    bool Current(CameraBinding lease) {
        std::lock_guard<std::mutex> lock(mutex_);
        return custody_&&opt_in_&&lease==lease_;
    }
    bool ReleaseChecked(CameraBinding lease,bool closed) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!custody_||!(lease==lease_)||opt_in_||start_pending_||!closed)return false;
        custody_=running_=false;lease_={};return true;
    }
    bool ReleaseReceipt(CameraBinding lease,const CameraDriverStopReceipt& receipt) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!custody_||!(lease==lease_)||opt_in_||start_pending_||!(receipt.binding()==lease)||
           (!receipt.owned_driver_closed()&&!receipt.owned_never_started()))return false;
        last_closed_binding_=lease;custody_=running_=false;lease_={};return true;
    }
    bool ReleaseNeverStarted(CameraBinding lease,uint64_t& authority,uint64_t& serial) {
        authority=serial=0;std::lock_guard<std::mutex> lock(mutex_);
        if(!custody_||!(lease==lease_)||opt_in_||start_pending_||start_attempted_||running_||!authority_||stop_serial_==UINT64_MAX)return false;
        authority=authority_;serial=++stop_serial_;last_closed_binding_=lease;custody_=false;lease_={};return true;
    }
};
}
