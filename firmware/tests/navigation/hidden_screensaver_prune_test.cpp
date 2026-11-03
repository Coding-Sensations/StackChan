#include "apps/app_launcher/view/screensaver_library.hpp"
#include "apps/app_launcher/view/screensaver_styles.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
int main(){using namespace view::saver;static_assert(SlideCount==342);
const unsigned requested[]={3,7,8,9,11,12,15,16,34,39,44,47,55,62,64,66,70,71,74,80,88,90,91,92,94,96,98,99,100,101,102,103,104,105,106,108,109,110,114,115,116,118,119,121,133,134,136,138,142,157,220,237,240,242,245,252,257,260,261,262,264,269,274,275,279,282,284,289,298,301,307,311,322,324,335,338};
std::vector<uint16_t> pixels(320*240,0xa55a);
for(unsigned id:requested){
 if(Allowed(id)){std::fprintf(stderr,"Requested hidden ID %u remains selectable\n",id);return 1;}
 assert(!effect_catalog::Usable(uint16_t(id-1)));
 assert(std::strcmp(effect_catalog::Name(uint16_t(id-1)),"Unavailable")==0);
 effect_catalog::Reset(uint16_t(id-1));assert(!effect_catalog::Render(uint16_t(id-1),1000,pixels.data()));
 assert(std::all_of(pixels.begin(),pixels.end(),[](auto p){return p==0xa55a;}));
 Selection old;old.bits[id/8]|=uint8_t(1u<<(id%8));
 auto restored=Library::fromPayload(old.payload());assert(restored.effective().count()==1&&restored.effective().selected(0));
 assert(!restored.inView(id,false)&&!restored.inView(id,true));assert(FromStored(int(id))==Style::Dvd);
}
// The three now-confirmed spellings are part of requested[] above.
assert(AllowedCount==140);assert(effect_catalog::Count()==341);
}
