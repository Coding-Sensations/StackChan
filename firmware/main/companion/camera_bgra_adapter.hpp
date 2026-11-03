#pragma once
#include "companion/desk_camera_adapter.hpp"
#include "camera_video_wire.hpp"
namespace companion::desk {
// Called with the actual producer mutex already held. No IO/allocation/transport.
class CameraBGRAAdapter {
    uint64_t last_sequence_=0,last_now_=0;
public:
    template<class Camera,class Current,class Now>
    bool CopyLocked(Camera& camera,CameraBinding binding,TrustedFrameStamp stamp,Current current,Now now,
                    uint8_t* out,size_t capacity,uint64_t& sequence,uint64_t& captured_ns) {
        sequence=captured_ns=0;
        const size_t wipe=capacity>companion::CameraBGRABytes?companion::CameraBGRABytes:capacity;
        auto clear=[&](){if(out){volatile uint8_t* p=out;for(size_t i=0;i<wipe;++i)p[i]=0;}};
        auto guard=[&](){
            if(!current(binding)||!stamp.timestamp_trusted||!stamp.canonical_layout)return false;
            const uint64_t tick=now();
            if(tick<last_now_||!stamp.captured_at_ms||stamp.captured_at_ms>tick||tick-stamp.captured_at_ms>=250)return false;
            last_now_=tick;return current(binding);
        };
        auto reject=[&](){clear();return false;};
        if(!out||capacity!=companion::CameraBGRABytes||stamp.sequence<=last_sequence_||!guard())return reject();
        const uint8_t* src=camera.GetFrameData();const size_t length=camera.GetFrameSize();
        const int w=camera.GetFrameWidth(),h=camera.GetFrameHeight();const uint32_t fmt=uint32_t(camera.GetFrameFormat());
        const size_t bpp=fmt==CameraRGB565?2:fmt==CameraRGB24?3:fmt==CameraGrey?1:0;
        if(!guard()||!src||w<1||h<1||w>640||h>480||!bpp||length!=size_t(w)*size_t(h)*bpp)return reject();
        const uintptr_t source=reinterpret_cast<uintptr_t>(src),dest=reinterpret_cast<uintptr_t>(out);
        if(source>UINTPTR_MAX-length||dest>UINTPTR_MAX-capacity||(dest<source+length&&source<dest+capacity))return reject();
        clear(); // opaque black letterbox, with no retained prior-frame pixels
        for(size_t i=3;i<capacity;i+=4)out[i]=255;
        int tw=320,th=240;
        if(size_t(w)*240>size_t(h)*320)th=int(size_t(h)*320/size_t(w));
        else tw=int(size_t(w)*240/size_t(h));
        if(tw<1)tw=1;
        if(th<1)th=1;
        const int dx=(320-tw)/2,dy=(240-th)/2;
        for(int y=0;y<th;++y)for(int x=0;x<tw;++x){
            const uint8_t* p=src+(size_t(y)*size_t(h)/size_t(th)*size_t(w)+size_t(x)*size_t(w)/size_t(tw))*bpp;
            uint8_t* q=out+(size_t(y+dy)*320+size_t(x+dx))*4;
            if(fmt==CameraRGB24){q[0]=p[2];q[1]=p[1];q[2]=p[0];}
            else if(fmt==CameraGrey)q[0]=q[1]=q[2]=p[0];
            else{const uint16_t v=uint16_t(p[0])|uint16_t(uint16_t(p[1])<<8);const uint8_t r=(v>>11)&31,g=(v>>5)&63,b=v&31;
                q[0]=uint8_t((b<<3)|(b>>2));q[1]=uint8_t((g<<2)|(g>>4));q[2]=uint8_t((r<<3)|(r>>2));}
        }
        if(!guard()||stamp.captured_at_ms>UINT64_MAX/1000000)return reject();
        last_sequence_=stamp.sequence;sequence=stamp.sequence;captured_ns=stamp.captured_at_ms*1000000;return true;
    }
};
}
