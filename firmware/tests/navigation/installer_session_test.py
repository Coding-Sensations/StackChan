"""Execute real session/cleanup entry and native installer task bodies, no device IO."""
from pathlib import Path
import shutil
import subprocess
import tempfile

main = Path(__file__).resolve().parents[2] / 'main'


def extract(source, signature):
    start = source.index(signature)
    body = source.index('{', start)
    depth, end = 1, body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


session = '\n'.join(line for line in (main / 'apps/runtime_lifecycle/session.cc').read_text().splitlines()
                    if not line.startswith('#include'))
cleanup = extract((main / 'apps/runtime_lifecycle/cleanup.cc').read_text(), 'bool PollSourcesStopped(')
session_code = r'''
#include "apps/runtime_lifecycle/session.h"
#include "companion/station_authority.hpp"
#include <cassert>
#include <future>
#include <thread>
#include <vector>
constexpr int pdPASS=1,ESP_OK=0,ESP_ERR_WIFI_NOT_STARTED=-1,ESP_ERR_WIFI_NOT_INIT=-2,WIFI_MODE_NULL=0;
using wifi_mode_t=int;
#define pdMS_TO_TICKS(value) (value)
void vTaskDelay(int){}void vTaskDelete(void*){}
void(*pending)(void*)=nullptr;int created=0,network_stops=0;
int xTaskCreate(void(*task)(void*),const char*,int,void*,int,void*){++created;pending=task;return 1;}
namespace mooncake {
struct Ability{int getID(){return 0;}};
struct Extensions{std::vector<Ability*> getAllAbilityInstance(){return {};}void destroyAbility(int){}void updateAbilities(){}int getAbilityNum(){return 0;}};
struct Mooncake{Extensions extensions;Extensions* extensionManager(){return &extensions;}};
}
namespace network_setup{void EndOriginalOnboardingSession(){}void EndStockWifiProvisioningSession(){}}
struct Hal{bool stopBleServer(){return true;}bool stopEspNow(){return true;}void forgetNativeNetwork(){}};
Hal& GetHAL(){static Hal h;return h;}
struct Board{static Board& GetInstance(){static Board b;return b;}void SetNetworkEventCallback(void*){}};
struct WifiManager{static WifiManager& GetInstance(){static WifiManager w;return w;}
 void SetEventCallback(void*){}bool IsInitialized(){return true;}void StopConfigAp(){++network_stops;}void StopStation(){++network_stops;}};
int esp_wifi_get_mode(int* mode){*mode=0;return 0;}int esp_wifi_stop(){++network_stops;return 0;}int esp_wifi_set_mode(int){return 0;}
bool esp_sntp_enabled(){return false;}void esp_sntp_stop(){}
namespace companion::network {bool DrainStationEvents(){return true;}}
namespace runtime_lifecycle {
bool VendorNetworkAvailable(){return companion::network::Authority().Current()==companion::network::Owner::None;}
void QuietHardware(){}void ClearNativeSignals(){}
namespace {std::atomic<int> job{0};mooncake::Mooncake* old_shell=nullptr;}
}
''' + session + '\nnamespace runtime_lifecycle {\n' + cleanup + r'''
void reset_job(){assert(job!=1);job=0;old_shell=nullptr;}
}
void finish(){
 using namespace runtime_lifecycle;
 RequestHome();
 auto poll=[] {return Session().Poll(100,[]{},[]{return PollSourcesStopped(nullptr);},[]{return true;},[]{return true;});};
 if(!poll()){assert(pending);auto task=pending;pending=nullptr;task(nullptr);assert(poll());}
 reset_job();
}
int main(){using namespace runtime_lifecycle;using companion::network::Owner;
 assert(Begin()&&InstallerStarted()&&!InstallerStarted());
 RequestHome();int count=created;
 assert(!PollSourcesStopped(nullptr)&&created==count&&network_stops==0);
 assert(companion::network::Authority().Current()==Owner::Vendor&&!companion::network::Authority().Acquire(Owner::Saved));
 assert(!InstallerStarted()&&!InstallerJoined());InstallerExited();assert(InstallerJoined());
 finish();assert(network_stops>0&&companion::network::Authority().Current()==Owner::None);
 assert(Begin());RequestHome();int selected=0;
 assert(!CommitFirmwareInstall([&]{++selected;return true;}));assert(selected==0);finish();
 assert(Begin());std::promise<void> entered,release;auto wait=release.get_future();bool accepted=false;
 std::thread commit([&]{accepted=CommitFirmwareInstall([&]{++selected;entered.set_value();wait.wait();return true;});});
 entered.get_future().wait();std::atomic<bool> requested{false};
 std::thread stop([&]{requested=true;RequestHome();});while(!requested)std::this_thread::yield();
 release.set_value();commit.join();stop.join();assert(accepted&&selected==1&&HomeRequested());
 assert(!CommitFirmwareInstall([&]{++selected;return true;}));assert(selected==1);finish();
 // Preparation never changes persistent boot selection. Rejected admission
 // cannot discard the current session's staged candidate.
 assert(Begin());int boots=0,reboots=0;
 assert(StageFirmwareInstall([&]{++boots;return true;}));assert(boots==0&&!Begin());
 assert(!StageFirmwareInstall([]{assert(false);return true;}));
 assert(CommitFirmwareInstall([&]{assert(boots==1);++reboots;return true;}));assert(reboots==1);finish();
 assert(Begin());assert(StageFirmwareInstall([&]{++boots;return true;}));RequestHome();
 assert(!CommitFirmwareInstall([&]{++reboots;return true;}));assert(boots==1&&reboots==1);finish();
 assert(Begin());assert(StageFirmwareInstall([]{return false;}));
 assert(!CommitFirmwareInstall([&]{++reboots;return true;}));assert(reboots==1);
 assert(StageFirmwareInstall([&]{++boots;return true;}));DiscardFirmwareInstall();
 assert(CommitFirmwareInstall([&]{++reboots;return true;}));assert(boots==1&&reboots==2);finish();
}
'''

