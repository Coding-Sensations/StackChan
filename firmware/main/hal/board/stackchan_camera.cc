#include "stackchan_camera.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <stdexcept>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_log.h>
#include "esp_video_device.h"
#include "linux/videodev2.h"
#define TAG "StackChanCamera"
#define CAM_PRINT_FOURCC(format) (void)0
static std::atomic<StackChanCamera*> video_owner{nullptr};

StackChanCamera::StackChanCamera(const esp_video_init_config_t&) {} // inert: no driver IO
bool StackChanCamera::StartBound(const esp_video_init_config_t& config, Binding binding, bool local_opt_in, CurrentOwner current_owner, void* owner_context)
{
    std::unique_lock<std::mutex> lock(producer_.mutex,std::try_to_lock);
    if(!lock.owns_lock())return false;
#if defined(CONFIG_XIAOZHI_ENABLE_ROTATE_CAMERA_IMAGE) || CONFIG_ESP_VIDEO_ENABLE_ISP_VIDEO_DEVICE
    // Unsupported autonomous/rotation producers cannot participate in this lock.
    (void)config;(void)binding;(void)local_opt_in;(void)current_owner;(void)owner_context;return false;
#else
    if(!stop_authority_)return false;
    if(!current_owner||!current_owner(owner_context,binding)||!producer_.BeginLocked(binding,local_opt_in))return false;
    auto authorized=[&](){return producer_.Current(binding)&&current_owner(owner_context,binding);};
    StackChanCamera* expected=nullptr;
    if(!video_owner.compare_exchange_strong(expected,this)){
        producer_.FinishStopLocked(binding,[](){return true;});return false;
    }
    close_progress_={};last_closed_receipt_={};capture_clock_.ResetLocked();
    custody_binding_=binding;
    adapter_.Disable();
    auto initialize=[&]() -> bool {

    if(!authorized())return false;
    close_progress_.Require(companion::desk::CameraVideoDeinit);
    video_initialized_=true;
    if (esp_video_init(&config) != ESP_OK) {
        ESP_LOGE(TAG, "esp_video_init failed");
        return false;
    }

#ifdef CONFIG_XIAOZHI_ENABLE_CAMERA_DEBUG_MODE
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
#endif  // CONFIG_XIAOZHI_ENABLE_CAMERA_DEBUG_MODE

    const char* video_device_name = nullptr;

    if (false) { /* 用于构建 else if */
    }
#if CONFIG_ESP_VIDEO_ENABLE_MIPI_CSI_VIDEO_DEVICE
    else if (config.csi != nullptr) {
        video_device_name = ESP_VIDEO_MIPI_CSI_DEVICE_NAME;
    }
#endif
#if CONFIG_ESP_VIDEO_ENABLE_DVP_VIDEO_DEVICE
    else if (config.dvp != nullptr) {
        video_device_name = ESP_VIDEO_DVP_DEVICE_NAME;
    }
#endif
#if CONFIG_ESP_VIDEO_ENABLE_HW_JPEG_VIDEO_DEVICE
    else if (config.jpeg != nullptr) {
        video_device_name = ESP_VIDEO_JPEG_DEVICE_NAME;
    }
#endif
#if CONFIG_ESP_VIDEO_ENABLE_SPI_VIDEO_DEVICE
    else if (config.spi != nullptr) {
        video_device_name = ESP_VIDEO_SPI_DEVICE_NAME;
    }
#endif
#if CONFIG_ESP_VIDEO_ENABLE_USB_UVC_VIDEO_DEVICE
    else if (config.usb_uvc != nullptr) {
        video_device_name = ESP_VIDEO_USB_UVC_DEVICE_NAME(0);
    }
#endif

    if (video_device_name == nullptr) {
        ESP_LOGE(TAG, "no video device is enabled");
        return false;
    }

    video_fd_ = open(video_device_name, O_RDWR | O_NONBLOCK);

    if (video_fd_ < 0) {
        ESP_LOGE(TAG, "open %s failed, errno=%d(%s)", video_device_name, errno, strerror(errno));
#if CONFIG_XIAOZHI_ENABLE_CAMERA_DEBUG_MODE
        log_available_video_devices();
#endif  // CONFIG_XIAOZHI_ENABLE_CAMERA_DEBUG_MODE
        return false;
    }

    close_progress_.Require(companion::desk::CameraFdClose);
    struct v4l2_capability cap = {};
    if (ioctl(video_fd_, VIDIOC_QUERYCAP, &cap) != 0) {
        ESP_LOGE(TAG, "VIDIOC_QUERYCAP failed, errno=%d(%s)", errno, strerror(errno));
        return false;
    }

    ESP_LOGD(
        TAG,
        "VIDIOC_QUERYCAP: driver=%s, card=%s, bus_info=%s, version=0x%08lx, capabilities=0x%08lx, device_caps=0x%08lx",
        cap.driver, cap.card, cap.bus_info, cap.version, cap.capabilities, cap.device_caps);

    struct v4l2_format format = {};
    format.type               = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(video_fd_, VIDIOC_G_FMT, &format) != 0) {
        ESP_LOGE(TAG, "VIDIOC_G_FMT failed, errno=%d(%s)", errno, strerror(errno));
        return false;
    }
    ESP_LOGD(TAG, "VIDIOC_G_FMT: pixelformat=0x%08lx, width=%ld, height=%ld", format.fmt.pix.pixelformat,
             format.fmt.pix.width, format.fmt.pix.height);
    CAM_PRINT_FOURCC(format.fmt.pix.pixelformat);

    struct v4l2_format setformat = {};
    setformat.type               = V4L2_BUF_TYPE_VIDEO_CAPTURE;
#ifdef CONFIG_XIAOZHI_ENABLE_ROTATE_CAMERA_IMAGE
    sensor_width_  = format.fmt.pix.width;
    sensor_height_ = format.fmt.pix.height;
#endif  // CONFIG_XIAOZHI_ENABLE_ROTATE_CAMERA_IMAGE
    setformat.fmt.pix.width  = format.fmt.pix.width;
    setformat.fmt.pix.height = format.fmt.pix.height;

    struct v4l2_fmtdesc fmtdesc = {};
    fmtdesc.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmtdesc.index               = 0;
    uint32_t best_fmt           = 0;
    int best_rank               = 1 << 30;  // large number

    // 注: 当前版本 esp_video 中 YUV422P 实际输出为 YUYV。
    auto get_rank=[](uint32_t fmt)->int {
        switch(fmt){case V4L2_PIX_FMT_RGB565:return 0;case V4L2_PIX_FMT_RGB565X:return 1;
            case V4L2_PIX_FMT_RGB24:return 2;case V4L2_PIX_FMT_GREY:return 3;default:return 1<<29;}
    };
    while (ioctl(video_fd_, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        ESP_LOGD(TAG, "VIDIOC_ENUM_FMT: pixelformat=0x%08lx, description=%s", fmtdesc.pixelformat, fmtdesc.description);
        CAM_PRINT_FOURCC(fmtdesc.pixelformat);
        int rank = get_rank(fmtdesc.pixelformat);
        if (rank < best_rank) {
            best_rank = rank;
            best_fmt  = fmtdesc.pixelformat;
        }
        fmtdesc.index++;
    }
    if (best_rank < (1 << 29)) {
        setformat.fmt.pix.pixelformat = best_fmt;
        sensor_format_                = best_fmt;
    }

    if (!setformat.fmt.pix.pixelformat) {
        ESP_LOGE(TAG, "no supported pixel format found");
        sensor_format_ = 0;
        return false;
    }

    ESP_LOGD(TAG, "selected pixel format: 0x%08lx", setformat.fmt.pix.pixelformat);

    if (ioctl(video_fd_, VIDIOC_S_FMT, &setformat) != 0) {
        ESP_LOGE(TAG, "VIDIOC_S_FMT failed, errno=%d(%s)", errno, strerror(errno));
        sensor_format_ = 0;
        return false;
    }

    if(!setformat.fmt.pix.width||!setformat.fmt.pix.height||
       setformat.fmt.pix.width>640||setformat.fmt.pix.height>480)return false;
#ifdef CONFIG_XIAOZHI_ENABLE_ROTATE_CAMERA_IMAGE
    frame_.width  = setformat.fmt.pix.height;
    frame_.height = setformat.fmt.pix.width;
#else
    frame_.width  = setformat.fmt.pix.width;
    frame_.height = setformat.fmt.pix.height;
#endif

    const size_t bpp=(sensor_format_==V4L2_PIX_FMT_RGB565||sensor_format_==V4L2_PIX_FMT_RGB565X)?2:
        sensor_format_==V4L2_PIX_FMT_RGB24?3:sensor_format_==V4L2_PIX_FMT_GREY?1:0;
    if(!bpp||!frame_.width||!frame_.height||frame_.width>640||frame_.height>480||
       setformat.fmt.pix.pixelformat!=sensor_format_||
       setformat.fmt.pix.bytesperline!=size_t(frame_.width)*bpp)return false;

    // 申请缓冲并mmap
    struct v4l2_requestbuffers req = {};
    req.count                      = strcmp(video_device_name, ESP_VIDEO_MIPI_CSI_DEVICE_NAME) == 0 ? 2 : 1;
    req.type                       = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory                     = V4L2_MEMORY_MMAP;
    if (ioctl(video_fd_, VIDIOC_REQBUFS, &req) != 0) {
        ESP_LOGE(TAG, "VIDIOC_REQBUFS failed");
        sensor_format_ = 0;
        return false;
    }
    if(req.count==0||req.count>2)return false;
    mmap_buffers_.resize(req.count);
    for (uint32_t i = 0; i < req.count; i++) {
        struct v4l2_buffer buf = {};
        buf.type               = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory             = V4L2_MEMORY_MMAP;
        buf.index              = i;
        if (ioctl(video_fd_, VIDIOC_QUERYBUF, &buf) != 0) {
            ESP_LOGE(TAG, "VIDIOC_QUERYBUF failed");
            sensor_format_ = 0;
            return false;
        }
        if(!buf.length||buf.length>MaxRawFrame)return false;
        void* start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, video_fd_, buf.m.offset);
        if (start == MAP_FAILED) {
            ESP_LOGE(TAG, "mmap failed");
            sensor_format_ = 0;
            return false;
        }
        close_progress_.Require(companion::desk::CameraUnmap);
        mmap_buffers_[i].start  = start;
        mmap_buffers_[i].length = buf.length;

        if (ioctl(video_fd_, VIDIOC_QBUF, &buf) != 0) {
            ESP_LOGE(TAG, "VIDIOC_QBUF failed");
            sensor_format_ = 0;
            return false;
        }
    }

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(!authorized())return false;
    if (ioctl(video_fd_, VIDIOC_STREAMON, &type) != 0) {
        ESP_LOGE(TAG, "VIDIOC_STREAMON failed");
        sensor_format_ = 0;
        return false;
    }


    close_progress_.Require(companion::desk::CameraStreamOff);close_progress_.ever_started=true;
    streaming_on_ = true;
    return authorized();
    };
    const bool ok=initialize();
    if(!ok)producer_.Revoke(binding); // retain partial-open custody for checked cleanup
    return ok;
