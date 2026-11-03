#pragma once
#include "desk_camera_adapter.hpp"
#include "camera_capture_clock.hpp"
#include <atomic>
#include <mutex>
#include <cstring>
namespace companion::desk {
// One mutex owns hardware calls, frame replacement and borrowed conversion.
// Cancellation is atomic so a blocked driver call cannot defer revocation.
class CameraProducerGate {
    std::atomic<uint64_t> active_{0};
    std::atomic<uint64_t> active_epoch_{0};
    uint64_t last_generation_=0;
    CameraBinding binding_{};
    bool custody_=false;
    uint64_t sequence_=0,last_capture_=0;
    TrustedFrameStamp stamp_{};
    uint32_t last_driver_sequence_=0;
    bool have_driver_sequence_=false;
    std::array<uint8_t,16> driver_boot_{};
public:
    std::mutex mutex;
    bool BeginLocked(CameraBinding b,bool opt_in) noexcept {
        if(!opt_in||!b.generation||!b.epoch||custody_||b.generation<=last_generation_)return false;
        binding_=b;last_generation_=b.generation;custody_=true;stamp_={};
        last_driver_sequence_=0;have_driver_sequence_=false;driver_boot_={};
        active_epoch_.store(b.epoch,std::memory_order_release);
        active_.store(b.generation,std::memory_order_release);return true;
    }
    bool Current(CameraBinding b) const noexcept {
        // epoch comparison is used only under mutex; atomic generation is also
        // usable by revocation without reading non-atomic binding state.
        return b==binding_&&active_.load(std::memory_order_acquire)==b.generation;
    }
    void Revoke(CameraBinding b) noexcept {
        // Generations are unique and never reused; an old callback cannot revoke new custody.
        uint64_t expected=b.generation;
        if(active_epoch_.load(std::memory_order_acquire)!=b.epoch)return;
        active_.compare_exchange_strong(expected,0,std::memory_order_acq_rel);
    }
    bool MatchesLocked(CameraBinding b) const noexcept {return custody_&&b==binding_;}
    bool CustodyLocked() const noexcept {return custody_;}
    uint64_t LastGenerationLocked() const noexcept {return last_generation_;}
    void ClosedLocked() noexcept {custody_=false;binding_={};stamp_={};}
    template<class Close> bool FinishStopLocked(CameraBinding b,Close close) {
        if(!MatchesLocked(b))return false;
        Revoke(b);stamp_={};
        if(!close())return false;
        ClosedLocked();return true;
    }
    void InvalidateFrameLocked() noexcept {stamp_={};}
    bool PublishLocked(CameraBinding b,uint64_t captured,bool trusted,bool canonical,uint64_t now) noexcept {
        stamp_={};
        if(!Current(b)||!trusted||!canonical||captured>now||now-captured>=500||
           (sequence_&&captured<last_capture_)||sequence_==UINT64_MAX)return false;
        last_capture_=captured;stamp_={++sequence_,captured,true,true};return true;
    }
    TrustedFrameStamp StampLocked() const noexcept {return stamp_;}
    bool PublishAttestedLocked(CameraBinding binding,const AttestedCaptureClock& clock,bool canonical,uint64_t now_ms) noexcept {
        if(!clock.valid()||!(clock.binding()==binding)||(have_driver_sequence_&&
           (clock.driver_sequence()<=last_driver_sequence_||clock.boot_identity()!=driver_boot_))){stamp_={};return false;}
        // Only an installed-source provider may construct a valid capture proof.
        if(!PublishLocked(binding,clock.captured_ns()/1000000,true,canonical,now_ms))return false;
        last_driver_sequence_=clock.driver_sequence();driver_boot_=clock.boot_identity();have_driver_sequence_=true;return true;
    }
    static bool Normalize(const uint8_t* src,size_t n,uint32_t format,bool swap,
                          uint8_t* dst,size_t capacity,int w,int h) noexcept {
        if(!src||!dst||w<1||h<1||w>4096||h>4096)return false;
        const uint32_t rgbx=CameraFourCC('R','G','B','R');
        const size_t bpp=(format==CameraRGB565||format==rgbx)?2:format==CameraRGB24?3:format==CameraGrey?1:0;
        if(!bpp)return false;
        const size_t required=size_t(w)*size_t(h)*bpp;
        if(n!=required||capacity<required)return false; // no hidden stride/padding
        // The former global swap is application postprocessing, not proof of
        // the raw mmap driver's byte order. Reject that configuration until
        // its exact raw contract is qualified. RGB565X explicitly declares BE.
        if(swap)return false;
        if(format==rgbx){for(size_t i=0;i<required;i+=2){dst[i]=src[i+1];dst[i+1]=src[i];}}
        else std::memcpy(dst,src,required);
        return true;
    }
};
}
