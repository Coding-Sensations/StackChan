#pragma once
#include "companion/desk_camera_adapter.hpp"
#include <cstdint>
#include <array>
#include <memory>
#include "companion/camera_boot_context.hpp"
#include <esp_video_capture_clock_contract.h>
namespace companion::desk {
struct DriverCaptureObservation {
    int64_t seconds=0,microseconds=0;
    uint32_t flags=0,driver_sequence=0;
};
// Numeric parsing is useful for diagnostics/qualification, never itself trust.
inline bool ParseDriverCaptureNs(DriverCaptureObservation raw,uint64_t& ns) noexcept {
    ns=0;
    if(raw.seconds<0||raw.microseconds<0||raw.microseconds>=1000000)return false;
    const uint64_t sub=uint64_t(raw.microseconds)*1000;
    if(uint64_t(raw.seconds)>(UINT64_MAX-sub)/1000000000)return false;
    ns=uint64_t(raw.seconds)*1000000000+sub;return ns!=0;
}
struct ParsedCaptureDomain {
    uint64_t raw_capture_us=0,domain_capture_ms=0;
    uint32_t quantization_uncertainty_ns=1000000;
};
inline bool ParseCaptureDomain(DriverCaptureObservation raw,ParsedCaptureDomain& out) noexcept {
    out={};uint64_t ns=0;if(!ParseDriverCaptureNs(raw,ns))return false;
    out.raw_capture_us=ns/1000;out.domain_capture_ms=out.raw_capture_us/1000;
    return out.domain_capture_ms!=0;
}
class InstalledVideoCaptureClock;
class AttestedCaptureClock {
    CameraBinding binding_{};
    std::array<uint8_t,16> boot_identity_{};
    uint64_t captured_ns_=0;
    uint32_t driver_sequence_=0;
    friend class InstalledVideoCaptureClock;
public:
    AttestedCaptureClock()=default;
    bool valid() const noexcept {
        bool boot=false;for(auto b:boot_identity_)boot=boot||b!=0;
        return binding_.generation&&binding_.epoch&&boot&&captured_ns_&&captured_ns_%1000000==0;
    }
    CameraBinding binding() const noexcept {return binding_;}
    uint64_t captured_ns() const noexcept {return captured_ns_;}
    const std::array<uint8_t,16>& boot_identity() const noexcept {return boot_identity_;}
    uint32_t quantization_uncertainty_ns() const noexcept {return 1000000;}
    uint32_t driver_sequence() const noexcept {return driver_sequence_;}
    const char* clock_domain() const noexcept {return valid()?"esp-monotonic-ms-v1":"unqualified-v4l2-timeval";}
};
// Implement only from reviewed external hardware timing qualification; software
// EOF/MONOTONIC flags alone cannot satisfy this deployment-owned interface.
class QualifiedCaptureTimingAuthority {
public:
    virtual ~QualifiedCaptureTimingAuthority()=default;
    virtual bool Qualified(uint32_t driver_revision,const std::array<uint8_t,16>& boot,CameraBinding binding,DriverCaptureObservation raw)=0;
};
class InstalledVideoCaptureClock {
    DriverCaptureObservation last_{};
    bool have_observation_=false;
    std::shared_ptr<QualifiedCaptureTimingAuthority> external_timing_;
public:
    static constexpr bool CaptureOriginQualified() noexcept {return false;}
    uint32_t SelectedDriverRevision() const noexcept {return esp_video_capture_clock_contract_revision();}
    void SetExternalTimingAuthority(std::shared_ptr<QualifiedCaptureTimingAuthority> qualified) noexcept {external_timing_=std::move(qualified);}
    AttestedCaptureClock ObserveDequeued(CameraBinding binding,DriverCaptureObservation raw) noexcept {
        // Actual driver snapshot is retained before QBUF can mutate its struct.
        // No valid proof can be minted from flags/sequence/timeval alone.
        last_=raw;have_observation_=true;
        ParsedCaptureDomain parsed;const uint32_t revision=SelectedDriverRevision();
        // Runtime acceptance remains disabled in this nonactivating target.
        // Provider/context/selected-contract and external qualification hook are
        // wired; enabling acceptance requires genuine attended evidence review.
        if(!CaptureOriginQualified()||!external_timing_||revision!=ESP_VIDEO_CAPTURE_CLOCK_CONTRACT_REVISION||
           !raw.driver_sequence||!ParseCaptureDomain(raw,parsed)||
           !external_timing_->Qualified(revision,CameraProducerBootBytes(),binding,raw))return {};
        AttestedCaptureClock proof;proof.binding_=binding;proof.boot_identity_=CameraProducerBootBytes();
        proof.driver_sequence_=raw.driver_sequence;proof.captured_ns_=parsed.domain_capture_ms*1000000;return proof;
    }
    bool RawObservation(DriverCaptureObservation& out) const noexcept {out=last_;return have_observation_;}
    void ResetLocked() noexcept {last_={};have_observation_=false;}
};
}
