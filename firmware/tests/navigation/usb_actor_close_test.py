"""Exercise actual USB actor mailbox publication under a queued Select/close race.
Runtime, scheduler and SDK boundaries are inert; no device I/O.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parents[2]/"main"
actor = (root / "companion/direct_usb_pcm/usb_pcm_actor.cc").read_text()
actor = "\n".join(line for line in actor.splitlines()
                  if not line.startswith("#include"))

fixture = r'''
#include <mutex>
#include <functional>
#include <string>
#include <cstdint>
#include <cassert>
#include <cstdio>
using TaskHandle_t=void*;
constexpr int pdPASS=1;
#define pdMS_TO_TICKS(x) (x)
int64_t esp_timer_get_time(){return 1;}
void vTaskDelay(int){throw 1;} // Run exactly one real loop iteration per call.
int xTaskCreatePinnedToCore(void(*)(void*),const char*,int,void*,int,void**,int){return 1;}
struct Board{
    static Board& GetInstance(){static Board board;return board;}
    void* GetAudioCodec(){return nullptr;}
};
namespace direct_usb_pcm {
enum class SetupIssue{None};
enum class RuntimeState{Closed,Provisioning};
struct Snapshot{};
struct Selection{
    uint32_t boot=0,epoch=1;
    uint16_t stream=1;
    uint64_t cutoff=100;
};
struct View{
    RuntimeState state=RuntimeState::Closed;
    SetupIssue issue=SetupIssue::None;
    std::string code,stamp;
    Snapshot audio;
    bool closed=true,configured=false;
};
void SelectFromPage();
void StopFromPage();
View ReadView();
std::function<void()> stop_hook,select_hook;
struct Runtime{
    bool closed=true,owned=false;
    Runtime()=default;
    Runtime(std::function<uint64_t()>,std::function<void*()>){ }
    bool PrepareSelected(uint32_t,uint32_t,uint16_t,uint64_t){
        // Models UI preemption after the actor entered a valid PrepareSelected,
        // before it acquires byte custody. No actual device is opened.
        if(select_hook)select_hook();
        owned=true;closed=false;return true;
    }
    void ConfirmDisplayedCode(const std::string&,const std::string&){}
    void Stop(){
        owned=false;closed=true;
        // UI Connect can run while this actor's Stop finishes, before publish.
        if(stop_hook)stop_hook();
    }
    void Tick(){}
    RuntimeState State(){return closed?RuntimeState::Closed:RuntimeState::Provisioning;}
    SetupIssue Issue(){return SetupIssue::None;}
    std::string Code(){return {};}
    std::string ConsentStamp(){return {};}
    Snapshot Audio(){return {};}
    bool Closed(){return closed;}
};
}
'''

checks = r'''
int main(){
    using namespace direct_usb_pcm;
    Runtime actor_runtime;
    runtime=&actor_runtime;task=(void*)1;
    view.configured=true;view.closed=false;actor_runtime.closed=false;
    command=Command::Stop;
    stop_hook=[] {SelectFromPage();};
    try{Run(nullptr);}catch(int){}
    stop_hook={};
    assert(command==Command::Select);
    const bool stale_closed_while_queued=ReadView().closed;

    bool parent_released=false;
    select_hook=[&] {
        // Exact stop/read receipt operations used by Page::RequestClose.
        StopFromPage();
        parent_released=ReadView().closed;
    };
    try{Run(nullptr);}catch(int){}
    select_hook={};
    std::printf("closed_while_select_queued=%d premature_parent_release=%d owned_after_select=%d\n",
                stale_closed_while_queued,parent_released,actor_runtime.owned);
    std::fflush(stdout);
    assert(!stale_closed_while_queued);
    assert(!parent_released);
    assert(actor_runtime.owned&&!actor_runtime.closed);
    assert(!ReadView().closed);

    // The queued Stop must still eventually supply a real closure receipt.
    try{Run(nullptr);}catch(int){}
    assert(actor_runtime.closed&&!actor_runtime.owned&&ReadView().closed);
    puts("Actual actor queued/in-flight Select and final Stop acknowledgement passed");
}
'''

with tempfile.TemporaryDirectory(prefix="nested-usb-actor-receipt-") as temporary:
    temporary = Path(temporary)
    source = temporary / "actor.cpp"
    source.write_text(fixture + actor + checks)
    binary = temporary / "actor"
    subprocess.run(["clang++", "-std=c++17", "-fsanitize=address,undefined",
                    str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
