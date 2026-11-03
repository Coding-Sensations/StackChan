#pragma once
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace companion::desk {
// Values from the camera's V4L2 interface; RGB565 is little-endian after
// StackChanCamera's RGB565X normalization. No JPEG decoder is provided.
constexpr uint32_t CameraFourCC(char a,char b,char c,char d) noexcept {
    return uint32_t(a)|(uint32_t(b)<<8)|(uint32_t(c)<<16)|(uint32_t(d)<<24);
}
constexpr uint32_t CameraRGB565=CameraFourCC('R','G','B','P');
constexpr uint32_t CameraRGB24=CameraFourCC('R','G','B','3');
constexpr uint32_t CameraGrey=CameraFourCC('G','R','E','Y');
constexpr uint32_t CameraJPEG=CameraFourCC('J','P','E','G');
struct CameraBinding {
    uint64_t generation=0,epoch=0;
    bool operator==(CameraBinding b) const noexcept {return generation==b.generation&&epoch==b.epoch;}
};
struct TrustedFrameStamp {
    uint64_t sequence=0,captured_at_ms=0;
    bool timestamp_trusted=false;
    // Owner attests tightly packed getter bytes: RGB565 LE / RGB24 RGB / GREY.
    // This MUST account for CONFIG_XIAOZHI_ENABLE_CAMERA_ENDIANNESS_SWAP.
    bool canonical_layout=false;
};
class BorrowedCameraAdapter {
    CameraBinding binding_{};
    bool enabled_=false,have_sequence_=false,have_time_=false;
    uint64_t sequence_=0,last_time_=0;
    static void Wipe(uint8_t* out,size_t capacity) noexcept {
        if(out){volatile uint8_t* p=out;for(size_t i=0;i<capacity;++i)p[i]=0;}
    }
    template<class Now,class Current> bool Guard(CameraBinding binding,const TrustedFrameStamp& stamp,
                                                  Now& now,Current& current) {
        if(!enabled_||!(binding==binding_)||!current(binding))return false;
        const uint64_t tick=now();
        if((have_time_&&tick<last_time_)||!stamp.timestamp_trusted||!stamp.canonical_layout||
           stamp.captured_at_ms>tick||tick-stamp.captured_at_ms>=500)return false;
        have_time_=true;last_time_=tick;
        // Producer cancellation is atomic and can change during a time read.
        return current(binding);
    }
public:
    static constexpr size_t MaxEnvelope=8+160*120*3;
    // Caller serializes Enable/Disable/Copy with its existing owner protocol;
    // callbacks must be nonthrowing, side-effect-free validation/time readers.
    template<class Current> bool Enable(CameraBinding binding,bool local_opt_in,Current current) {
        if(!local_opt_in||binding.generation==0||binding.epoch==0||!current(binding)) {
            if(binding==binding_)Disable();
            return false;
        }
        Disable();
        binding_=binding;enabled_=true;return true;
    }
    void Disable() noexcept {enabled_=false;binding_={};have_sequence_=false;sequence_=0;}
    bool enabled() const noexcept {return enabled_;}
    // ExistingLock is the SAME BasicLockable used by all camera producers and
    // owner revocation. Caller must not hold it already. Camera is borrowed;
    // only GetFrame* is used. out must be a valid separate writable buffer.
    // No constructor, Capture, StreamCaptures, Explain, transport or driver IO.
    template<class Camera,class ExistingLock,class Now,class Current>
    bool CopyLV01(Camera& camera,ExistingLock& existing_lock,CameraBinding binding,
                  TrustedFrameStamp stamp,Now now,Current current,
                  uint8_t* out,size_t capacity,size_t& written) {
        written=0;
        // An excessive capacity is a caller contract error. Wipe only the
        // documented bounded envelope region, never an unbounded caller range.
        const size_t wipe_size=capacity>MaxEnvelope?MaxEnvelope:capacity;
        auto reject=[&](){
            Wipe(out,wipe_size);
            // Retired callbacks may reject their output, never revoke a newer owner.
            if(binding==binding_)Disable();
            return false;
        };
        if(!out||capacity<8||capacity>MaxEnvelope)return reject();
        std::lock_guard<ExistingLock> lock(existing_lock);
        if(!Guard(binding,stamp,now,current)||(have_sequence_&&stamp.sequence<=sequence_))return reject();
        const uint8_t* data=camera.GetFrameData();
        const size_t length=camera.GetFrameSize();
        const int width=camera.GetFrameWidth(),height=camera.GetFrameHeight();
        const uint32_t format=uint32_t(camera.GetFrameFormat());
        if(!Guard(binding,stamp,now,current)||!data||width<1||height<1||width>4096||height>4096)return reject();
        const size_t bpp=format==CameraRGB565?2:format==CameraRGB24?3:format==CameraGrey?1:0;
        if(!bpp)return reject();
        const size_t required=size_t(width)*size_t(height)*bpp;
        if(length<required)return reject();
        // Disallow in-place conversion, including buffers that overlap only a
        // suffix of the source. Pointer arithmetic never wraps.
        const uintptr_t src=reinterpret_cast<uintptr_t>(data),dst=reinterpret_cast<uintptr_t>(out);
        if(src>UINTPTR_MAX-required||dst>UINTPTR_MAX-capacity||
           (dst<src+required&&src<dst+capacity))return reject();
        int tw=width,th=height;
        if(width>160||height>120){
            if(size_t(width)*120>size_t(height)*160){tw=160;th=int(size_t(height)*160/size_t(width));}
            else{th=120;tw=int(size_t(width)*120/size_t(height));}
            if(tw<1)tw=1;
            if(th<1)th=1;
        }
        const size_t total=8+size_t(tw)*size_t(th)*3;
        if(capacity<total)return reject();
        // Clear unused caller storage as well; no stale pixels remain on success.
        Wipe(out,capacity);
        out[0]='L';out[1]='V';out[2]='0';out[3]='1';
        out[4]=uint8_t(tw>>8);out[5]=uint8_t(tw);out[6]=uint8_t(th>>8);out[7]=uint8_t(th);
        for(int y=0;y<th;++y)for(int x=0;x<tw;++x){
            const size_t sy=size_t(y)*size_t(height)/size_t(th),sx=size_t(x)*size_t(width)/size_t(tw);
            const uint8_t* p=data+(sy*size_t(width)+sx)*bpp;
            uint8_t* q=out+8+(size_t(y)*size_t(tw)+size_t(x))*3;
            if(format==CameraRGB24){q[0]=p[0];q[1]=p[1];q[2]=p[2];}
            else if(format==CameraGrey){q[0]=q[1]=q[2]=p[0];}
            else {const uint16_t v=uint16_t(p[0])|uint16_t(uint16_t(p[1])<<8);
                const uint8_t r=uint8_t((v>>11)&31),g=uint8_t((v>>5)&63),b=uint8_t(v&31);
                q[0]=uint8_t((r<<3)|(r>>2));q[1]=uint8_t((g<<2)|(g>>4));q[2]=uint8_t((b<<3)|(b>>2));}
        }
        if(!Guard(binding,stamp,now,current))return reject();
        sequence_=stamp.sequence;have_sequence_=true;written=total;return true;
    }
};
}
