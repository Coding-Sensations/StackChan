// Synthetic SDK boundary fixture; production function bodies inserted verbatim.
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <algorithm>
#include "companion/diagnostics.h"
#include "companion/pair_ui_action.hpp"
namespace companion::diagnostic {void Stage(Task,StageCode){}void Sample(Task){}void Fault(Task,FaultCode,int32_t){}void CountTick(){}void CountHeartbeat(bool){}}
struct lv_obj_t{};
constexpr int LV_OBJ_FLAG_HIDDEN=1,LV_STATE_DISABLED=2;
void lv_obj_add_flag(lv_obj_t*,int){} void lv_obj_remove_flag(lv_obj_t*,int){}
void lv_obj_add_state(lv_obj_t*,int){} void lv_obj_remove_state(lv_obj_t*,int){}
lv_obj_t* lv_obj_get_child(lv_obj_t* p,int){return p;}
void lv_obj_set_width(lv_obj_t*,int){} void lv_obj_set_height(lv_obj_t*,int){}
void lv_label_set_text(lv_obj_t*,const char*){}
struct LvglLockGuard{};
std::atomic<uint64_t> clock_ms{500};long long esp_timer_get_time(){return clock_ms.load()*1000;}
struct NetworkInterface{};struct Board{static Board& GetInstance(){static Board b;return b;}NetworkInterface* GetNetwork(){return nullptr;}void* GetAudioCodec(){return nullptr;}};
void esp_wifi_connect(){}
namespace companion {
enum class AudioOwner{None,Mira};AudioOwner CurrentAudioOwner(){return AudioOwner::None;}bool AcquireAudio(AudioOwner){return true;}void ReleaseAudio(AudioOwner){}
struct AuthorizedGrant{std::string uri,device,credential,request;bool host_audio_qualified=false;uint64_t expires_at_ms=0;bool numeric_pairing=false;};
std::mutex grant_mutex;std::optional<AuthorizedGrant> pending_grant;
std::atomic<bool> diagnostic_requested{false};
std::atomic<bool> confirm_requested{false},reject_requested{false},pair_requested{false},connect_requested{false},listen_requested{false},mute_requested{false},connected{true};
bool StartSavedWifi(){return false;}bool StageAuthorizedGrant(AuthorizedGrant){return false;}
struct NumericUsbRequest{std::string request;uint32_t boot_nonce=1;};
enum class NumericUsbRead{None,Accepted,Rejected};enum class ProvisioningRead{None,Accepted,Rejected};
struct ProvisioningFields{std::string uri,device,credential,request;void Clear(){}};
struct Provisioner{const char* ExportMessage(){return "USB flush unconfirmed - check host capture";}bool ExportDiagnostics(bool){return false;}bool Close(){return true;}bool Active(){return false;}bool Numeric(){return true;}bool Open(){return false;}NumericUsbRead PollNumeric(NumericUsbRequest&){return NumericUsbRead::None;}ProvisioningRead Poll(ProvisioningFields&){return ProvisioningRead::None;}bool CompleteAccepted(bool,const std::string&){return false;}};
struct NumericGrant{std::string request,credential;uint64_t expires_at_ms=0;bool audio_requested=false;};
struct NumericPairing{
 std::mutex mutex;std::condition_variable condition;bool block=false,entered=false,released=false;
 void Tick(uint64_t){}bool Active(uint64_t){return true;}bool Start(NetworkInterface&,const std::string&,uint32_t,uint64_t){return false;}
 void Cancel(){std::unique_lock<std::mutex> lock(mutex);if(block){entered=true;condition.notify_all();condition.wait(lock,[&]{return released;});}}
 bool Confirm(uint64_t){return false;}bool TakeGrant(NumericGrant&){return false;}bool Staged(){return false;}bool Confirmed(){return true;}bool AudioRequested(){return true;}bool ConnectionReady(){return false;}std::string Code(){return {};}
};
struct ConnectGate{void Cancel(){}void Request(bool){}bool Ready(bool){return false;}};
struct Consent{void Cancel(){}bool Arm(const std::string&,uint64_t,bool,bool,uint64_t){return false;}bool RequestConnection(bool){return false;}bool Take(uint64_t,bool){return false;}};
struct Tasks{std::atomic<bool> active{true};unsigned interrupted=0,stops=0;bool IsActive(){return active;}bool IsStopped(){return stops>0;}void Interrupt(uint32_t){++interrupted;}bool Stop(){++stops;return true;}template<class A,class B>bool Start(void*,A,B){return false;}};
class CompanionSocket{
public:enum class State{Disconnected,AwaitingBegin,ConsentRequired,Arming,Listening,Speaking,Muted,Fault};
 Tasks& tasks_;std::atomic<State> state_{State::Speaking};std::atomic<uint32_t> epoch_{1};std::atomic<bool> consent_{true};std::atomic<uint64_t> last_heartbeat_ms_{0};unsigned heartbeats=0,closes=0;
 explicit CompanionSocket(Tasks& t):tasks_(t){}bool Text(const std::string&){++heartbeats;return true;}State GetState(){return state_;}bool PhysicalConsent(){return false;}bool SendAudio(const uint8_t*,size_t){return false;}bool Connect(NetworkInterface&,const std::string&,const std::string&,const std::string&,const std::string&){return false;}
 void Close(){++closes;state_=State::Disconnected;epoch_=0;}void Tick();
};
static uint64_t ms(){return clock_ms;}
ACTUAL_SOCKET_TICK
struct Runtime{Provisioner provisioner;NumericPairing numeric_pairing;NumericUsbRequest numeric_request;uint64_t numeric_until=120000;bool numeric_connected=true;Tasks tasks;CompanionSocket socket{tasks};bool wifi_started=true,grant_connected=true;ConnectGate connect_gate;Consent matching_consent;PairUiAction pair_ui;lv_obj_t ui;lv_obj_t* status=&ui;lv_obj_t* diagnostic_button=&ui;lv_obj_t *pair_button=&ui,*listen_button=&ui,*connect_button=&ui,*mute_button=&ui,*confirm_button=&ui,*reject_button=&ui;const char* message="Speaking";};
Runtime& runtime(){static Runtime state;return state;}
constexpr bool STACKCHAN_COMPANION_TRUSTED_USB_PROVISIONING=false;
ACTUAL_TICK_VIEW
}
class Page{public:std::atomic<int> action_{0};bool exit_pending_=false;unsigned closes=0;bool StopResources(){return true;}void close(){++closes;}
ACTUAL_PAGE_BACK
ACTUAL_APP_RUNNING
};
int main(){
 auto& state=companion::runtime();Page page;
 page.onRunning();assert(state.socket.heartbeats==1 && page.closes==0);
 // Exact production stopped callback, invoked at a synthetic codec boundary.
 using namespace companion;auto stopped=ACTUAL_STOPPED_CALLBACK;
 state.tasks.active=false;state.numeric_pairing.block=true;stopped(1,false);
 std::atomic<bool> returned{false};std::thread main_loop([&]{page.onRunning();returned=true;});
 {std::unique_lock<std::mutex> lock(state.numeric_pairing.mutex);assert(state.numeric_pairing.condition.wait_for(lock,std::chrono::seconds(1),[&]{return state.numeric_pairing.entered;}));}
 clock_ms=3000;assert(!returned && state.socket.heartbeats==1 && state.socket.closes==0 && page.closes==0);
 {std::lock_guard<std::mutex> lock(state.numeric_pairing.mutex);state.numeric_pairing.released=true;}state.numeric_pairing.condition.notify_all();main_loop.join();
 assert(returned && state.socket.heartbeats==1 && state.socket.closes==1 && state.tasks.stops==1 && page.closes==0);
 assert(std::string(state.message)=="Mira companion\nMuted - connection closed");
 clock_ms=3500;page.onRunning();assert(state.socket.heartbeats==1 && page.closes==0);
 // Positive navigation control verifies the actual Back method is exercised.
 page.action_=999;page.onRunning();assert(page.closes==1);
}
