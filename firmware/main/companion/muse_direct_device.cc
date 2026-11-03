// SPDX-License-Identifier: Apache-2.0
#include "muse_direct_device.h"
#include "muse_voice_task.h"
#include "muse_direct_live.h"
#include "muse_direct_activity.hpp"
#include "muse_direct_ble.h"
#include "muse_direct_session.hpp"
#include "muse_direct_credential_custody.hpp"
#include "muse_direct_api.h"
#include "provider_room_runtime.h"
#include <esp_timer.h>
#include <freertos/queue.h>
#include <memory>
#include <mutex>
#include <type_traits>
namespace companion::muse {
namespace {
uint64_t Now(){return uint64_t(esp_timer_get_time()/1000);}
std::atomic<bool> visible{false};
constexpr unsigned PacketQueueDepth=16;
struct Packet{uint32_t press=0;uint16_t length=0;uint8_t data[160]{};
 void Clear(){auto* p=reinterpret_cast<volatile unsigned char*>(this);for(size_t n=0;n<sizeof(*this);++n)p[n]=0;}
};
static_assert(std::is_trivially_copyable_v<Packet>);
struct PacketWiper{Packet& packet;~PacketWiper(){packet.Clear();}};
// Caller has joined the producer, or has not started it. Overwrite every ring
// slot, not just drained stack copies, before the queue allocation is released.
void WipePacketQueue(QueueHandle_t queue){Packet packet{};PacketWiper packet_wiper{packet};while(xQueueReceive(queue,&packet,0)==pdTRUE)packet.Clear();
 for(unsigned n=0;n<PacketQueueDepth;++n){packet.Clear();xQueueSend(queue,&packet,0);xQueueReceive(queue,&packet,0);}packet.Clear();
}
struct Window {
    ActivityCustody custody;
    CredentialLease credentials;
    PairingGrant grant;
    Secret<48> token;
    DirectBle ble;
    QueueHandle_t queue=nullptr;
    TaskHandle_t worker=nullptr;
    std::atomic<bool> revoked{false},done{false},started{false},busy{false},reaping{false};
    std::atomic<uint32_t> ticket{0};
    std::atomic<uint64_t> received{0},verify_version{0};
    bool Current(){return credentials.Current()&&visible.load()&&!revoked&&Now()<grant.expires_ms&&room::ProviderSelected(room::Provider::Muse)&&grant.enrollment_current&&grant.enrollment_current()&&grant.network_current&&grant.network_current()&&grant.storage_current&&grant.storage_current();}
    bool Quiescent(){return !busy||(received==verify_version&&uxQueueMessagesWaiting(queue)==0);}
    void Input(const uint8_t* data,size_t size){
        ++received;if(!Current()||busy){if(busy)revoked=true;return;}
        Packet packet{};PacketWiper packet_wiper{packet};if(!size||size>sizeof(packet.data)){revoked=true;return;}packet.length=size;std::memcpy(packet.data,data,size);
        if(xQueueSend(queue,&packet,0)!=pdTRUE)revoked=true;
    }
    ~Window(){token.Clear();if(queue){WipePacketQueue(queue);vQueueDelete(queue);}}
};
std::mutex mutex;
std::shared_ptr<Window> window;
class Verifier final:public DeviceVerifier {
    Window& owner_;DirectApiVerifier verifier_;
public:
    explicit Verifier(Window& owner):owner_(owner),verifier_([&owner]{return owner.Current()&&owner.Quiescent();}){}
    bool Verify(const DeviceRecord& record)override{
        owner_.verify_version=owner_.received.load();owner_.busy=true;
        if(uxQueueMessagesWaiting(owner_.queue))return false;
        return verifier_.Verify(record);
    }
};
void Run(void* context){
    auto* owner=static_cast<Window*>(context);
    {
    Verifier verifier(*owner);
    // The pairing owner alone is larger than the worker stack. Allocate its
    // fixed secret buffers on the heap and fail closed on allocation failure.
    std::unique_ptr<PairingSession> pairing(new(std::nothrow)PairingSession(owner->credentials,verifier));
    const bool opened=pairing&&owner->credentials.CanCreateNew()&&pairing->Open(owner->grant.node,owner->grant.device,owner->grant.mac,owner->grant.version,owner->token.Get(),owner->grant.expires_ms,Now,[owner]{return owner->Current()&&owner->Quiescent();},[owner](const std::string& value){return owner->ble.Send(value);});
    owner->token.Clear();
    std::string name="MuseGadget-";for(size_t n=owner->grant.mac.size()>8?9:0;n<owner->grant.mac.size();++n)if(owner->grant.mac[n]!=':')name+=char(owner->grant.mac[n]>='a'&&owner->grant.mac[n]<='f'?owner->grant.mac[n]-32:owner->grant.mac[n]);
    const bool radio=opened&&owner->ble.Start(name,[owner]{return owner->Current();},[owner](const uint8_t* data,size_t size){owner->Input(data,size);},[owner]{owner->revoked=true;});
    std::string assembled;unsigned next=0,total=0;
    while(radio&&owner->Current()&&pairing->Tick()){
        Packet packet{};PacketWiper packet_wiper{packet};
        if(xQueueReceive(owner->queue,&packet,pdMS_TO_TICKS(20))==pdTRUE){
            if(packet.press){pairing->PhysicalPress(packet.press);}
            else if(packet.length){
                bool complete=false;
                if(packet.data[0]!=0xfe){Wipe(assembled);assembled.assign(reinterpret_cast<const char*>(packet.data),packet.length);complete=true;}
                else{
                    if(packet.length<3||!packet.data[2]){owner->revoked=true;break;}
                    const unsigned index=packet.data[1],count=packet.data[2];
                    if(index==0){Wipe(assembled);next=0;total=count;}
                    if(index!=next||count!=total||assembled.size()+packet.length-3>8192){owner->revoked=true;break;}
                    assembled.append(reinterpret_cast<const char*>(packet.data+3),packet.length-3);++next;complete=next==total;
                }
                if(complete){const bool valid=pairing->Receive(assembled);Wipe(assembled);if(!valid){owner->revoked=true;break;}}
            }
        }
        owner->ticket=pairing->PhysicalTicket();
        if(pairing->State()==PairState::Stored){
            // auth_ok has been fully submitted to BLE. Give the phone a bounded
            // quiet interval, then stop; no cloud runtime follows pairing.
            const auto finish=Now()+1500;while(owner->Current()&&Now()<finish)vTaskDelay(pdMS_TO_TICKS(20));break;
        }
    }
    owner->ticket=0;owner->revoked=true;if(pairing)pairing->Close();Wipe(assembled);
    // The retaining worker owns this stack and attempts joined teardown. A
    // failed stop retains revoked BLE custody and this worker/context for retry.
    while(!owner->ble.StopAndJoin())vTaskDelay(pdMS_TO_TICKS(250));
    } // Destroy and wipe worker locals before the external join receipt.
    owner->done=true;for(;;)vTaskSuspend(nullptr);
}
void Reap(void* context){
    std::unique_ptr<std::shared_ptr<Window>> retained(static_cast<std::shared_ptr<Window>*>(context));
    auto owner=*retained;
    while(!owner->done)vTaskDelay(pdMS_TO_TICKS(20));
    if(owner->worker)vTaskDelete(owner->worker);
    owner->worker=nullptr;
    {std::lock_guard<std::mutex> lock(mutex);if(window==owner)window.reset();}
    retained.reset();owner.reset();
    // This is our cleanup dispatcher, not the SDK NimBLE task. It holds no
    // device/account references and clears all owned state before self-delete.
    vTaskDelete(nullptr);
}
}
void DeviceProviderVisible(bool value){DeviceCredentialsProviderVisible(value);visible=value;SessionWorkerProviderVisible(value);VoiceTaskProviderVisible(value);if(!value)StopDevicePairing();}
bool PrepareDevicePairing(const PairingGrant& grant,std::string_view token){
    if(!visible||!room::ProviderSelected(room::Provider::Muse)||!grant.enrollment_current||!grant.network_current||!grant.storage_current||!grant.enrollment_current()||!grant.network_current()||!grant.storage_current()||grant.expires_ms<=Now()||grant.expires_ms-Now()>600000||!Id(grant.node)||!Id(grant.device)||!Id(grant.version,32)||grant.mac.size()!=17||token.size()!=48||token.substr(0,5)!="mgst_")return false;
    for(size_t n=0;n<17;++n){const char c=grant.mac[n];if(n%3==2){if(c!=':')return false;}else if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;}
    std::string suffix;for(size_t n=9;n<17;++n)if(grant.mac[n]!=':'){char c=grant.mac[n];if(c>='A'&&c<='F')c+=32;suffix+=c;}
    std::string canonical=grant.mac;for(char& c:canonical)if(c>='A'&&c<='F')c+=32;
    if(grant.node!="homelink-"+suffix||grant.device!="hatch-link:"+canonical)return false;
    for(char c:token.substr(5))if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'))return false;
    if(std::string_view("AEIMQUYcgkosw048").find(token.back())==std::string_view::npos)return false;
    std::lock_guard<std::mutex> lock(mutex);if(window)return false;
    auto prepared=std::make_shared<Window>();if(!prepared->custody.Acquire()||!prepared->credentials.Acquire())return false;prepared->grant=grant;if(!prepared->token.Set(token))return false;window=std::move(prepared);return true;
}
bool DevicePairingPrepared(){std::lock_guard<std::mutex> lock(mutex);if(window&&!window->started&&!window->Current())window.reset();return window&&!window->started;}
bool StartReaperLocked(){
    if(!window||window->reaping)return bool(window);
    auto* retained=new(std::nothrow)std::shared_ptr<Window>(window);
    if(!retained)return false;
    window->reaping=true;
    if(xTaskCreate(Reap,"muse_pair_join",3072,retained,3,nullptr)!=pdPASS){window->reaping=false;delete retained;return false;}
    return true;
}
bool StartPreparedDevicePairing(){
    std::lock_guard<std::mutex> lock(mutex);if(!window||window->started||!window->Current())return false;
    window->queue=xQueueCreate(PacketQueueDepth,sizeof(Packet));if(!window->queue)return false;
    window->started=true;
    if(xTaskCreate(Run,"muse_pair",6144,window.get(),4,&window->worker)!=pdPASS){window->started=false;WipePacketQueue(window->queue);vQueueDelete(window->queue);window->queue=nullptr;return false;}
    if(!StartReaperLocked()){window->revoked=true;return false;}
    return true;
}
uint32_t DevicePhysicalTicket(){std::lock_guard<std::mutex> lock(mutex);return window&&window->started&&window->Current()?window->ticket.load():0;}
bool QueueDevicePhysicalPress(uint32_t captured){
    std::lock_guard<std::mutex> lock(mutex);if(!window||!captured||!window->Current()||captured!=window->ticket)return false;Packet packet{};PacketWiper packet_wiper{packet};packet.press=captured;return xQueueSend(window->queue,&packet,0)==pdTRUE;
}
void StopDevicePairing(){std::lock_guard<std::mutex> lock(mutex);if(window){window->revoked=true;if(!window->started)window.reset();else if(!window->reaping)StartReaperLocked();}}
bool DevicePairingCleanupComplete(){std::lock_guard<std::mutex> lock(mutex);return !window;}
}
