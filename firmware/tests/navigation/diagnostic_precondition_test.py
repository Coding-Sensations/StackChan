"""Actual diagnostic run body, real station authority, inert SDK/HTTP boundaries."""
from pathlib import Path
import subprocess,sys,tempfile

root=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(__file__).resolve().parents[3]
base=root/'firmware/main/hal/utils/network_diagnostics'
source=(base/'network_diagnostics.cpp').read_text()
header=(base/'network_diagnostics.h').read_text()
def extract(text,signature):
    start=text.index(signature);begin=text.index('{',start);depth=1;end=begin+1
    while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
types=header[header.index('enum class Status'):header.index('// Tasks retain')]
state=extract(source,'struct NetworkDiagnostics::State')+';'
body=extract(source,'void NetworkDiagnostics::run(')
fixture=r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include <cstdio>
#include "companion/station_authority.hpp"
constexpr int ESP_OK=0;constexpr const char* CONFIG_OTA_URL="https://fixture.invalid";
namespace Lang {constexpr const char* CODE="en-US";}
struct Settings {Settings(const char*,bool){}std::string GetString(const char*){return {};}};
struct SystemInfo {static std::string GetMacAddress(){return "SYNTHETIC";}static std::string GetUserAgent(){return "fixture";}};
struct Board {static Board& GetInstance(){static Board x;return x;}std::string GetUuid(){return "fixture";}std::string GetSystemInfoJson(){return "{}";}};
namespace secret_logic {std::string get_server_url(){return "https://fixture.invalid";}std::string generate_auth_token(){return "synthetic-not-a-device-token";}}
struct WifiManagerConfig {std::string ssid_prefix;const char* language=nullptr;};
struct WifiManager {bool connected=false,initialized=true,connect_on_start=false,init_ok=true;int starts=0,initializes=0;
 static WifiManager& GetInstance(){static WifiManager x;return x;}bool IsConnected(){return connected;}bool IsInitialized(){return initialized;}
 bool Initialize(const WifiManagerConfig&){++initializes;initialized=init_ok;return init_ok;}
 void StartStation(){++starts;if(companion::network::Authority().Current()==companion::network::Owner::Vendor&&companion::network::Authority().CanAcquire(companion::network::Owner::Vendor))connected=connect_on_start;}
};
struct SsidManager {bool saved=true;static SsidManager& GetInstance(){static SsidManager x;return x;}std::vector<int> GetSsidList(){return saved?std::vector<int>{1}:std::vector<int>{};}};
struct wifi_ap_record_t {unsigned char ssid[33]="SYNTHETIC";};struct esp_netif_ip_info_t {int ip=0;};
int esp_wifi_sta_get_ap_info(wifi_ap_record_t*){return ESP_OK;}void* esp_netif_get_handle_from_ifkey(const char*){return nullptr;}
int esp_netif_get_ip_info(void*,esp_netif_ip_info_t*){return ESP_OK;}void esp_ip4addr_ntoa(int*,char*,unsigned){}
int64_t now=0;std::function<void()> on_delay;int64_t esp_timer_get_time(){return now;}
int pdMS_TO_TICKS(int x){return x;}void vTaskDelay(int ms){now+=int64_t(ms)*1000;if(on_delay)on_delay();}
struct HAL {int clocks=0;void startSntp(){++clocks;}} hal;HAL& GetHAL(){return hal;}
namespace network_diagnostics {
'''
support=r'''
std::mutex WifiStartupMutex;bool WifiProvisioningActive=false;int probes=0;std::function<void()> on_probe;
using Headers=std::vector<std::pair<std::string,std::string>>;
std::string httpUrl(std::string x){return x;}
ServiceResult probe(const std::string&,const Headers&,const std::string&,bool,const std::atomic_bool&){++probes;if(on_probe)on_probe();return {Status::Available,"synthetic endpoint reached",200};}
class NetworkDiagnostics {public:struct State;static void run(const std::shared_ptr<State>&);};
'''
checks=r'''
}
void clear(){using namespace companion::network;auto& a=Authority();auto current=a.Current();if(current!=Owner::None){a.RequestStop(current);assert(a.FinishStop(current,true));}
 WifiManager::GetInstance()=WifiManager{};SsidManager::GetInstance().saved=true;network_diagnostics::WifiProvisioningActive=false;network_diagnostics::probes=0;on_delay={};network_diagnostics::on_probe={};now=0;hal.clocks=0;}
