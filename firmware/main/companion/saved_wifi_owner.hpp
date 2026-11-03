#pragma once
#include <atomic>
#include <cstdint>
namespace companion {
enum class SavedWifiPhase {Idle,SurveyReady,NoCredentials,Starting,Connecting,Connected,Disconnected,ConnectFailed,RestartRequired,Stopping};
enum class SavedWifiAction {Initialize,Connect,Waiting,AlreadyConnected,Refused};
// One boot-lifetime station owner. SDK calls remain in the existing boot adapter.
class SavedWifiOwner {
    std::atomic<SavedWifiPhase> phase_{SavedWifiPhase::Idle};
    std::atomic<bool> touched_{false},ready_{false},restart_required_{false},stopping_{false};
    std::atomic<uint16_t> disconnect_reason_{0};
public:
    SavedWifiAction Begin(bool credentials_valid,bool shared_driver_owned=false){
        if(stopping_||shared_driver_owned)return SavedWifiAction::Refused;
        const auto phase=Phase();
        if(phase==SavedWifiPhase::Connected)return SavedWifiAction::AlreadyConnected;
        if(phase==SavedWifiPhase::Connecting||phase==SavedWifiPhase::Starting)return SavedWifiAction::Waiting;
        if(phase==SavedWifiPhase::RestartRequired)return SavedWifiAction::Refused;
        if(!credentials_valid){phase_=SavedWifiPhase::NoCredentials;return SavedWifiAction::Refused;}
        if(ready_){phase_=SavedWifiPhase::Connecting;return SavedWifiAction::Connect;}
        phase_=SavedWifiPhase::Starting;return SavedWifiAction::Initialize;
    }
    SavedWifiAction BeginSurvey(bool shared_driver_owned){
        if(stopping_||shared_driver_owned)return SavedWifiAction::Refused;
        if(restart_required_)return SavedWifiAction::Refused;
        if(Phase()==SavedWifiPhase::Starting||Phase()==SavedWifiPhase::Connecting)return SavedWifiAction::Waiting;
        if(ready_)return SavedWifiAction::AlreadyConnected;
        phase_=SavedWifiPhase::Starting;return SavedWifiAction::Initialize;
    }
    bool DriverIsReady()const{return ready_&&!restart_required_&&!stopping_;}
    void StopRequested(){stopping_=true;}
    bool Stopped(bool checked){if(!checked)return false;ready_=false;restart_required_=false;disconnect_reason_=0;phase_=SavedWifiPhase::Idle;stopping_=false;return true;}
    void SurveyReady(){ready_=true;phase_=SavedWifiPhase::SurveyReady;}
    void Touched(){touched_=true;}
    void DriverReady(){ready_=true;}
    void InitializationFailed(){restart_required_=true;phase_=SavedWifiPhase::RestartRequired;}
    void ConnectResult(bool success){
        // A synchronous callback may already have advanced the actual link state.
        if(Phase()!=SavedWifiPhase::Connected&&Phase()!=SavedWifiPhase::Disconnected)
            phase_=success?SavedWifiPhase::Connecting:SavedWifiPhase::ConnectFailed;
    }
    void GotIp(){if(!stopping_){phase_=SavedWifiPhase::Connected;disconnect_reason_=0;}}
    void Disconnected(uint16_t reason){if(!stopping_){disconnect_reason_=reason;phase_=SavedWifiPhase::Disconnected;}}
    SavedWifiPhase Phase()const{return stopping_.load()?SavedWifiPhase::Stopping:restart_required_.load()?SavedWifiPhase::RestartRequired:phase_.load();}
    bool WasStarted()const{return touched_.load();}
    uint16_t DisconnectReason()const{return disconnect_reason_.load();}
};
inline const char* SavedWifiLabel(SavedWifiPhase phase){
    switch(phase){
    case SavedWifiPhase::SurveyReady:return "Wi-Fi ready for survey; not connected";
    case SavedWifiPhase::Idle:return "Wi-Fi not started";
    case SavedWifiPhase::NoCredentials:return "Save Wi-Fi in Setup first";
    case SavedWifiPhase::Starting:return "Starting saved Wi-Fi";
    case SavedWifiPhase::Connecting:return "Connecting saved Wi-Fi";
    case SavedWifiPhase::Connected:return "Link state unavailable";
    case SavedWifiPhase::Disconnected:return "Wi-Fi disconnected";
    case SavedWifiPhase::ConnectFailed:return "Connection request failed; retry";
    case SavedWifiPhase::RestartRequired:return "Wi-Fi cleanup required; retry after Home";
    case SavedWifiPhase::Stopping:return "Stopping saved Wi-Fi";
    }
    return "Wi-Fi state unavailable";
}
}
