#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace stack_tools {
enum class ProximityState { Unknown, Far, Near };
enum class ProximityError { None, BusUnavailable, ReadFailed, WrongDevice, ForeignActive, ConfigureFailed, RestoreFailed };
struct ProximityTelemetry { bool valid=false,saturated=false;uint16_t raw=0,filtered=0;uint64_t observed_ms=0;ProximityState state=ProximityState::Unknown; };
// Driver owns only an address-0x23 device handle on the existing HAL bus.
// Raw proximity counts are not physical distance. Thresholds require calibration.
template<class Driver> class Ltr553Proximity {
    Driver& driver_;
    bool attached_=false,owned_=false;
    std::array<uint8_t,4> saved_{};
    uint16_t far_,near_;
    uint64_t start_=0;
    ProximityTelemetry telemetry_{};
    ProximityError error_=ProximityError::None;
    bool Restore() {
        bool ok=driver_.Write(0x81,0); // First stop our emitter.
        for(unsigned i=1;i<4;++i)ok=driver_.Write(static_cast<uint8_t>(0x81+i),saved_[i])&&ok;
        ok=driver_.Write(0x81,saved_[0])&&ok;
        return ok;
    }
public:
    Ltr553Proximity(Driver& driver,uint16_t far_threshold,uint16_t near_threshold):driver_(driver),far_(far_threshold),near_(near_threshold){}
    Ltr553Proximity(const Ltr553Proximity&)=delete;
    Ltr553Proximity& operator=(const Ltr553Proximity&)=delete;
    const ProximityTelemetry& Telemetry() const {return telemetry_;}
    ProximityError Error() const {return error_;}
    bool HasCustody() const {return attached_ || owned_;}
    bool Start(uint64_t now) {
        if(!Close())return false;
        error_=ProximityError::None;telemetry_={};
        if(far_>=near_ || near_>2047){error_=ProximityError::ConfigureFailed;return false;}
        if(!driver_.Attach()){error_=ProximityError::BusUnavailable;return false;}
        attached_=true;
        uint8_t identity[2]{};
        if(!driver_.Read(0x86,identity,2)){error_=ProximityError::ReadFailed;Close();return false;}
        if((identity[0]&0xf0)!=0x90 || identity[1]!=0x05){error_=ProximityError::WrongDevice;Close();return false;}
        if(!driver_.Read(0x81,saved_.data(),saved_.size())){error_=ProximityError::ReadFailed;Close();return false;}
        if(saved_[0]&3){error_=ProximityError::ForeignActive;Close();return false;}
        owned_=true;
        // 60kHz,25% duty,20mA,4 pulses,100ms interval; no ALS/interrupt changes.
        if(!driver_.Write(0x82,0x62) || !driver_.Write(0x83,4) || !driver_.Write(0x84,2) || !driver_.Write(0x81,0x23)) {
            error_=ProximityError::ConfigureFailed;Close();return false;
        }
        start_=now;return true;
    }
    bool Poll(uint64_t now) {
        if(!owned_ || now<start_ || now-start_<100)return false;
        uint8_t status=0;
        if(!driver_.Read(0x8c,&status,1)){telemetry_.valid=false;telemetry_.state=ProximityState::Unknown;error_=ProximityError::ReadFailed;return false;}
        if(!(status&1))return false;
        uint8_t bytes[2]{};
        if(!driver_.Read(0x8d,bytes,2)){telemetry_.valid=false;telemetry_.state=ProximityState::Unknown;error_=ProximityError::ReadFailed;return false;}
        const uint16_t raw=static_cast<uint16_t>(bytes[0]|((bytes[1]&7)<<8));
        const bool saturated=bytes[1]&0x80;
        const uint16_t filtered=telemetry_.valid ? static_cast<uint16_t>((3U*telemetry_.filtered+raw)/4U) : raw;
        telemetry_.raw=raw;telemetry_.filtered=filtered;telemetry_.observed_ms=now;telemetry_.saturated=saturated;
        telemetry_.valid=!saturated;
        if(saturated)telemetry_.state=ProximityState::Unknown;
        else if(filtered>=near_)telemetry_.state=ProximityState::Near;
        else if(filtered<=far_)telemetry_.state=ProximityState::Far;
        error_=ProximityError::None;return true;
    }
    ProximityTelemetry FreshTelemetry(uint64_t now,uint64_t max_age_ms) const {
        auto copy=telemetry_;
        if(now<copy.observed_ms || now-copy.observed_ms>max_age_ms){copy.valid=false;copy.state=ProximityState::Unknown;}
        return copy;
    }
    bool Close() {
        telemetry_.valid=false;telemetry_.state=ProximityState::Unknown;
        if(owned_ && !Restore()){error_=ProximityError::RestoreFailed;return false;}
        owned_=false;
        if(attached_ && !driver_.Detach()){error_=ProximityError::RestoreFailed;return false;}
        attached_=false;if(error_==ProximityError::RestoreFailed)error_=ProximityError::None;return true;
    }
};
}
