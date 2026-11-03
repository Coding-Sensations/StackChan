#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace stackchan::hal {
// A timestamp-indexed stereo buffer, one mixed producer and any number of readers.
// All shared payload and metadata are atomic; version validation never races plain
// float storage. SC ordering makes invalidation/payload/publication order explicit.
// A reader takes one snapshot attempt per frame, returning silence on contention.
class Timeline {
public:
    static constexpr uint32_t maxFrames=2048, capacity=32768, latency=4096;
    static constexpr uint32_t maxProducers=128;
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    Timeline(){leases[0].store(1);} // standalone synthetic producer, overwritten by driver Start
    void authorize(uint32_t producer,uint64_t token) noexcept {
        if(producer<maxProducers)leases[producer].store(token);
    }
    // Serialized non-RT control owner calls start/stop only at first/last writer.
    bool start() noexcept {
        const auto old=generation.load();
        if((old&1) || old>=UINT64_MAX-2) return false;
        generation.store(old+1); return true;
    }
    void stop() noexcept {
        const auto old=generation.load();
        if(old&1) generation.store(old+1);
    }
    bool write(uint64_t frame,uint32_t count,const float* pcm,uint32_t producer=0,uint64_t token=1) noexcept {
        return writeObserved(frame,count,pcm,producer,token,[]() noexcept {});
    }
    // The observer is a compile-time seam for a deterministic publication-race
    // test. Production instantiates only the empty inline observer above.
    template<class BeforePublish> bool writeObserved(uint64_t frame,uint32_t count,const float* pcm,uint32_t producer,uint64_t token,BeforePublish beforePublish) noexcept {
        if(!pcm || !count || count>maxFrames || frame>UINT64_MAX-count || producer>=maxProducers || !token || leases[producer].load()!=token) return false;
        // HAL normally serializes WriteMix. Reject overlapping writers rather than
        // blocking or allowing a data race if that contract is ever violated.
        if(writer.test_and_set(std::memory_order_acquire)) return false;
        struct Unlock { std::atomic_flag& flag; ~Unlock(){flag.clear(std::memory_order_release);} } unlock{writer};
        const auto epoch=generation.load();
        if(!(epoch&1)) return false;
        if(writerEpoch!=epoch){writerEpoch=epoch;lastEnd=0;hasWritten=false;}
        if(hasWritten && frame<lastEnd) return false;
        for(uint32_t i=0;i<count;++i){
            auto& s=slots[(frame+i)%capacity];
            const auto v=s.version.load();
            if(v>=UINT64_MAX-2) return false; // refuse wrap; no control-state mutation
            float stereo[2];
            for(unsigned c=0;c<2;++c){const float f=pcm[i*2+c];stereo[c]=std::isfinite(f)?std::fmax(-1.f,std::fmin(1.f,f)):0.f;}
            uint64_t packed;std::memcpy(&packed,stereo,sizeof(packed));
            s.version.store(v+1); // odd: invalid, before modifying any payload
            beforePublish();
            s.pcm.store(packed);s.frame.store(frame+i);s.epoch.store(epoch);
            s.producer.store(producer);s.lease.store(token);
            s.version.store(v+2);
        }
        lastEnd=frame+count;hasWritten=true;
        return generation.load()==epoch && leases[producer].load()==token;
    }
    void read(uint64_t frame,uint32_t count,float* pcm) const noexcept {
        if(!pcm || count>maxFrames) return;
        std::memset(pcm,0,count*2*sizeof(float));
        const auto epoch=generation.load();
        if(!(epoch&1) || frame>UINT64_MAX-count) return;
        for(uint32_t i=0;i<count;++i){
            const auto& s=slots[(frame+i)%capacity];
            const auto v=s.version.load();
            if(v&1)continue;
            const auto tag=s.frame.load(),g=s.epoch.load(),packed=s.pcm.load();
            const auto producer=s.producer.load(),token=s.lease.load();
            if(tag==frame+i && g==epoch && producer<maxProducers && token && leases[producer].load()==token && s.version.load()==v)
                std::memcpy(pcm+i*2,&packed,sizeof(packed));
        }
        // Last producer stop retires all in-flight reads from its generation.
        if(generation.load()!=epoch)std::memset(pcm,0,count*2*sizeof(float));
    }
private:
    struct Slot { std::atomic<uint64_t> version{0},pcm{0},frame{UINT64_MAX},epoch{0},producer{0},lease{0}; };
    std::array<Slot,capacity> slots{};
    std::array<std::atomic<uint64_t>,maxProducers> leases{};
    std::atomic<uint64_t> generation{0};
    std::atomic_flag writer=ATOMIC_FLAG_INIT;
    uint64_t writerEpoch=0,lastEnd=0; bool hasWritten=false; // writer flag only
};
}
