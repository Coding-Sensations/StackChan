#pragma once
#include <cstddef>
#include <cstdint>

namespace companion {
// Pure control policy shared with the custom firmware. Hardware owner serializes
// calls and actually stops codec/DMA before acknowledging interrupt(). No NVS IO.
class AudioSession {
    uint32_t epoch_=0, mic_seq_=0, speaker_seq_=0;
    uint64_t deadline_ms_=0;
    bool begun_=false, acknowledged_=false, armed_=false, playing_=false;
public:
    bool begin(uint32_t epoch,uint64_t now_ms) noexcept {
        if(!epoch || epoch<=epoch_ || now_ms>UINT64_MAX-2000) return false;
        epoch_=epoch; mic_seq_=speaker_seq_=0; deadline_ms_=now_ms+2000;
        begun_=true; acknowledged_=armed_=playing_=false; return true;
    }
    bool acknowledge(uint32_t epoch) noexcept {
        if(!epoch || epoch!=epoch_ || !begun_) return false;
        acknowledged_=true; return true;
    }
    bool arm(uint32_t epoch,bool physical_consent) noexcept {
        if(epoch!=epoch_ || !acknowledged_ || !physical_consent) return false;
        armed_=true; return true;
    }
    bool heartbeat(uint32_t epoch,uint64_t now_ms) noexcept {
        if(!epoch || epoch!=epoch_ || !acknowledged_ || now_ms>=deadline_ms_ || now_ms>UINT64_MAX-2000) { mute(); return false; }
        deadline_ms_=now_ms+2000; return true;
    }
    bool tick(uint64_t now_ms) noexcept {
        if(now_ms>=deadline_ms_) mute();
        return armed_;
    }
    bool packet(uint32_t epoch,uint32_t sequence,std::size_t bytes,bool speaker,uint64_t now_ms) noexcept {
        if(!tick(now_ms) || epoch!=epoch_ || bytes==0 || bytes>4096 || (!speaker && playing_)) return false;
        auto& expected=speaker?speaker_seq_:mic_seq_;
        // No replay, gap, or wrap. Gap requires mute and a new session handshake.
        if(sequence!=expected || expected==UINT32_MAX) { mute(); return false; }
        ++expected; return true;
    }
    bool playing(uint32_t epoch,bool value) noexcept {
        if(epoch!=epoch_ || !armed_) return false;
        playing_=value; return true;
    }
    bool interrupt(uint32_t epoch) noexcept {
        if(!epoch || epoch!=epoch_) return false;
        mute(); return true;
    }
    void mute() noexcept { begun_=false; armed_=false; playing_=false; acknowledged_=false; }
    uint32_t epoch() const noexcept { return epoch_; }
    bool armed() const noexcept { return armed_; }
};
}
