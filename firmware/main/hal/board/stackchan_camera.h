#pragma once
#include "sdkconfig.h"
#ifndef CONFIG_IDF_TARGET_ESP32
#include <vector>
#include "camera.h"
#include "jpg/image_to_jpeg.h"
#include "esp_video_init.h"
#include "companion/camera_producer_gate.hpp"
#include "companion/camera_bgra_adapter.hpp"
#include "companion/camera_stop_receipt.hpp"
#include "companion/camera_capture_clock.hpp"
class StackChanCamera : public Camera {
public:
    using Binding=companion::desk::CameraBinding;
    enum class StopResult { Closed, Busy, Stale, DriverFailure };
    // Constructing the board's existing camera does not initialize a sensor.
    explicit StackChanCamera(const esp_video_init_config_t&);
    ~StackChanCamera();
    using CurrentOwner=bool(*)(void*,Binding);
    bool StartBound(const esp_video_init_config_t&,Binding,bool local_opt_in,CurrentOwner,void*);
    bool CaptureBound(Binding);
    void ConfigureExternalTiming(std::shared_ptr<companion::desk::QualifiedCaptureTimingAuthority> qualified) {
        std::lock_guard<std::mutex> lock(producer_.mutex);capture_clock_.SetExternalTimingAuthority(std::move(qualified));
    }
    uint64_t StopAuthorityFor(Binding binding) {
        std::lock_guard<std::mutex> lock(producer_.mutex);
        return custody_binding_==binding?stop_authority_:0;
    }
    StopResult StopChecked(Binding);
    companion::desk::CameraDriverStopReceipt StopCheckedReceipt(Binding);
    void Revoke(Binding b) noexcept {producer_.Revoke(b);}
    bool CopyLV01(Binding,uint8_t*,size_t,size_t&);
    bool CopyBGRA(Binding,uint8_t*,size_t,uint64_t& sequence,uint64_t& captured_ns);
    bool Capture() override {return false;} // legacy unbound ingress fails closed
    bool StreamCaptures() {return false;}
    void SetExplainUrl(const std::string&,const std::string&) override {}
    std::string Explain(const std::string&) override;
    bool SetHMirror(bool) override {return false;}
    bool SetVFlip(bool) override {return false;}
    // Legacy raw readers cannot hold an owned frame and are deliberately closed.
    const uint8_t* GetFrameData() {return nullptr;}
    size_t GetFrameSize() {return 0;}
    int GetFrameWidth() {return 0;}
    int GetFrameHeight() {return 0;}
    int GetFrameFormat() {return 0;}
private:
    struct FrameBuffer {uint8_t* data=nullptr;size_t len=0;uint16_t width=0,height=0;v4l2_pix_fmt_t format=0;} frame_;
    struct MmapBuffer {void* start=nullptr;size_t length=0;};
    companion::desk::CameraProducerGate producer_;
    companion::desk::BorrowedCameraAdapter adapter_;
    companion::desk::CameraBGRAAdapter bgra_adapter_;
    Binding custody_binding_{};
    const uint64_t stop_authority_=companion::desk::NextCameraAuthorityId();
    uint64_t stop_report_serial_=0;
    companion::desk::CameraCloseProgress close_progress_{};
    companion::desk::CameraDriverStopReceipt last_closed_receipt_{};
    companion::desk::InstalledVideoCaptureClock capture_clock_;
    bool AnyDriverResourcesLocked() const;
    int video_fd_=-1;
    bool streaming_on_=false,video_initialized_=false,close_uncertain_=false;
    v4l2_pix_fmt_t sensor_format_=0;
    std::vector<MmapBuffer> mmap_buffers_;
    static constexpr size_t MaxRawFrame=640*480*3;
    void WipeFrameLocked();
    bool CloseDriverLocked();
};
#endif
