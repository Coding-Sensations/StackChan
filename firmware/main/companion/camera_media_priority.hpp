#pragma once
#include <cstdint>
#include <atomic>
#include <mutex>
namespace companion {
// Audio/control is selected before any new video fragment. An already-running
// existing transport write cannot be preempted; never hold a camera lock here.
struct CameraMediaPriority {
    std::mutex writer;
    std::atomic<uint32_t> critical_pending{0};
    struct Critical {
        CameraMediaPriority& p;std::unique_lock<std::mutex> lock;
        explicit Critical(CameraMediaPriority& owner):p(owner),lock(owner.writer,std::defer_lock){++p.critical_pending;lock.lock();}
        ~Critical(){--p.critical_pending;}
    };
    bool VideoAvailable() const noexcept {return critical_pending.load()==0;}
};
}