#endif
}

void StackChanCamera::WipeFrameLocked()
{
    if(frame_.data){volatile uint8_t* p=frame_.data;for(size_t i=0;i<frame_.len;++i)p[i]=0;heap_caps_free(frame_.data);}
    const auto w=frame_.width,h=frame_.height;frame_={};frame_.width=w;frame_.height=h;
    producer_.InvalidateFrameLocked();
}
bool StackChanCamera::CloseDriverLocked()
{
    WipeFrameLocked();adapter_.Disable();
    if(streaming_on_&&video_fd_>=0){
        int type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if(ioctl(video_fd_,VIDIOC_STREAMOFF,&type)!=0)return false;
        streaming_on_=false;close_progress_.Complete(companion::desk::CameraStreamOff);
    }
    for(auto& b:mmap_buffers_){
        if(b.start&&b.length){
            if(munmap(b.start,b.length)!=0)return false;
            b.start=nullptr;b.length=0;
        }
    }
    if(close_progress_.required&companion::desk::CameraUnmap)close_progress_.Complete(companion::desk::CameraUnmap);
    if(video_fd_>=0){
        // A failed close has unspecified descriptor disposition on some VFS
        // implementations. Keep custody and require an external recovery proof;
        // never retry a possibly reused descriptor.
        if(close_uncertain_)return false;
        if(close(video_fd_)!=0){close_uncertain_=true;return false;}
        video_fd_=-1;close_progress_.Complete(companion::desk::CameraFdClose);
    }
    if(video_initialized_){
        if(esp_video_deinit()!=ESP_OK)return false;
        video_initialized_=false;close_progress_.Complete(companion::desk::CameraVideoDeinit);
    }
    mmap_buffers_.clear();sensor_format_=0;return true;
}
bool StackChanCamera::AnyDriverResourcesLocked() const {
    if(video_initialized_||video_fd_>=0||streaming_on_)return true;
    for(const auto& b:mmap_buffers_)if(b.start&&b.length)return true;
    return false;
}
companion::desk::CameraDriverStopReceipt StackChanCamera::StopCheckedReceipt(Binding binding)
{
    using Receipt=companion::desk::CameraDriverStopReceipt;
    using Outcome=companion::desk::CameraDriverStopOutcome;
    producer_.Revoke(binding);
    Receipt report;
    std::unique_lock<std::mutex> lock(producer_.mutex,std::try_to_lock);
    if(!lock.owns_lock()){report.outcome_=Outcome::Busy;return report;}
    // Never turn an arbitrary stale request into Closed merely because this
    // object is currently inert. Exact producer custody must precede a receipt.
    if(!producer_.MatchesLocked(binding)){
        // A retry may refresh the last exact closure snapshot only while no
        // newer producer reservation or resources exist on this same object.
        if(!producer_.CustodyLocked()&&producer_.LastGenerationLocked()==binding.generation&&
           last_closed_receipt_.binding()==binding&&!AnyDriverResourcesLocked()&&
           stop_report_serial_!=UINT64_MAX&&
           (last_closed_receipt_.owned_driver_closed()||last_closed_receipt_.owned_never_started())){
            report=last_closed_receipt_;report.serial_=++stop_report_serial_;return report;
        }
        report.outcome_=Outcome::Stale;return report;
    }
    if(!stop_authority_||stop_report_serial_==UINT64_MAX){report.outcome_=Outcome::DriverFailure;return report;}
    report.binding_=binding;report.authority_=stop_authority_;report.serial_=++stop_report_serial_;
    report.revoked_=!producer_.Current(binding);report.producer_quiesced_=true;
    const bool closed=CloseDriverLocked();
    report.progress_=close_progress_;report.raw_wiped_=frame_.data==nullptr;
    report.resources_remaining_=AnyDriverResourcesLocked();report.uncertain_close_=close_uncertain_;
    if(!closed||report.resources_remaining_||!close_progress_.complete()){
        report.outcome_=Outcome::DriverFailure;return report; // retain cleanup custody
    }
    report.outcome_=close_progress_.required?Outcome::DriverClosed:Outcome::NotStarted;
    last_closed_receipt_=report;
    producer_.ClosedLocked();capture_clock_.ResetLocked();custody_binding_={};
    StackChanCamera* expected=this;video_owner.compare_exchange_strong(expected,nullptr);
    return report;
}
StackChanCamera::StopResult StackChanCamera::StopChecked(Binding binding)
{
    using Outcome=companion::desk::CameraDriverStopOutcome;
    switch(StopCheckedReceipt(binding).outcome()){
        case Outcome::DriverClosed:case Outcome::NotStarted:return StopResult::Closed;
        case Outcome::Busy:return StopResult::Busy;
        case Outcome::Stale:return StopResult::Stale;
        default:return StopResult::DriverFailure;
    }
}
StackChanCamera::~StackChanCamera()
{
    // Owner must stop/join before destruction. Never pretend an unchecked
    // destruction is an off receipt. Failed physical closure is fatal here.
    if(producer_.CustodyLocked()&&StopChecked(custody_binding_)!=StopResult::Closed){
        ESP_LOGE(TAG,"camera destroyed without checked closure");std::terminate();
    }
}
bool StackChanCamera::CaptureBound(Binding binding)
{
    std::unique_lock<std::mutex> lock(producer_.mutex,std::try_to_lock);
    if(!lock.owns_lock())return false;
    if(!producer_.Current(binding)||!streaming_on_||video_fd_<0)return false;
    WipeFrameLocked();
    struct v4l2_buffer buf={};buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;buf.memory=V4L2_MEMORY_MMAP;
    if(ioctl(video_fd_,VIDIOC_DQBUF,&buf)!=0)return false;
    const companion::desk::DriverCaptureObservation observation{
        static_cast<int64_t>(buf.timestamp.tv_sec),static_cast<int64_t>(buf.timestamp.tv_usec),buf.flags,buf.sequence};
    bool valid=producer_.Current(binding)&&buf.index<mmap_buffers_.size();
    if(valid){
        const auto& m=mmap_buffers_[buf.index];
        const size_t bpp=(sensor_format_==V4L2_PIX_FMT_RGB565||sensor_format_==V4L2_PIX_FMT_RGB565X)?2:
                          sensor_format_==V4L2_PIX_FMT_RGB24?3:sensor_format_==V4L2_PIX_FMT_GREY?1:0;
        const size_t required=size_t(frame_.width)*frame_.height*bpp;
        valid=m.start&&required&&required<=MaxRawFrame&&buf.bytesused==required&&m.length>=required;
        if(valid){
            frame_.data=static_cast<uint8_t*>(heap_caps_malloc(required,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
            frame_.len=frame_.data?required:0;
#if defined(CONFIG_XIAOZHI_ENABLE_CAMERA_ENDIANNESS_SWAP) || defined(CONFIG_CAMERA_SENSOR_SWAP_PIXEL_BYTE_ORDER)
            constexpr bool swap=true;
#else
            constexpr bool swap=false;
#endif
            valid=frame_.data&&companion::desk::CameraProducerGate::Normalize(
                static_cast<const uint8_t*>(m.start),buf.bytesused,uint32_t(sensor_format_),swap,
                frame_.data,frame_.len,frame_.width,frame_.height);
            frame_.format=sensor_format_==V4L2_PIX_FMT_RGB565X?V4L2_PIX_FMT_RGB565:sensor_format_;
        }
    }
    if(ioctl(video_fd_,VIDIOC_QBUF,&buf)!=0){producer_.Revoke(binding);valid=false;}
    if(!producer_.Current(binding))valid=false;
    if(!valid){WipeFrameLocked();return false;}
    // Exact installed esp_video 1.3.1 does not populate V4L2 time/sequence.
    // The separate driver-source candidate is not selected or qualified here. Never stamp it with dequeue/read
    // time. The real producer publishes its bytes under this mutex but LV01
    // stays unavailable until that driver contract is qualified.
    const auto clock=capture_clock_.ObserveDequeued(binding,observation);
    producer_.PublishAttestedLocked(binding,clock,true,uint64_t(esp_timer_get_time()/1000));
    return true;
}
bool StackChanCamera::CopyLV01(Binding binding,uint8_t* out,size_t capacity,size_t& written)
{
    std::unique_lock<std::mutex> lock(producer_.mutex,std::try_to_lock);
    if(!lock.owns_lock()){written=0;if(out){volatile uint8_t* p=out;const size_t n=capacity>companion::desk::BorrowedCameraAdapter::MaxEnvelope?companion::desk::BorrowedCameraAdapter::MaxEnvelope:capacity;for(size_t i=0;i<n;++i)p[i]=0;}return false;}
    struct AlreadyHeld {void lock(){}void unlock(){}} held;
    struct View {
        FrameBuffer& f;
        const uint8_t* GetFrameData(){return f.data;}size_t GetFrameSize(){return f.len;}
        int GetFrameWidth(){return f.width;}int GetFrameHeight(){return f.height;}
        int GetFrameFormat(){return int(f.format);}
    } view{frame_};
    auto current=[this](Binding b){return producer_.Current(b);};
    if(!adapter_.enabled())adapter_.Enable(binding,true,current);
    return adapter_.CopyLV01(view,held,binding,producer_.StampLocked(),
        [](){return uint64_t(esp_timer_get_time()/1000);},current,out,capacity,written);
}
std::string StackChanCamera::Explain(const std::string&)
{
    // No physical consent-bound, cancellation-safe HTTP worker exists. All old
    // capture/encode/upload ingress must remain closed, including MCP callers.
    throw std::runtime_error("camera upload unavailable: no bound transport worker");
}

bool StackChanCamera::CopyBGRA(Binding binding,uint8_t* out,size_t capacity,uint64_t& sequence,uint64_t& captured_ns)
{
    sequence=captured_ns=0;
    std::unique_lock<std::mutex> lock(producer_.mutex,std::try_to_lock);
    if(!lock.owns_lock()){
        if(out){volatile uint8_t* p=out;const size_t n=capacity>companion::CameraBGRABytes?companion::CameraBGRABytes:capacity;for(size_t i=0;i<n;++i)p[i]=0;}
        return false;
    }
    struct View {
        FrameBuffer& f;
        const uint8_t* GetFrameData(){return f.data;}size_t GetFrameSize(){return f.len;}
        int GetFrameWidth(){return f.width;}int GetFrameHeight(){return f.height;}
        int GetFrameFormat(){return int(f.format);}
    } view{frame_};
    return bgra_adapter_.CopyLocked(view,binding,producer_.StampLocked(),
        [this](Binding b){return producer_.Current(b);},[](){return uint64_t(esp_timer_get_time()/1000);},
        out,capacity,sequence,captured_ns);
}