auto state(){return std::make_shared<network_diagnostics::NetworkDiagnostics::State>();}
void skipped(const std::shared_ptr<network_diagnostics::NetworkDiagnostics::State>& s){
 using namespace network_diagnostics;assert(s->results.xiaozhi.detail.find("Not tested:")==0);assert(s->results.m5stack.detail.find("Not tested:")==0);assert(s->results.xiaozhi.http_status==0&&s->results.m5stack.http_status==0);
 #ifdef HAS_NOT_TESTED
 assert(s->results.xiaozhi.status==Status::NotTested&&s->results.m5stack.status==Status::NotTested);
 #endif
}
int main(){using namespace companion::network;using network_diagnostics::NetworkDiagnostics;
 clear();auto none=state();NetworkDiagnostics::run(none);skipped(none);assert(!WifiManager::GetInstance().starts&&!network_diagnostics::probes);assert(Authority().Current()==Owner::None);
 clear();assert(Authority().Acquire(Owner::Saved));auto saved=state();NetworkDiagnostics::run(saved);skipped(saved);assert(!WifiManager::GetInstance().starts&&!network_diagnostics::probes);assert(Authority().Current()==Owner::Saved);
 clear();assert(Authority().Acquire(Owner::Vendor));Authority().RequestStop(Owner::Vendor);auto stopping=state();NetworkDiagnostics::run(stopping);skipped(stopping);assert(!WifiManager::GetInstance().starts&&!network_diagnostics::probes);
 clear();assert(Authority().Acquire(Owner::Vendor));network_diagnostics::WifiProvisioningActive=true;auto setup=state();NetworkDiagnostics::run(setup);skipped(setup);assert(!network_diagnostics::probes);
 clear();assert(Authority().Acquire(Owner::Vendor));SsidManager::GetInstance().saved=false;auto no_saved=state();NetworkDiagnostics::run(no_saved);skipped(no_saved);assert(!WifiManager::GetInstance().starts&&!network_diagnostics::probes);
 clear();assert(Authority().Acquire(Owner::Vendor));auto timeout=state();NetworkDiagnostics::run(timeout);skipped(timeout);assert(WifiManager::GetInstance().starts==1&&!network_diagnostics::probes);
 clear();assert(Authority().Acquire(Owner::Vendor));WifiManager::GetInstance().initialized=false;WifiManager::GetInstance().init_ok=false;auto init=state();NetworkDiagnostics::run(init);skipped(init);assert(!network_diagnostics::probes);
 clear();assert(Authority().Acquire(Owner::Vendor));WifiManager::GetInstance().connect_on_start=true;auto valid=state();NetworkDiagnostics::run(valid);assert(network_diagnostics::probes==2);assert(valid->results.xiaozhi.status==network_diagnostics::Status::Available);
 clear();assert(Authority().Acquire(Owner::Vendor));on_delay=[](){auto& a=Authority();a.RequestStop(Owner::Vendor);assert(a.FinishStop(Owner::Vendor,true));assert(a.Acquire(Owner::Saved));on_delay={};};auto lost=state();NetworkDiagnostics::run(lost);skipped(lost);assert(!network_diagnostics::probes&&Authority().Current()==Owner::Saved);
 clear();assert(Authority().Acquire(Owner::Vendor));WifiManager::GetInstance().connected=true;network_diagnostics::on_probe=[](){auto& a=Authority();a.RequestStop(Owner::Vendor);assert(a.FinishStop(Owner::Vendor,true));assert(a.Acquire(Owner::Vendor));network_diagnostics::on_probe={};};auto replaced=state();NetworkDiagnostics::run(replaced);skipped(replaced);assert(network_diagnostics::probes==1);
 clear();auto cancelled=state();cancelled->cancelled=true;NetworkDiagnostics::run(cancelled);assert(!WifiManager::GetInstance().starts&&!network_diagnostics::probes);
 puts("11 actual diagnostic precondition/generation paths passed; inert SDK and HTTP only");
}
'''
with tempfile.TemporaryDirectory(prefix='stackchan-diagnostic-preconditions-') as temp:
    p=Path(temp);(p/'test.cpp').write_text(fixture+types+support+state+'\n'+body+'\n'+checks)
    flags=['-DHAS_NOT_TESTED=1'] if 'NotTested' in types else []
    subprocess.run(['clang++','-std=c++17','-DSTACKCHAN_COMPANION_BOOT=1',*flags,'-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(root/'firmware/main'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
