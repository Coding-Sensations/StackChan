"""Execute existing runtime status selection and new initial-view projection."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'firmware/main/companion/remembered_service.cc').read_text()
f=s[s.index('const char* RememberedStatusText(){'):s.index('bool RememberedRuntimeStopped(){')]
fixture=r'''
#include <cassert>
#include <string>
#include <cstdio>
#include "companion/companion_initial_status.hpp"
namespace companion {
enum class ReconnectState{Stopped,Network,Backoff,Connecting,ListenRequired,Arming,Armed,Cleanup};
enum class PeerReadStatus{Available,Revoked,Expired,ClockUnavailable,Unavailable};
struct Events{ReconnectState state=ReconnectState::Stopped;PeerReadStatus status=PeerReadStatus::Available;ReconnectState State(){return state;}PeerReadStatus Status(){return status;}};
struct Startup{companion::Events events;companion::Events& Events(){return events;}};
struct Service{Startup startup;};Service* service=nullptr;
ACTUAL
}
int main(){using namespace companion;
 assert(std::string(InitialViewStatus(false,RememberedStatusText())).find("disabled")!=std::string::npos);
 assert(std::string(InitialViewStatus(true,RememberedStatusText())).find("unavailable")!=std::string::npos);
 Service owner;service=&owner;
 struct Case{ReconnectState state;const char* expected;};
 for(auto c:{Case{ReconnectState::Stopped,"Muted - tap Sync"},Case{ReconnectState::Network,"connecting Wi-Fi"},Case{ReconnectState::Backoff,"retry waiting; muted"},Case{ReconnectState::Connecting,"authenticating; muted"},Case{ReconnectState::ListenRequired,"connected - tap Listen"},Case{ReconnectState::Arming,"waiting for host approval"},Case{ReconnectState::Armed,"audio active"},Case{ReconnectState::Cleanup,"cleanup pending"}}){owner.startup.events.state=c.state;assert(std::string(InitialViewStatus(true,RememberedStatusText())).find(c.expected)!=std::string::npos);}
 owner.startup.events.state=ReconnectState::Stopped;
 for(auto status:{PeerReadStatus::Revoked,PeerReadStatus::Expired,PeerReadStatus::ClockUnavailable,PeerReadStatus::Unavailable}){owner.startup.events.status=status;auto text=std::string(InitialViewStatus(true,RememberedStatusText()));assert(text.find("muted")!=std::string::npos&&text.find("connected")==std::string::npos);}
 puts("disabled/unavailable, eight runtime states and four stopped error states preserve truthful status");
}
'''.replace('ACTUAL',f)
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(root/'firmware/main'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
