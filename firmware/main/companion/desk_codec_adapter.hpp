#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include "esp_ae_rate_cvt.h"

namespace companion::desk {
struct AudioBinding {
    uint64_t generation=0, epoch=0;
    bool operator==(AudioBinding b) const noexcept {return generation==b.generation&&epoch==b.epoch;}
};
enum class InputPurpose { LocalWake, PhysicalListen };
inline void WipeSamples(int16_t* p,size_t n) noexcept {
    if(p){volatile int16_t* q=p;while(n--)*q++=0;}
}
// Owns only an ESP DSP handle. No codec construction, startup or audio IO.
class EspMonoResampler {
    esp_ae_rate_cvt_handle_t handle_=nullptr;
public:
    EspMonoResampler()=default;
    EspMonoResampler(const EspMonoResampler&)=delete;
    EspMonoResampler& operator=(const EspMonoResampler&)=delete;
    ~EspMonoResampler(){if(handle_)esp_ae_rate_cvt_close(handle_);}
    bool Prepare() {
        if(handle_)return esp_ae_rate_cvt_reset(handle_)==ESP_AE_ERR_OK;
        esp_ae_rate_cvt_cfg_t cfg{};
        cfg.src_rate=24000;cfg.dest_rate=16000;cfg.channel=1;
        cfg.bits_per_sample=16;cfg.complexity=2;cfg.perf_type=ESP_AE_RATE_CVT_PERF_TYPE_MEMORY;
        const auto result=esp_ae_rate_cvt_open(&cfg,&handle_);
        if(result!=ESP_AE_ERR_OK||!handle_){
            if(handle_)esp_ae_rate_cvt_close(handle_);
            handle_=nullptr;return false;
        }
        return true;
    }
    bool Reset(){return !handle_||esp_ae_rate_cvt_reset(handle_)==ESP_AE_ERR_OK;}
    bool Convert(int16_t* mono,int16_t* out,size_t capacity,size_t& written) {
        written=0;
        if(!handle_||!mono||!out||capacity!=512)return false;
        uint32_t required=0;
        if(esp_ae_rate_cvt_get_max_out_sample_num(handle_,480,&required)!=ESP_AE_ERR_OK||
           required==0||required>capacity)return false;
        uint32_t actual=static_cast<uint32_t>(capacity);
        if(esp_ae_rate_cvt_process(handle_,mono,480,out,&actual)!=ESP_AE_ERR_OK||
           actual==0||actual>capacity){WipeSamples(out,capacity);return false;}
        written=actual;WipeSamples(out+actual,capacity-actual);return true;
    }
};

// All methods run serialized on the existing audio-owner worker. Bridge must
// arbitrate ALL users, including DirectUSB, and retain a cleanup lease after
// revoking authorization. Current() validates purpose-specific physical consent.
// No implicit stop in destructor: checked cleanup must finish before destruction.
template<class Codec,class OwnerBridge,class Resampler=EspMonoResampler>
class BorrowedCodecInput {
    Codec& codec_;OwnerBridge& owner_;Resampler& resampler_;
    AudioBinding binding_{};
    InputPurpose purpose_=InputPurpose::LocalWake;
    bool leased_=false,ready_=false,stopped_=false;
    uint64_t last_generation_=0;
    std::vector<int16_t> raw_; // preallocated once; bounded 480 stereo points
    std::array<int16_t,480> mono_{};
    void Wipe() noexcept {WipeSamples(raw_.data(),raw_.size());WipeSamples(mono_.data(),mono_.size());}
public:
    BorrowedCodecInput(Codec& c,OwnerBridge& o,Resampler& r):codec_(c),owner_(o),resampler_(r),raw_(960,0){}
    BorrowedCodecInput(const BorrowedCodecInput&)=delete;
    BorrowedCodecInput& operator=(const BorrowedCodecInput&)=delete;
    ~BorrowedCodecInput(){Wipe();} // never invokes hardware or releases a failed stop
    bool HasLease()const noexcept{return leased_;}
    bool cleanup_pending() const noexcept {return leased_&&!ready_;}
    bool Begin(AudioBinding b,InputPurpose purpose) {
        if(leased_||!b.generation||!b.epoch||b.generation<=last_generation_||
           (purpose!=InputPurpose::LocalWake&&purpose!=InputPurpose::PhysicalListen))return false;
        if(!owner_.AcquireNoneToMira(b,purpose))return false;
        leased_=true;ready_=false;stopped_=false;binding_=b;purpose_=purpose;last_generation_=b.generation;
        if(!owner_.HoldsMiraLease(b)||!owner_.Current(b,purpose)||codec_.input_enabled()||codec_.output_enabled()||
           codec_.input_sample_rate()!=24000||
           (codec_.input_channels()!=1&&codec_.input_channels()!=2)||!resampler_.Prepare()) {
            // No input open attempted. Existing enabled IO belongs to another
            // producer and MUST NOT be stopped by this adapter.
            stopped_=true;Stop(b);return false;
        }
        // DSP preparation can block; consent must still hold before opening IO.
        if(!owner_.HoldsMiraLease(b)||!owner_.Current(b,purpose)) {
            stopped_=true;Stop(b);return false;
        }
        if(!codec_.EnableInputChecked(true)||!owner_.Current(b,purpose)) {Stop(b);return false;}
        ready_=true;return true;
    }
    bool Stop(AudioBinding b) {
        if(!leased_||!(b==binding_)||!owner_.HoldsMiraLease(b))return false;
        ready_=false;owner_.RevokeMira(b);Wipe();
        if(!stopped_){if(!codec_.EnableInputChecked(false))return false;stopped_=true;}
        if(!resampler_.Reset()||!owner_.ReleaseMira(b))return false;
        leased_=false;binding_={};return true;
    }
    bool Read16k(AudioBinding b,int16_t* out,size_t capacity,size_t& written) {
        written=0;
        const size_t wipe=capacity>512?512:capacity;
        auto fail=[&](){WipeSamples(out,wipe);Wipe();return false;};
        // Caller supplies separate writable storage; no alias with internals.
        if(!out||capacity!=512)return fail();
        WipeSamples(out,capacity);
        // A stale callback never stops a later lease.
        if(!leased_||!(b==binding_))return fail();
        if(!ready_||!owner_.HoldsMiraLease(b)||!owner_.Current(b,purpose_)){Stop(b);return fail();}
        const int channels=codec_.input_channels();
        if(codec_.input_sample_rate()!=24000||(channels!=1&&channels!=2)){Stop(b);return fail();}
        raw_.resize(480*static_cast<size_t>(channels));
        if(!codec_.InputDataChecked(raw_)||raw_.size()!=480*static_cast<size_t>(channels)||
           !owner_.Current(b,purpose_)){Stop(b);return fail();}
        for(size_t i=0;i<480;++i)mono_[i]=raw_[i*static_cast<size_t>(channels)];
        if(!resampler_.Convert(mono_.data(),out,capacity,written)||!owner_.Current(b,purpose_)) {
            written=0;Stop(b);return fail();
        }
        Wipe();return true;
    }
};
}
