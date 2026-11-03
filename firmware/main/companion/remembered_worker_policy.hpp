#pragma once
#include <atomic>
#include <cstdint>
namespace companion {
// Generation-scoped cancellation is shared by UI owner, transport worker and TLS
// callbacks. A late command cannot clear cancellation after physical Mute.
class RememberedWorkerGate {
    std::atomic<uint64_t> generation_{0};std::atomic<bool> cancelled_{true};
public:
    bool Begin(uint64_t generation){if(!generation||generation<=generation_.load())return false;generation_=generation;cancelled_=false;return true;}
    void Cancel(){cancelled_=true;}
    bool Live(uint64_t generation)const{return generation&&generation==generation_.load()&&!cancelled_.load();}
};
}
