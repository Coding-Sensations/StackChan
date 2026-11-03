#include "personal_binding.hpp"
#include "remembered_service.hpp"
#include "remembered_audio_worker.hpp"
#include "boot.h"
#include "existing_sc_pair.hpp"
#include "local_tls.h"
#include <esp_timer.h>
#include <esp_mac.h>
#include <mbedtls/sha256.h>
#include <ctime>
#include <cstring>
#include <cstdio>
#include <atomic>
#ifndef STACKCHAN_COMPANION_REMEMBERED_RUNTIME
#define STACKCHAN_COMPANION_REMEMBERED_RUNTIME 0
#endif
namespace companion {
namespace {
uint64_t Ms(){return esp_timer_get_time()/1000;}
uint64_t Wall(){auto t=std::time(nullptr);return t>1700000000?uint64_t(t):0;}
struct Service {
    NvsExistingScPairBackend backend;ExistingScPairVault store;
    RememberedAudioWorker worker;RememberedStartup startup;
    explicit Service(RememberedPeerBinding binding):store(backend,std::move(binding),STACKCHAN_PIN_EXPIRY),worker(store),startup(store,worker){}
};
// Boot-lifetime owner: never free TLS callback/codec state while it unwinds.
std::mutex physical_control_mutex;const void* physical_control=nullptr;uint64_t physical_view=0;
const void* camera_allow=nullptr;const void* camera_decline=nullptr;uint64_t camera_view=0;
Service* service=nullptr;std::atomic<bool> present{false};std::atomic<bool> active{false},listen_ready{false},armed{false};std::atomic<uint64_t> published_generation{0};
void Publish(){
 if(!service){active=false;listen_ready=false;armed=false;published_generation=0;return;}
 auto& c=service->startup.Events();
 listen_ready=false;published_generation=c.Generation();active=c.State()!=ReconnectState::Stopped;armed=c.State()==ReconnectState::Armed;
 listen_ready=c.State()==ReconnectState::ListenRequired;
}
[[maybe_unused]] bool CompiledBinding(const RememberedPeerBinding& b){
    const char* ca=companion_local_ca_pem();const char* host=companion_local_tls_host();
    if(!PersonalBindingAvailable()||b.device!=STACKCHAN_PIN_DEVICE||b.ca_sha256!=STACKCHAN_PIN_CA_SHA256||!ca||!*ca||!host||std::strcmp(host,STACKCHAN_PIN_HOST)||b.uri!=STACKCHAN_PIN_URI)return false;
    uint8_t mac[6];if(esp_read_mac(mac,ESP_MAC_WIFI_STA)!=ESP_OK)return false;
    char device[32];std::snprintf(device,sizeof(device),"stackchan-%02x%02x%02x%02x%02x%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);if(b.device!=device)return false;
    uint8_t digest[32];if(mbedtls_sha256(reinterpret_cast<const unsigned char*>(ca),std::strlen(ca),digest,0))return false;
    const char* hex="0123456789abcdef";std::string hash;for(auto byte:digest){hash+=hex[byte>>4];hash+=hex[byte&15];}return hash==b.ca_sha256;
}
}
void RegisterRememberedPhysicalControl(const void* control,uint64_t view){std::lock_guard<std::mutex> lock(physical_control_mutex);physical_control=control;physical_view=view;if(service)service->worker.RegisterPhysicalControl(control,view);}
void RegisterRememberedCameraControls(const void* allow,const void* decline,uint64_t view){
 std::lock_guard<std::mutex> lock(physical_control_mutex);camera_allow=allow;camera_decline=decline;camera_view=view;
 if(service)service->worker.RegisterCameraControls(allow,decline,view);
}
std::string ShowRememberedCameraPermission(){return service?service->worker.ShowCameraPermission():std::string{};}
void HideRememberedCameraPermission(){if(service)service->worker.HideCameraPermission();}
bool CaptureRememberedCameraChoice(const void* control,uint64_t view){
 std::lock_guard<std::mutex> lock(physical_control_mutex);const auto generation=published_generation.load();
 return service&&view&&view==camera_view&&(control==camera_allow||control==camera_decline)&&armed.load()&&
     service->worker.CaptureCameraChoice(control,view,generation);
}
void CaptureRememberedLocalProcessingGesture(const void* control,uint64_t view,uint64_t generation){std::lock_guard<std::mutex> lock(physical_control_mutex);if(service&&control==physical_control&&physical_view==view)service->worker.CapturePhysicalLocalProcessing(control,view,generation);}
PeerReadStatus ConfigureRememberedRuntime(const RememberedPeerBinding& binding,bool approved){
    if(!STACKCHAN_COMPANION_REMEMBERED_RUNTIME)return PeerReadStatus::Unavailable;
    if(!approved||!CompiledBinding(binding))return PeerReadStatus::TrustMismatch;
    if(service&&!RememberedRuntimeStopped())return PeerReadStatus::Unavailable;
    if(!service)service=new Service(binding);
    {std::lock_guard<std::mutex> lock(physical_control_mutex);service->worker.RegisterPhysicalControl(physical_control,physical_view);service->worker.RegisterCameraControls(camera_allow,camera_decline,camera_view);}
    auto status=service->startup.Start(binding,Ms(),Wall(),true);
    if(status!=PeerReadStatus::Available){service->store.ReleaseSecret();return status;}
    if(service->startup.Events().Peer().expires_unix!=STACKCHAN_PIN_EXPIRY){service->startup.Events().Mute();service->startup.Events().CleanupJoined(service->startup.Events().Generation(),Ms());service->store.ReleaseSecret();return PeerReadStatus::TrustMismatch;}
    if(!service->worker.StartWorker(true)){service->startup.Mute();service->startup.Events().CleanupJoined(service->startup.Events().Generation(),Ms());service->store.ReleaseSecret();return PeerReadStatus::Unavailable;}
    present=true;Publish();return status;
}
void BootRememberedRuntime(){
    if(!STACKCHAN_COMPANION_REMEMBERED_RUNTIME||!RememberedRuntimeStopped()||!RememberedStartAllowed())return;
    ConfigureRememberedRuntime({STACKCHAN_PIN_DEVICE,STACKCHAN_PIN_URI,STACKCHAN_PIN_CA_SHA256},true);
}
bool RememberedRuntimePresent(){return present.load();}
void TickRememberedRuntime(){
    if(!present||!service)return;
    RememberedWorkerEvent event{RememberedWorkerEvent::Kind::Failure,0,0};
    auto& events=service->startup.Events();
    while(service->worker.Poll(event)){
        if(event.generation!=events.Generation())continue; // stale mailbox events cannot mute a new attempt
        switch(event.kind){
        case RememberedWorkerEvent::Kind::Authenticated:events.Authenticated(event.generation,Ms());break;
        case RememberedWorkerEvent::Kind::Armed:if(!events.Armed(event.generation,Ms(),Wall())){service->worker.InterruptImmediately();events.Failed(event.generation,ReconnectFailure::Transient,Ms());}break;
        case RememberedWorkerEvent::Kind::Failure:events.Failed(event.generation,event.failure,Ms());break;
        case RememberedWorkerEvent::Kind::Joined:events.CleanupJoined(event.generation,Ms());break;
        }
    }
    service->startup.Tick(Ms(),Wall(),IsWifiConnected());Publish();
}
uint64_t CaptureRememberedListen(){auto first=published_generation.load();bool ready=listen_ready.load();auto second=published_generation.load();return ready&&first==second?first:0;}
bool RememberedPhysicalListen(uint64_t captured){bool ok=present&&service&&captured&&service->startup.Events().PhysicalListen(captured);Publish();return ok;}
void InterruptRememberedRuntimeImmediately(){listen_ready=false;if(present&&service)service->worker.InterruptImmediately();}
bool RememberedTransportActive(){return active.load();}
bool RememberedAudioArmed(){return armed.load();}
void MuteRememberedRuntime(){listen_ready=false;if(present&&service){service->startup.Mute();Publish();}}
bool AuthenticatedRememberedEnrollmentStored(){return !service||service->store.AuthenticatedEnrollmentStored(true,RememberedRuntimeStopped());}
StoreResult RevokeRememberedRuntime(){return present&&service?service->startup.Revoke():StoreResult::Missing;}
const char* RememberedStatusText(){
    if(!service)return nullptr;
    auto& c=service->startup.Events();
    if(c.State()==ReconnectState::Stopped){
        if(c.Status()==PeerReadStatus::Revoked)return "Mira companion\nSaved pairing revoked - muted";
        if(c.Status()==PeerReadStatus::Expired)return "Mira companion\nSaved pairing expired - muted";
        if(c.Status()==PeerReadStatus::ClockUnavailable)return "Mira companion\nClock unavailable - muted";
        if(c.Status()!=PeerReadStatus::Available)return "Mira companion\nSaved pairing unavailable - muted";
        return "Mira companion\nMuted - tap Sync to reconnect";
    }
    switch(c.State()){
    case ReconnectState::Network:return "Mira companion\nSaved Mac - connecting Wi-Fi";
    case ReconnectState::Backoff:return "Mira companion\nSaved Mac - retry waiting; muted";
    case ReconnectState::Connecting:return "Mira companion\nSaved Mac - authenticating; muted";
    case ReconnectState::ListenRequired:return "Mira companion\nSaved Mac connected - tap Listen";
    case ReconnectState::Arming:return "Mira companion\nMuted - waiting for host approval";
    case ReconnectState::Armed:return "Mira companion\nSaved Mac audio active";
    case ReconnectState::Cleanup:return "Mira companion\nMuted - cleanup pending";
    case ReconnectState::Stopped:break;
    }
    return nullptr;
}
bool RememberedRuntimeStopped(){return !present||!service||service->startup.Events().State()==ReconnectState::Stopped;}
}
