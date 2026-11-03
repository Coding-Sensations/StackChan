#include "remembered_audio_worker.hpp"
#include "boot.h"
#include "audio_owner.hpp"
#include "reactive_avatar_service.hpp"
#include <board.h>
#include <esp_timer.h>
#include <esp_random.h>
#include <esp_wifi.h>
#include <ctime>
namespace companion {
namespace {
uint64_t Ms(){return esp_timer_get_time()/1000;}
uint64_t Wall(){auto value=std::time(nullptr);return value>1700000000?uint64_t(value):0;}
void Wipe(std::string& s){volatile char* p=s.empty()?nullptr:&s[0];for(size_t i=0;i<s.size();++i)p[i]=0;s.clear();}
std::string Request(){uint8_t b[16];esp_fill_random(b,sizeof(b));const char* h="0123456789abcdef";std::string out;for(auto v:b){out+=h[v>>4];out+=h[v&15];}return out;}
}
uint64_t RememberedAudioWorker::LocalClock(void*)noexcept{return Ms();}
void RememberedAudioWorker::RegisterPhysicalControl(const void* control,uint64_t view){std::lock_guard<std::mutex> lock(mutex_);physical_control_=control;physical_view_=view;physical_source_.Publish(control,view,generation_,socket_.Epoch());captured_gesture_={};captured_transport_=0;}
void RememberedAudioWorker::CapturePhysicalLocalProcessing(const void* control,uint64_t view,uint64_t transport){
    auto gesture=physical_source_.CapturePhysicalListen(control,view);if(!gesture)return;
    std::lock_guard<std::mutex> lock(mutex_);if(transport!=generation_.load()||!gate_.Live(transport))return;captured_gesture_=gesture;captured_transport_=transport;
}
RememberedAudioWorker::RememberedAudioWorker(RememberedCredentialVault& store):store_(store){socket_.SetConsentGuard([this]{return Live();});}
bool RememberedAudioWorker::StartWorker(bool approved){
    if(!approved)return false;
    if(started_)return true;
    if(xTaskCreate([](void* p){static_cast<RememberedAudioWorker*>(p)->Run();},"remembered_audio",8192,this,3,&task_)!=pdPASS)return false;
    started_=true;return true;
}
bool RememberedAudioWorker::Live()const{auto wall=Wall();return gate_.Live(generation_)&&wall&&wall<expiry_;}
void RememberedAudioWorker::InterruptImmediately(){
    gate_.Cancel();AudioLease captured;
    {std::lock_guard<std::mutex> lock(mutex_);captured=audio_lease_;RevokeAudio(captured);}
    StopReactiveAvatar(captured);socket_.InterruptImmediately();
}
bool RememberedAudioWorker::Queue(Command c){
    std::lock_guard<std::mutex> lock(mutex_);
    if(!started_||commands_.size()>=3){Wipe(c.secret);return false;}
    if(c.kind==CommandKind::Close){for(auto& item:commands_)Wipe(item.secret);commands_.clear();}
    commands_.push_back(std::move(c));return true;
}
void RememberedAudioWorker::EnsureSavedWifi(uint64_t g){
    // Owner accepts generation BEFORE enqueue. Worker never clears a later Mute.
    if(!gate_.Begin(g))return;
    generation_=g;
    if(!Queue({CommandKind::Wifi,g,{},{}}))Emit(RememberedWorkerEvent::Kind::Failure);
}
void RememberedAudioWorker::ConnectExisting(const ExistingRememberedPeer& p,uint64_t g){
    Command c{CommandKind::Connect,g,p,{}};
    if(!gate_.Live(g)||!store_.Credential(p.secret_handle,c.secret)){Emit(RememberedWorkerEvent::Kind::Failure,ReconnectFailure::Authentication);return;}
    expiry_=p.expires_unix;if(!Queue(std::move(c)))Emit(RememberedWorkerEvent::Kind::Failure);
}
void RememberedAudioWorker::RequestPhysicalConsent(uint64_t g){Command command{CommandKind::Listen,g,{},{}};{std::lock_guard<std::mutex> lock(mutex_);if(captured_transport_==g){command.gesture=captured_gesture_;captured_gesture_={};captured_transport_=0;}}if(!Queue(std::move(command)))Emit(RememberedWorkerEvent::Kind::Failure);}
void RememberedAudioWorker::CloseAndJoin(uint64_t g){
    std::lock_guard<std::mutex> lock(mutex_);
    if(!started_)return;
    // Idempotent owner polling does not create an unbounded cleanup queue.
    for(const auto& c:commands_)if(c.kind==CommandKind::Close&&c.generation==g)return;
    for(auto& c:commands_){Wipe(c.secret);}
    commands_.clear();commands_.push_back({CommandKind::Close,g,{},{}});
}
void RememberedAudioWorker::Emit(RememberedWorkerEvent::Kind kind,ReconnectFailure failure){
    std::lock_guard<std::mutex> lock(mutex_);
    if(events_.size()>=4){gate_.Cancel();return;} // fail closed; owner timeout/cleanup remains
    events_.push_back({kind,generation_,Ms(),failure});
}
bool RememberedAudioWorker::Poll(RememberedWorkerEvent& out){std::lock_guard<std::mutex> lock(mutex_);if(events_.empty())return false;out=events_.front();events_.pop_front();return true;}
void RememberedAudioWorker::Execute(Command& c){
    if(c.generation!=generation_){Wipe(c.secret);return;}
    if(c.kind==CommandKind::Close){
        AudioLease captured;{std::lock_guard<std::mutex> lock(mutex_);captured=audio_lease_;RevokeAudio(captured);}
        StopReactiveAvatar(captured);closing_=true;socket_.Close();Wipe(c.secret);return;
    }
    if(!gate_.Live(c.generation)){Wipe(c.secret);return;}
    if(c.kind==CommandKind::Wifi){if(!EnsureSavedNetwork())Emit(RememberedWorkerEvent::Kind::Failure);}
    else if(c.kind==CommandKind::Connect){
        auto network=Board::GetInstance().GetNetwork();bool ok=Live()&&network&&socket_.Connect(*network,c.peer.uri,c.peer.device,c.secret,Request());Wipe(c.secret);
        if(!ok||!Live()){socket_.InterruptImmediately();Emit(RememberedWorkerEvent::Kind::Failure,socket_.AuthenticationRejected()?ReconnectFailure::Authentication:ReconnectFailure::Transient);}
    }else if(c.kind==CommandKind::Listen){
        bool ok=Live()&&socket_.GetState()==CompanionSocket::State::ConsentRequired&&[this]{std::lock_guard<std::mutex> lock(mutex_);return AcquireAudio(AudioOwner::Mira,AudioPurpose::PhysicalListen,socket_.Epoch(),audio_lease_);}();
        if(ok){
            owns_audio_=true;codec_fault_=false;
            const AudioLease captured=audio_lease_;const uint64_t transport_generation=c.generation;
            const auto asset=wake_runtime::QualifiedAssetView();
            if(wake_runtime::kLiveBorrowedProcessingAvailable&&asset.qualification&&asset.bytes&&asset.size&&physical_source_.Validate(c.gesture,c.generation,socket_.Epoch(),wake_runtime::IntentPurpose::PhysicalListen)){
                ok=cpu_factory_.CreateQualified(captured.generation(),local_consent_,LocalClock,nullptr,asset.bytes,asset.size)&&tasks_.BindBorrowedCpu(cpu_factory_.Endpoint());
            }
            ok=ok&&tasks_.BindAudioLease(captured)&&BindReactiveAvatar(captured)&&
                tasks_.ConfigureLevelObserver(captured.generation(),[this,captured,transport_generation](uint64_t generation,uint32_t epoch,uint16_t peak,bool playback){
                    if(generation_.load()!=transport_generation || !gate_.Live(transport_generation) || !Live() || !CommonAudioAuthority().Current(captured))return;
                    ObserveReactiveAvatar(captured,generation,epoch,socket_.Epoch(),peak,playback,static_cast<uint32_t>(Ms()));
                })&&tasks_.Start(Board::GetInstance().GetAudioCodec(),[this,captured,transport_generation](const uint8_t* p,size_t n){
                    return generation_.load()==transport_generation&&Live()&&CommonAudioAuthority().Current(captured)&&socket_.SendAudio(p,n);
                },[this,captured](uint32_t,bool){StopReactiveAvatar(captured);codec_fault_=true;});
        }
        if(ok&&Live())ok=socket_.PhysicalConsent();else ok=false;
        if(ok&&cpu_factory_.Endpoint()){
            wake_runtime::ExplicitLocalConsent proof;
            ok=physical_source_.IssueBound(c.gesture,audio_lease_,c.generation,socket_.Epoch(),wake_runtime::IntentPurpose::PhysicalListen,proof)&&tasks_.StageBorrowedPhysicalProof(audio_lease_,socket_.Epoch(),proof);
        }
        if(!ok){StopReactiveAvatar(audio_lease_);socket_.InterruptImmediately();Emit(RememberedWorkerEvent::Kind::Failure);}
    }
}
void RememberedAudioWorker::Run(){
    for(;;){
        Command c{CommandKind::Close,0,{},{}};bool have=false;
        {std::lock_guard<std::mutex> lock(mutex_);if(!commands_.empty()){c=std::move(commands_.front());commands_.pop_front();have=true;}}
        if(have)Execute(c);
        {std::lock_guard<std::mutex> lock(mutex_);physical_source_.Publish(physical_control_,physical_view_,generation_,socket_.Epoch());}
        if(closing_){
            AudioLease captured;{std::lock_guard<std::mutex> lock(mutex_);captured=audio_lease_;}
            StopReactiveAvatar(captured);
            const bool joined=tasks_.Stop(0);ClosedReactiveAvatar(captured,joined);
            if(joined){
                bool cpu_closed=true;
                if(cpu_factory_.Endpoint()){if(!cpu_receipt_)cpu_receipt_=tasks_.DetachBorrowedCpuAfterStop(cpu_factory_.Endpoint());cpu_closed=cpu_receipt_&&cpu_factory_.DestroyDetached(cpu_receipt_);if(cpu_closed)cpu_receipt_={};}
                bool released=cpu_closed;
                if(owns_audio_&&cpu_closed){std::lock_guard<std::mutex> lock(mutex_);released=ReleaseAudio(audio_lease_,true);}
                if(released){owns_audio_=false;authenticated_=armed_=false;codec_fault_=false;closing_=false;Emit(RememberedWorkerEvent::Kind::Joined);}
                // On mismatched release, retain closing/custody and reach the
                // normal bounded retry delay below after the mutex is released.
            }
        }else if(generation_){
            socket_.Tick();if(Live()&&CommonAudioAuthority().Current(audio_lease_))UpdateReactiveMediaEpoch(audio_lease_,socket_.Epoch());auto state=socket_.GetState();using S=CompanionSocket::State;
            if(state==S::ConsentRequired&&!authenticated_&&Live()){authenticated_=true;Emit(RememberedWorkerEvent::Kind::Authenticated);}
            if((state==S::Listening||state==S::Speaking)&&!armed_&&Live()){armed_=true;Emit(RememberedWorkerEvent::Kind::Armed);}
            if(codec_fault_.exchange(false)||((authenticated_||armed_)&&(!Live()||state==S::Disconnected||state==S::Fault||state==S::Muted))){StopReactiveAvatar(audio_lease_);socket_.InterruptImmediately();authenticated_=armed_=false;Emit(RememberedWorkerEvent::Kind::Failure);}
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
}