runtime = (main / 'apps/stock_services/firmware_install_runtime.cc').read_text()
native_code = r'''
#include "apps/stock_services/firmware_install_policy.h"
#include <atomic>
#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
bool home=false,installer=false,fail_create=false,upgrade_success=false,cancel_upgrade=false;
int destroyed_requests=0,upgrades=0,reboots=0;void(*task)(void*)=nullptr;void* argument=nullptr;
namespace runtime_lifecycle{
bool HomeRequested(){return home;}
bool InstallerStarted(){if(home||installer)return false;installer=true;return true;}
void InstallerExited(){assert(installer);installer=false;}
bool CommitFirmwareInstall(const std::function<bool()>& action){return !home&&action();}
}
namespace companion{enum class AudioOwner{Stock,Native};AudioOwner CurrentAudioOwner(){return AudioOwner::Native;}}
struct lv_obj_t{void* user=nullptr;bool alive=true;std::vector<lv_obj_t*> children;};
std::vector<std::unique_ptr<lv_obj_t>> nodes;
lv_obj_t* lv_obj_create(lv_obj_t* parent){auto node=std::make_unique<lv_obj_t>();auto* result=node.get();nodes.push_back(std::move(node));if(parent)parent->children.push_back(result);return result;}
lv_obj_t* lv_layer_top(){return nullptr;}lv_obj_t* lv_label_create(lv_obj_t* parent){return lv_obj_create(parent);}
void lv_obj_delete(lv_obj_t* node){assert(node&&node->alive);for(auto* child:node->children)if(child->alive)lv_obj_delete(child);node->alive=false;}
void lv_obj_set_size(lv_obj_t*,int,int){}void lv_obj_set_pos(lv_obj_t*,int,int){}void lv_obj_set_width(lv_obj_t*,int){}void lv_obj_set_height(lv_obj_t*,int){}
void lv_label_set_text(lv_obj_t* node,const char*){assert(node&&node->alive&&installer);}
struct LvglLockGuard{};
struct lv_event_t{void* user;lv_obj_t* target;};
void* lv_event_get_user_data(lv_event_t* e){return e->user;}void* lv_event_get_target(lv_event_t* e){return e->target;}void* lv_obj_get_user_data(lv_obj_t* p){return p->user;}
constexpr int pdPASS=1;
int xTaskCreate(void(*entry)(void*),const char*,int,void* data,int,void*){assert(installer&&!stock_services::FirmwareInstallAllowed());if(fail_create)return 0;task=entry;argument=data;return 1;}
void vTaskDelete(void*){assert(!installer&&destroyed_requests>0&&!stock_services::FirmwareInstallAllowed());}
struct Hal{void reboot(){++reboots;assert(!home);}};Hal& GetHAL(){static Hal h;return h;}
struct Ota{static bool Upgrade(const std::string&,std::function<void(int,size_t)> callback){++upgrades;assert(installer&&stock_services::FirmwareInstallAllowed());callback(10,0);if(cancel_upgrade)home=true;return upgrade_success;}};
struct Application{static Application& GetInstance(){static Application a;return a;}void Schedule(std::function<void()> f){f();}void UpgradeFirmware(const std::string&,const std::string&) {assert(false);}};
namespace stock_services{
enum class Feature{AppCenter,FirmwareCheck};std::optional<Feature> CurrentFeature(){return Feature::FirmwareCheck;}
std::atomic<bool> install_allowed{false},offer_pending{false};lv_obj_t* install_panel=nullptr;
struct Offer{std::string url,version;lv_obj_t* panel=nullptr;bool stock=false;~Offer(){++destroyed_requests;}};
void ResetFirmwareInstallUi();
''' + '\n'.join(extract(runtime, signature) for signature in [
    'bool ContextAllowed()', 'void NativeInstall(', 'void Confirm(', 'void ResetFirmwareInstallUi()',
    'bool FirmwareInstallAllowed()', 'void SetFirmwareInstallAllowed(']) + r'''
}
void begin(bool failure){
 using namespace stock_services;
 assert(!installer);home=false;fail_create=failure;destroyed_requests=0;task=nullptr;offer_pending=true;
 auto* request=new Offer{"https://example.invalid/fw.bin","test",lv_obj_create(nullptr),false};
 auto* button=lv_obj_create(nullptr);button->user=(void*)1;lv_event_t event{request,button};Confirm(&event);
}
void run(){assert(task&&installer);auto entry=task;task=nullptr;entry(argument);assert(!installer&&!stock_services::offer_pending&&destroyed_requests==1);stock_services::ResetFirmwareInstallUi();}
int main(){
 begin(true);assert(!installer&&!task&&destroyed_requests==1&&!stock_services::offer_pending);
 begin(false);home=true;run();assert(upgrades==0&&!reboots);
 begin(false);upgrade_success=false;run();assert(upgrades==1&&!reboots);
 begin(false);upgrade_success=true;cancel_upgrade=true;run();assert(upgrades==2&&!reboots);
 begin(false);cancel_upgrade=false;run();assert(upgrades==3&&reboots==1);
 for(const auto& node:nodes)if(node->alive)lv_obj_delete(node.get());
}
'''

