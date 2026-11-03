#pragma once
#include <lvgl.h>
#if !LV_USE_LODEPNG
#error "Mira's approved PNG character requires the existing LODEPNG decoder"
#endif
// Exact handoff-approved Mira idle artwork, copied unmodified from the existing
// Mac Resources/mira-idle-frame.png. SHA256 93432bf8...35e4ce79; no new character art.
extern const unsigned char mira_idle_start[] asm("_binary_mira_idle_frame_png_start");
extern const unsigned char mira_idle_end[] asm("_binary_mira_idle_frame_png_end");
namespace companion::desk {
inline const lv_image_dsc_t* MiraCharacterImage(){
    static const lv_image_dsc_t image=[] {
        lv_image_dsc_t value{};value.header.magic=LV_IMAGE_HEADER_MAGIC;
        value.header.cf=LV_COLOR_FORMAT_RAW;value.header.w=320;value.header.h=240;
        value.data_size=static_cast<uint32_t>(mira_idle_end-mira_idle_start);value.data=mira_idle_start;return value;
    }();
    return &image;
}
}
