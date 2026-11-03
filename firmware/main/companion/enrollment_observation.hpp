#pragma once
// Public diagnostic values only. No protocol strings, identities, credentials,
// headers, comparison codes, SSIDs, or raw exceptions are accepted by this API.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <ctime>
namespace companion::enrollment_observation {
enum class PairStage:uint32_t {None,Tap,StoredRecordPresent,CleanupRequired,WifiWait,WifiStartFailed,WifiDisconnected,PolicyRejected,RequestRejected,WebSocketFailed,HelloWriteFailed,HelloSent,Expired};
enum class TlsStage:uint32_t {None,Starting,RouteRejected,DateChecksUnavailable,ClockUnset,ContextBusy,InitFailed,ConfigRejected,ConnectFailed,SocketSetupFailed,WorkerStartFailed,Ready};
enum class WebSocketStage:uint32_t {None,TransportFailed,RequestWriteFailed,UpgradeRejected,UpgradeTimedOut,Ready};
inline const char* Label(PairStage s){switch(s){
case PairStage::None:return "not observed";case PairStage::Tap:return "physical tap";case PairStage::StoredRecordPresent:return "record exists";
case PairStage::CleanupRequired:return "cleanup required";case PairStage::WifiWait:return "waiting for Wi-Fi";case PairStage::WifiStartFailed:return "Wi-Fi start refused";
case PairStage::WifiDisconnected:return "Wi-Fi disconnected";case PairStage::PolicyRejected:return "policy rejected";case PairStage::RequestRejected:return "request preflight rejected";
case PairStage::WebSocketFailed:return "connection failed";case PairStage::HelloWriteFailed:return "hello write failed";case PairStage::HelloSent:return "hello sent";case PairStage::Expired:return "window expired";}return "unknown";}
inline const char* Label(TlsStage s){switch(s){
case TlsStage::None:return "not observed";case TlsStage::Starting:return "started";case TlsStage::RouteRejected:return "route rejected";
case TlsStage::DateChecksUnavailable:return "date checks unavailable";case TlsStage::ClockUnset:return "clock unset";case TlsStage::ContextBusy:return "context busy";
case TlsStage::InitFailed:return "initialization failed";case TlsStage::ConfigRejected:return "config rejected";case TlsStage::ConnectFailed:return "connect failed";
case TlsStage::SocketSetupFailed:return "socket setup failed";case TlsStage::WorkerStartFailed:return "worker start failed";case TlsStage::Ready:return "verified TLS ready";}return "unknown";}
inline const char* Label(WebSocketStage s){switch(s){case WebSocketStage::None:return "not observed";case WebSocketStage::TransportFailed:return "transport failed";
case WebSocketStage::RequestWriteFailed:return "upgrade write failed";case WebSocketStage::UpgradeRejected:return "upgrade rejected";
case WebSocketStage::UpgradeTimedOut:return "upgrade timed out";case WebSocketStage::Ready:return "upgrade accepted";}return "unknown";}
struct Snapshot {uint32_t generation=0,tap_clock=0,tls_clock=0;PairStage pair=PairStage::None;TlsStage tls=TlsStage::None;WebSocketStage web_socket=WebSocketStage::None;int32_t esp_error=0,tls_error=0;uint32_t verify_flags=0;};
class Trace {
    std::atomic<uint32_t> generation_{0},tap_clock_{0},tls_clock_{0},pair_{0},tls_{0},web_socket_{0},verify_flags_{0};
    std::atomic<int32_t> esp_error_{0},tls_error_{0};std::atomic<bool> transport_{false};
public:
    // Main task calls Begin only for an actual physical Remember action.
    // Cleanup/Cancel disables recording but never clears the retained result.
    void Begin(uint32_t now){transport_=false;tap_clock_=now;tls_clock_=0;pair_=uint32_t(PairStage::Tap);tls_=0;web_socket_=0;esp_error_=0;tls_error_=0;verify_flags_=0;auto next=generation_.load()+1;generation_=next?next:1;}
    void Pair(PairStage s){pair_=uint32_t(s);}
    void EnableTransport(bool enabled){transport_=enabled;}
    bool Recording()const{return transport_.load();}
    void Tls(TlsStage s,uint32_t now,int32_t esp=0,int32_t tls=0,uint32_t flags=0){if(!Recording())return;tls_clock_=now;esp_error_=esp;tls_error_=tls;verify_flags_=flags;tls_=uint32_t(s);}
    void WebSocket(WebSocketStage s){if(Recording())web_socket_=uint32_t(s);}
    // View is stopped-only. Atomics also prevent data races during shutdown.
    Snapshot Get()const{return {generation_.load(),tap_clock_.load(),tls_clock_.load(),PairStage(pair_.load()),TlsStage(tls_.load()),WebSocketStage(web_socket_.load()),esp_error_.load(),tls_error_.load(),verify_flags_.load()};}
};
inline Trace& State(){static Trace state;return state;}
inline uint32_t Clock(std::time_t now){return now>=0&&uint64_t(now)<=UINT32_MAX?uint32_t(now):0;}
inline void Utc(char (&text)[32],uint32_t seconds){if(!seconds){std::snprintf(text,sizeof(text),"unset / out of range");return;}std::time_t t=seconds;std::tm calendar{};if(!gmtime_r(&t,&calendar)||!std::strftime(text,sizeof(text),"%Y-%m-%d %H:%M:%SZ",&calendar))std::snprintf(text,sizeof(text),"clock unavailable");}
struct Current {uint32_t clock=0;bool associated=false;uint32_t address=0;};
inline void Overview(char* text,std::size_t size,const Snapshot& s,const Current& current){
    char now[32],tap[32];Utc(now,current.clock);Utc(tap,s.tap_clock);
    std::snprintf(text,size,"UTC %s\nWi-Fi %s | IP %u.%u.%u.%u\nLast Remember tap:\n%s\nPair: %s\nWebSocket: %s",now,current.associated?"connected":"not connected",unsigned(current.address>>24),unsigned((current.address>>16)&255),unsigned((current.address>>8)&255),unsigned(current.address&255),s.generation?tap:"not observed",Label(s.pair),Label(s.web_socket));
}
inline void TlsDetails(char* text,std::size_t size,const Snapshot& s){char clock[32];Utc(clock,s.tls_clock);std::snprintf(text,size,"TLS: %s\nClock at TLS:\n%s\nESP result: 0x%08x\nmbedTLS result: %d\nVerify flags: 0x%08x",Label(s.tls),s.tls==TlsStage::None?"not observed":clock,unsigned(s.esp_error),int(s.tls_error),unsigned(s.verify_flags));}
}
