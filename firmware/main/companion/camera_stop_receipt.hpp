#pragma once
#include "companion/desk_camera_adapter.hpp"
#include <cstdint>
#include <atomic>
class StackChanCamera;
class M5StackCoreS3Board;
namespace companion::desk {
inline uint64_t NextCameraAuthorityId() noexcept {
    static std::atomic<uint64_t> next{0};
    uint64_t old=next.load();
    while(old!=UINT64_MAX){if(next.compare_exchange_weak(old,old+1))return old+1;}
    return 0;
}
enum class CameraDriverStopOutcome { Busy, Stale, DriverFailure, NotStarted, DriverClosed };
enum CameraCloseStage : uint8_t { CameraStreamOff=1,CameraUnmap=2,CameraFdClose=4,CameraVideoDeinit=8 };
// Numeric progress is not a receipt and cannot prove physical silence.
struct CameraCloseProgress {
    uint8_t required=0,completed=0;
    bool ever_started=false;
    void Require(uint8_t stage) noexcept {required|=stage;}
    void Complete(uint8_t stage) noexcept {completed|=uint8_t(stage&required);}
    bool complete() const noexcept {return (completed&required)==required;}
};
class CameraDriverStopReceipt {
    CameraBinding binding_{};
    uint64_t authority_=0,serial_=0;
    CameraDriverStopOutcome outcome_=CameraDriverStopOutcome::Stale;
    CameraCloseProgress progress_{};
    bool revoked_=false,producer_quiesced_=false,raw_wiped_=false,resources_remaining_=false,uncertain_close_=false;
    friend class ::StackChanCamera;
    friend class ::M5StackCoreS3Board;
public:
    CameraDriverStopReceipt()=default;
    CameraBinding binding() const noexcept {return binding_;}
    uint64_t authority_id() const noexcept {return authority_;}
    uint64_t report_serial() const noexcept {return serial_;}
    CameraDriverStopOutcome outcome() const noexcept {return outcome_;}
    CameraCloseProgress stages() const noexcept {return progress_;}
    bool authorization_revoked() const noexcept {return revoked_;}
    bool producer_quiesced() const noexcept {return producer_quiesced_;}
    bool raw_frame_wiped() const noexcept {return raw_wiped_;}
    bool resources_remaining() const noexcept {return resources_remaining_;}
    bool close_uncertain() const noexcept {return uncertain_close_;}
    bool owned_driver_closed() const noexcept {
        return binding_.generation&&binding_.epoch&&authority_&&serial_&&outcome_==CameraDriverStopOutcome::DriverClosed&&
            revoked_&&producer_quiesced_&&raw_wiped_&&!resources_remaining_&&!uncertain_close_&&progress_.required&&progress_.complete();
    }
    bool owned_never_started() const noexcept {
        return binding_.generation&&binding_.epoch&&authority_&&serial_&&outcome_==CameraDriverStopOutcome::NotStarted&&
            revoked_&&producer_quiesced_&&raw_wiped_&&!resources_remaining_&&!uncertain_close_&&!progress_.required&&!progress_.ever_started;
    }
    // STREAMOFF/unmap/close/deinit are software receipts. Hardware capture/DMA
    // quiescence/power and bounded-stop qualification are separate evidence.
    bool physical_stop_qualified() const noexcept {return false;}
};
class BoardCameraStopReport {
    CameraDriverStopReceipt driver_;
    CameraBinding requested_{};
    bool exact_session_=false,custody_released_=false;
    friend class ::M5StackCoreS3Board;
public:
    BoardCameraStopReport()=default;
    const CameraDriverStopReceipt& driver() const noexcept {return driver_;}
    CameraBinding requested() const noexcept {return requested_;}
    bool exact_session() const noexcept {return exact_session_;}
    bool custody_released() const noexcept {return custody_released_;}
    bool physical_off() const noexcept {return false;}
};
}