hal_source = (main / 'hal/hal_ota.cpp').read_text()
hal_code = r'''
#define STACKCHAN_COMPANION_BOOT 1
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <string_view>
bool home=false,upgrade_ok=true,cancel_in_upgrade=false,cancel_at_commit=false,new_version=true;
uint32_t cancel_wait=0;int reboots=0;
constexpr int ESP_OK=0;using esp_err_t=int;
const char* esp_err_to_name(int){return "test error";}
namespace runtime_lifecycle{
bool HomeRequested(){return home;}
bool WaitCancelled(uint32_t ms){if(cancel_wait==ms)home=true;return home;}
bool CommitFirmwareInstall(const std::function<bool()>& action){if(cancel_at_commit)home=true;return !home&&action();}
}
namespace mclog {template<class...T>void tagError(T...){}template<class...T>void tagInfo(T...){} }
std::string_view _tag="test";
struct Ota {std::string url="https://example.invalid/fw.bin",version="test";
 int CheckVersion(){return 0;}bool HasNewVersion(){return new_version;}void MarkCurrentVersionValid(){}
 const std::string& GetFirmwareUrl(){return url;}const std::string& GetFirmwareVersion(){return version;}
 static bool Upgrade(const std::string&,std::function<void(int,size_t)> progress){if(cancel_in_upgrade)home=true;progress(50,2000);return upgrade_ok;}};
struct Hal{void reboot(){assert(!home);++reboots;}bool updateFirmware(std::function<void(std::string_view)>);};
''' + extract(hal_source, 'static bool RebootUnlessCancelled(') + '\n' + extract(hal_source, 'bool Hal::updateFirmware(') + r'''
int main(){Hal hal;auto log=[](std::string_view){assert(!home);};
 assert(hal.updateFirmware(log)&&reboots==1);
 upgrade_ok=false;assert(!hal.updateFirmware(log)&&reboots==2);
 cancel_in_upgrade=true;assert(!hal.updateFirmware(log)&&reboots==2);
 home=false;cancel_in_upgrade=false;cancel_wait=5000;assert(!hal.updateFirmware(log)&&reboots==2);
 home=false;cancel_wait=1000;upgrade_ok=true;assert(!hal.updateFirmware(log)&&reboots==2);
 home=false;cancel_wait=0;cancel_at_commit=true;assert(!hal.updateFirmware(log)&&reboots==2);
 home=false;cancel_at_commit=false;new_version=false;assert(hal.updateFirmware(log)&&reboots==2);
}
'''
stock_source = (main / 'companion/stock_application.cc').read_text()
stock_code = r'''
#include <cassert>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define pdMS_TO_TICKS(ms) (ms)
void vTaskDelay(int){}
bool home=false,pending_boot=false,selection_ok=true;int boots=0,reboots=0,waits=0,cancel_wait=0,stops=0,fail_stop=0;
std::string status;
namespace runtime_lifecycle{
bool HomeRequested(){return home;}
bool WaitCancelled(int){if(++waits==cancel_wait){home=true;pending_boot=false;}return home;}
void DiscardFirmwareInstall(){pending_boot=false;}
bool CommitFirmwareInstall(const std::function<bool()>& reboot){if(home)return false;bool selected=pending_boot;pending_boot=false;if(selected){if(!selection_ok)return false;++boots;}return reboot();}
}
namespace stock_services{bool FirmwareInstallAllowed(){return !home;}bool IsFirmwareImageUrl(const std::string&){return true;}void OfferFirmwareInstall(const std::string&,const std::string&){} }
namespace Lang{namespace Strings{const char* OTA_UPGRADE="ota";const char* UPGRADING="upgrade";const char* NEW_VERSION="new";const char* ERROR="error";const char* UPGRADE_FAILED="failed";}namespace Sounds{const char* OGG_UPGRADE="sound";const char* OGG_EXCLAMATION="sound";}}
enum PowerSaveLevel{PERFORMANCE,LOW_POWER};constexpr int kDeviceStateUpgrading=1;
struct Display{void SetStatus(const char* message){status=message;}void SetChatMessage(const char*,const char*){}};
struct Board{Display display;static Board& GetInstance(){static Board board;return board;}Display* GetDisplay(){return &display;}void SetPowerSaveLevel(int){}void* GetAudioCodec(){return nullptr;}};
struct Protocol{bool IsAudioChannelOpened(){return true;}void CloseAudioChannel(){}};
struct Audio{bool Stop(){return ++stops!=fail_stop;}bool Initialize(void*){return true;}bool Start(){return true;}};
struct Hal{void reboot(){assert(!home);++reboots;}};Hal& GetHAL(){static Hal h;return h;}
struct Ota{static bool Upgrade(const std::string&,std::function<void(int,size_t)> progress){progress(100,1000);pending_boot=true;return true;}};
struct Application{std::unique_ptr<Protocol> protocol_;Audio audio_service_;
 template<class...T>void Alert(T...){}void SetDeviceState(int){}void Schedule(const std::function<void()>& action){action();}
 void Reboot();bool UpgradeFirmware(const std::string&,const std::string&);};
''' + extract(stock_source, 'static void ReportAudioStartupFailure(') + '\n' + extract(stock_source, 'void Application::Reboot()') + '\n' + extract(stock_source, 'bool Application::UpgradeFirmware(') + r'''
void reset(){assert(!pending_boot);home=false;selection_ok=true;boots=reboots=waits=cancel_wait=stops=fail_stop=0;status.clear();}
int main(){Application app;
 reset();assert(app.UpgradeFirmware("https://example.invalid/fw.bin","test"));assert(boots==1&&reboots==1&&!pending_boot);
 for(int cancel:{3,4}){reset();cancel_wait=cancel;assert(!app.UpgradeFirmware("https://example.invalid/fw.bin","test"));assert(!boots&&!reboots&&!pending_boot);}
 reset();fail_stop=2;assert(!app.UpgradeFirmware("https://example.invalid/fw.bin","test"));assert(!boots&&!reboots&&!pending_boot&&status=="Audio cleanup pending");
 fail_stop=0;app.Reboot();assert(!boots&&reboots==1);
 reset();selection_ok=false;assert(!app.UpgradeFirmware("https://example.invalid/fw.bin","test"));assert(!boots&&!reboots&&!pending_boot&&status=="Firmware boot selection failed");
 reset();pending_boot=true;fail_stop=1;app.Reboot();assert(!pending_boot&&!boots&&!reboots);
}
'''
with tempfile.TemporaryDirectory(prefix='stackchan-installer-session-') as temporary:
    directory = Path(temporary)
    for name, code in [('session', session_code), ('native', native_code), ('hal', hal_code), ('stock', stock_code)]:
        cpp, exe = directory / (name + '.cpp'), directory / name
        cpp.write_text(code)
        subprocess.run([shutil.which('clang++') or 'c++', '-std=c++17', '-pthread',
                        '-fsanitize=address,undefined', '-I' + str(main), str(cpp), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True, timeout=30)
print('Production installer registration/failure/exit, retained network cleanup, HAL/stock cancellation, staged selection and Home/commit serialization passed')
