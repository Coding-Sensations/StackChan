#pragma once
#include <atomic>
namespace companion::desk {
template<class Close,class Render>bool StopAndRenderClosing(Close close,Render render){const bool stopped=close();if(!stopped)render();return stopped;}
template<class Detach>void BeforeClosingPanelDelete(bool stopped,Detach detach){if(!stopped)detach();}
enum class ClosingState { Idle, Pending, Unconfirmed, ClosedChecked };
// Closure-only custody: no activation or permission is supplied by this policy.
class ClosingViewPolicy {
    std::atomic<ClosingState> state_{ClosingState::Idle};
    std::atomic<bool> worker_{false};
public:
    void Retain(){state_=ClosingState::Pending;}
    bool Pending()const{return state_==ClosingState::Pending||state_==ClosingState::Unconfirmed;}
    bool BeginWorker(){bool expected=false;return worker_.compare_exchange_strong(expected,true);}
    void EndWorker(){worker_=false;}
    bool OwnerMustRetry()const{return Pending()&&!worker_;}
    void ReportClosed(bool checked_closed_and_joined){state_=checked_closed_and_joined?ClosingState::ClosedChecked:ClosingState::Unconfirmed;}
    const char* Text()const{
        switch(state_.load()){
        case ClosingState::ClosedChecked:return "Companion directions closed\nPhysical playback drain unverified";
        case ClosingState::Unconfirmed:return "Mic/output state unverified\nStop unconfirmed - custody retained";
        case ClosingState::Pending:return "Mic/output state unverified\nStop pending - custody retained";
        case ClosingState::Idle:return "Mic/output state unverified";
        }
        return "Mic/output state unverified";
    }
};
}
