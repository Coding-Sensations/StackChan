#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

// One producer and one consumer only. Never reset cursors from a control thread.
// Epoch-tagged samples make interruption safe even if a producer is in flight.
template <std::size_t Capacity = 7680> class PcmRing {
    struct Sample { int16_t value; uint32_t epoch; };
    std::array<Sample, Capacity> data_{};
    alignas(64) std::atomic<uint64_t> write_{0};
    alignas(64) std::atomic<uint64_t> read_{0};
public:
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    std::atomic<uint64_t> dropped{0}, stale{0}, underrun{0};
    std::size_t queued() const noexcept {
        auto r=read_.load(std::memory_order_acquire),w=write_.load(std::memory_order_acquire);
        auto n=w>=r?w-r:0;return n<Capacity?static_cast<std::size_t>(n):Capacity;
    }
    std::size_t push(uint32_t epoch, const int16_t* pcm, std::size_t frames) noexcept {
        auto w=write_.load(std::memory_order_relaxed);
        auto r=read_.load(std::memory_order_acquire);
        auto available=Capacity-static_cast<std::size_t>(w-r);
        auto n=frames<available?frames:available;
        for(std::size_t i=0;i<n;++i) data_[(w+i)%Capacity]={pcm[i],epoch};
        write_.store(w+n,std::memory_order_release);
        dropped.fetch_add(frames-n,std::memory_order_relaxed);
        return n;
    }
    std::size_t pull(uint32_t epoch, int16_t* pcm, std::size_t frames) noexcept {
        auto r=read_.load(std::memory_order_relaxed);
        auto w=write_.load(std::memory_order_acquire);
        std::size_t n=0;
        while(r<w && n<frames) {
            auto s=data_[r++%Capacity];
            if(s.epoch==epoch) pcm[n++]=s.value;
            else stale.fetch_add(1,std::memory_order_relaxed);
        }
        read_.store(r,std::memory_order_release);
        for(auto i=n;i<frames;++i) pcm[i]=0;
        underrun.fetch_add(frames-n,std::memory_order_relaxed);
        return n;
    }
};
