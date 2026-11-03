from pathlib import Path
import subprocess,tempfile,os
r=Path(__file__).resolve().parent;fw=Path(os.environ.get('PORTRAIT_FW',str(r.parent)))
with tempfile.TemporaryDirectory(prefix='portrait-geometry-') as temp:
 t=Path(temp);(t/'lvgl.h').write_text('#pragma once\nstruct lv_image_header_t{unsigned w,h;};struct lv_image_dsc_t{lv_image_header_t header;unsigned data_size;const unsigned char* data;};\n')
 header=t/'external.hpp';header.write_text('''namespace companion_local_portraits{inline unsigned char bytes[8*8*4]{};inline lv_image_dsc_t image{{8,8},sizeof(bytes),bytes};inline companion::desk::LocalPortrait For(companion_modes::Provider p){return {p,&image};}}''')
 cpp=t/'test.cpp';cpp.write_text('''#include "companion/companion_portraits.hpp"
#include <cassert>
int main(){using P=companion_modes::Provider;assert(companion::desk::LocalPortraitFor(P::Muse));companion_local_portraits::image.header.h=0;assert(!companion::desk::LocalPortraitFor(P::Muse));companion_local_portraits::image.header.h=1025;assert(!companion::desk::LocalPortraitFor(P::Muse));companion_local_portraits::image.header.h=8;companion_local_portraits::image.data=nullptr;assert(!companion::desk::LocalPortraitFor(P::Muse));}
''')
 args=['clang++','-std=c++17','-fsanitize=address,undefined','-DSTACKCHAN_LOCAL_PORTRAITS=1','-DSTACKCHAN_LOCAL_MUSE_PORTRAIT=1','-DSTACKCHAN_LOCAL_PORTRAIT_HEADER="'+str(header)+'"','-I'+str(t),'-I'+str(fw/'main'),str(cpp),'-o',str(t/'test')]
 p=subprocess.run(args,capture_output=True,text=True);assert p.returncode==0,p.stderr;subprocess.run([str(t/'test')],check=True)
 # The backwards-compatible two-field input above uses the whole image.
 if 'FaceArea' in (fw/'main/companion/companion_portraits.hpp').read_text():
  header.write_text('''namespace companion_local_portraits{inline unsigned char bytes[8*8*4]{};inline lv_image_dsc_t image{{8,8},sizeof(bytes),bytes};inline companion::desk::FaceArea area{};inline companion::desk::LocalPortrait For(companion_modes::Provider p){return {p,&image,area};}}''')
  cpp.write_text('''#include "companion/companion_portraits.hpp"
#include <cassert>
int main(){using P=companion_modes::Provider;for(unsigned x:{0u,1u,7u,8u,9u})for(unsigned y:{0u,1u,7u,8u,9u})for(unsigned w:{0u,1u,8u,9u})for(unsigned h:{0u,1u,8u,9u}){companion_local_portraits::area={x,y,w,h};bool valid=(!x&&!y&&!w&&!h)||(w&&h&&x<8&&y<8&&w<=8-x&&h<=8-y);assert(bool(companion::desk::LocalPortraitFor(P::Muse))==valid);}companion_local_portraits::area={0,0,~0u,1};assert(!companion::desk::LocalPortraitFor(P::Muse));}
''')
  p=subprocess.run(args,capture_output=True,text=True);assert p.returncode==0,p.stderr;subprocess.run([str(t/'test')],check=True)
print('PASS invalid image dimensions/data and bounded face-area edge cases')
