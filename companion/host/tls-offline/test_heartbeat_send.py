"""Offline: compile the actual Tick body with fake clock/transport/task boundary."""
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[3]

class HeartbeatSendTests(unittest.TestCase):
    def test_incomplete_writes_interrupt_actual_tick(self):
        source = (ROOT / 'firmware/main/companion/companion_socket.cc').read_text()
        start = source.index('void CompanionSocket::Tick()')
        end = source.index('void CompanionSocket::Close()', start)
        tick = source[start:end]
        shim = r'''#include <cassert>
#include <atomic>
#include <string>
#include "transport_write.hpp"
#include "diagnostics.h"
namespace companion::diagnostic {void Stage(Task,StageCode){}void Fault(Task,FaultCode,int32_t){}void CountHeartbeat(bool){}}
namespace companion {
static uint64_t ms(){return 500;}
struct Tasks { bool interrupted=false; bool IsActive(){return true;} void Interrupt(uint32_t){interrupted=true;} };
class CompanionSocket {
public:
 enum class State { ConsentRequired, Listening, Speaking, Muted };
 std::atomic<uint32_t> epoch_{1}; State state_=State::ConsentRequired;
 bool consent_=false;std::atomic<uint64_t> last_heartbeat_ms_{0};Tasks tasks_;
 int result=0;bool full=false;unsigned calls=0;
 bool Text(const std::string& text){++calls;return CompleteTransportWrite(full?int(text.size()):result,text.size())>0;}
 void Tick();
};
''' + tick + r'''}
int main(){
 for(int result : {0,1,-1,-7}) {
  companion::CompanionSocket socket;socket.result=result;socket.Tick();
  assert(socket.calls==1 && socket.tasks_.interrupted);
 }
 companion::CompanionSocket socket;socket.full=true;socket.Tick();
 assert(socket.calls==1 && !socket.tasks_.interrupted);
}
'''
        with tempfile.TemporaryDirectory(prefix='stackchan-heartbeat-offline-') as folder:
            code=pathlib.Path(folder)/'heartbeat.cpp';binary=pathlib.Path(folder)/'heartbeat'
            code.write_text(shim)
            subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-I',str(ROOT/'firmware/main/companion'),str(code),'-o',str(binary)],check=True,capture_output=True)
            subprocess.run([str(binary)],check=True,capture_output=True)

if __name__ == '__main__':unittest.main()
