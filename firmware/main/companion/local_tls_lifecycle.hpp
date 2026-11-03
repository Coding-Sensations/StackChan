#pragma once
#include <memory>
#include <mutex>
#include <atomic>
#define COMPANION_TLS_WRITER_QUIESCENCE 1
namespace companion {
// Called by the owner, including after a peer has cleared its connected flag.
template<class T> void StopTransport(T* transport) {
    if(transport)transport->Disconnect();
}
template<class T> void ReleaseStoppedTransport(std::unique_ptr<T>& transport) {
    StopTransport(transport.get());
    transport.reset();
}
// Join before locking: receive-side ping replies can acquire the writer lock.
template<class T,class Mutex> void ReleaseStoppedTransport(std::unique_ptr<T>& transport,Mutex& writers) {
    StopTransport(transport.get());
    std::lock_guard<Mutex> guard(writers);
    transport.reset();
}
template<class T,class Mutex,class Operation> int WithLiveTransport(const std::unique_ptr<T>& transport,const std::atomic<bool>& live,Mutex& writers,Operation operation) {
    std::lock_guard<Mutex> guard(writers);
    if(!live.load() || !transport)return -1;
    return operation(*transport);
}
// Retry/progress budget is checked between SDK calls. A blocking SDK write
// may exceed it by its socket timeout; this is not an end-to-end deadline.
inline bool WriteMayContinue(bool active,long long started_us,long long now_us) {
    return active && now_us>=started_us && now_us-started_us<5000000;
}
}
