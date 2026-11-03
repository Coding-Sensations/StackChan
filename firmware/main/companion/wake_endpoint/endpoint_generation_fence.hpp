#pragma once
#include <cstdint>
#include <mutex>
namespace companion::wake_runtime {
// One qualified endpoint incarnation per CommonAudioAuthority generation,
// including across different factory handles and allocator-address reuse.
// Claim is custody admission, NEVER a consent/IO grant. Failed allocation after
// a claim consumes the generation too; retry requires a new actual owner lease.
class EndpointGenerationFence final {
    inline static std::mutex mutex_;
    inline static std::uint64_t highwater_=0;
public:
    static bool Claim(std::uint64_t generation)noexcept{
        std::lock_guard<std::mutex> lock(mutex_);
        if(!generation||generation<=highwater_)return false;
        highwater_=generation;return true;
    }
};
}
