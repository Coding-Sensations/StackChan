"""Run reconciled network/shutdown production bodies against inert SDK boundaries."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

firmware = Path(__file__).resolve().parents[2]
root = firmware.parent


def source(path):
    ref = os.environ.get('STACKCHAN_RECONCILIATION_TEST_REF')
    if ref:
        return subprocess.check_output(['git', 'show', ref + ':firmware/' + path], cwd=root, text=True)
    return (firmware / path).read_text()


def extract(text, signature):
    start = text.index(signature)
    body = text.index('{', start)
    depth, end = 1, body + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


code = r'''
#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
namespace companion::network {
enum class Owner { None, Vendor, Saved };
struct AuthorityType { Owner owner=Owner::Vendor; bool allowed=true;
 Owner Current(){return owner;} bool CanAcquire(Owner){return allowed;} } authority;
AuthorityType& Authority(){return authority;}
}
bool home=false; namespace runtime_lifecycle { bool HomeRequested(){return home;} }
struct WifiManager {bool connected=false; static WifiManager& GetInstance(){static WifiManager w; return w;}
 bool IsConnected(){return connected;}std::string GetApSsid(){return "test";}std::string GetApWebUrl(){return "test";}};
enum class NetworkEvent {Scanning,Connecting,Connected,Disconnected,WifiConfigModeEnter,WifiConfigModeExit,
 ModemDetecting,ModemErrorNoSim,ModemErrorRegDenied,ModemErrorInitFailed,ModemErrorTimeout};
using Callback=std::function<void(NetworkEvent,const std::string&)>;
struct Board {Callback callback; int starts=0,event=0; static Board& GetInstance(){static Board b;return b;}
 void SetNetworkEventCallback(Callback cb){callback=std::move(cb);}
 void StartNetwork(){++starts;if(event==1)callback(NetworkEvent::Connected,"");if(event==2)callback(NetworkEvent::WifiConfigModeEnter,"");}};
namespace mclog {template<class... T>void tagInfo(T...){} }
namespace fmt {template<class... T>std::string format(T...){return "connecting";} }
std::string _tag="Network"; struct SetupRequest {};
struct Hal {uint32_t now=0;int clocks=0,setup=0;bool cancel=false;
 uint32_t millis(){return now;}void delay(int){now+=1000;if(cancel)home=true;assert(now<=70000);}
 void startSntp(){++clocks;}[[noreturn]]void requestAppSetup(){++setup;assert(!Board::GetInstance().callback);throw SetupRequest{};}
 void startNetwork(std::function<void(std::string_view)>);void forgetNativeNetwork();};
Hal* active;Hal& GetHAL(){return *active;}
''' + extract(source('main/hal/hal_network.cpp'), 'void Hal::startNetwork(') + '\n' + extract(source('main/hal/hal_network.cpp'), 'void Hal::forgetNativeNetwork()') + r'''
void network_cases(){
 using namespace companion::network;
 auto reset=[](){Board::GetInstance()=Board{};WifiManager::GetInstance().connected=false;authority=AuthorityType{};home=false;};
 reset();Hal denied;active=&denied;authority.owner=Owner::Saved;WifiManager::GetInstance().connected=true;
 denied.startNetwork({});assert(!Board::GetInstance().starts&&!denied.clocks&&!denied.setup);
 reset();Hal connected;active=&connected;WifiManager::GetInstance().connected=true;
 connected.startNetwork({});assert(!Board::GetInstance().starts&&connected.clocks==1);connected.forgetNativeNetwork();
 reset();Hal cancelled;active=&cancelled;cancelled.cancel=true;
 cancelled.startNetwork({});assert(home&&!cancelled.setup&&!cancelled.clocks&&!Board::GetInstance().callback);
 reset();Hal timeout;active=&timeout;bool redirected=false;
 try{timeout.startNetwork({});}catch(SetupRequest&){redirected=true;}
 assert(redirected&&timeout.setup==1&&!timeout.clocks&&timeout.now>=65000);
 reset();Hal setup;active=&setup;Board::GetInstance().event=2;redirected=false;
 try{setup.startNetwork({});}catch(SetupRequest&){redirected=true;}
 assert(redirected&&setup.setup==1&&!setup.clocks);
 reset();Hal success;active=&success;Board::GetInstance().event=1;
 success.startNetwork({});assert(success.clocks==1&&!success.setup&&!Board::GetInstance().callback);
}
'''

avatar = source('main/hal/hal_ws_avatar.cpp')
code += r'''
int local_destroyed=0,socket_destroyed=0,remote_stops=0;
struct Local {~Local(){++local_destroyed;}};struct Socket {~Socket(){++socket_destroyed;}};
struct Signal {int calls=0,last=-1;void disconnect(int id){++calls;last=id;}};
struct AvatarHal {Signal onWsCallResponse,onWsCallEnd;} avatar_hal;
AvatarHal& GetAvatarHAL(){return avatar_hal;}
#define GetHAL GetAvatarHAL
struct WebSocketAvatar {std::atomic_bool _stopping{false},_shutdown_started{false};
 int _call_response_connection=17,_call_end_connection=23;
 std::unique_ptr<Local> _local_client=std::make_unique<Local>();
 std::shared_ptr<Socket> _websocket=std::make_shared<Socket>();
 void stopRemoteTask(){++remote_stops;}
''' + extract(avatar, 'void Shutdown()') + r'''
};
#undef GetHAL
int main(){network_cases();WebSocketAvatar service;service.Shutdown();
 assert(service._stopping&&local_destroyed==1&&socket_destroyed==1&&remote_stops==1);
 assert(avatar_hal.onWsCallResponse.calls==1&&avatar_hal.onWsCallResponse.last==17);
 assert(avatar_hal.onWsCallEnd.calls==1&&avatar_hal.onWsCallEnd.last==23);
 service.Shutdown();assert(local_destroyed==1&&socket_destroyed==1&&remote_stops==1);
 assert(avatar_hal.onWsCallResponse.calls==1&&avatar_hal.onWsCallEnd.calls==1);
}
'''

with tempfile.TemporaryDirectory(prefix='stackchan-reconciliation-network-') as temporary:
    directory = Path(temporary)
    cpp, exe = directory / 'network.cpp', directory / 'network'
    cpp.write_text(code)
    subprocess.run([shutil.which('clang++') or 'c++', '-std=c++17', '-fsanitize=address,undefined',
                    str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Production network owner/cancel/deadline/setup cases and idempotent complete Avatar shutdown passed')
