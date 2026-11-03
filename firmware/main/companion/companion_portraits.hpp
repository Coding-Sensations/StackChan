// SPDX-License-Identifier: MIT
#pragma once
#include <apps/stock_shell/provider_catalog.hpp>
#include <lvgl.h>
#ifndef STACKCHAN_LOCAL_PORTRAITS
#define STACKCHAN_LOCAL_PORTRAITS 0
#endif
#ifndef STACKCHAN_LOCAL_HERMES_PORTRAIT
#define STACKCHAN_LOCAL_HERMES_PORTRAIT 0
#endif
#ifndef STACKCHAN_LOCAL_CODEX_PORTRAIT
#define STACKCHAN_LOCAL_CODEX_PORTRAIT 0
#endif
#ifndef STACKCHAN_LOCAL_MUSE_PORTRAIT
#define STACKCHAN_LOCAL_MUSE_PORTRAIT 0
#endif
#ifndef STACKCHAN_LOCAL_CLAUDE_PORTRAIT
#define STACKCHAN_LOCAL_CLAUDE_PORTRAIT 0
#endif
namespace companion::desk {
// All zero means the whole image. Nonempty crop is inside the image.
struct FaceArea{unsigned x=0,y=0,width=0,height=0;};
struct LocalPortrait{companion_modes::Provider provider=companion_modes::Provider::StackChan;const lv_image_dsc_t* image=nullptr;FaceArea face{};};
}
#if STACKCHAN_LOCAL_PORTRAITS
#include STACKCHAN_LOCAL_PORTRAIT_HEADER
#endif
namespace companion::desk {
// Optional trusted local input, never a Mira fallback or a bundled vendor image.
inline LocalPortrait LocalPortraitEntry(companion_modes::Provider provider){
 using P=companion_modes::Provider;
 if(!companion_modes::Find(provider))return {};
 if(provider!=P::Hermes&&provider!=P::Muse&&provider!=P::Claude&&provider!=P::Codex)return {};
#if STACKCHAN_LOCAL_PORTRAITS
 if((provider==P::Hermes&&!STACKCHAN_LOCAL_HERMES_PORTRAIT)||(provider==P::Muse&&!STACKCHAN_LOCAL_MUSE_PORTRAIT)||(provider==P::Claude&&!STACKCHAN_LOCAL_CLAUDE_PORTRAIT)||(provider==P::Codex&&!STACKCHAN_LOCAL_CODEX_PORTRAIT))return {};
 const auto entry=companion_local_portraits::For(provider);
 if(entry.provider!=provider||!entry.image)return {};
 const auto& image=*entry.image;
 if(!image.header.w||!image.header.h||image.header.w>1024||image.header.h>1024||!image.data||!image.data_size)return {};
 const auto& face=entry.face;
 if(face.x||face.y||face.width||face.height){
  if(!face.width||!face.height||face.x>=image.header.w||face.y>=image.header.h||face.width>image.header.w-face.x||face.height>image.header.h-face.y)return {};
 }
 return entry;
#else
 return {};
#endif
}
inline const lv_image_dsc_t* LocalPortraitFor(companion_modes::Provider provider){return LocalPortraitEntry(provider).image;}
}
