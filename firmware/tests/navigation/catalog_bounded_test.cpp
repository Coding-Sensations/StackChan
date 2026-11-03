#include "apps/app_launcher/view/effect_catalog/effect_catalog.hpp"
#include "apps/app_launcher/view/screensaver_filter.hpp"
#include <vector>
#include <cassert>
#include <cstdio>
static uint64_t tick=0;
static uint64_t Clock(){tick+=13000;return tick;}
int main(){
 std::vector<uint16_t> guarded(320*240+2,0x5aad);
 auto* pixels=guarded.data()+1;
 // Clock advances deterministically: first checkpoint rejects this frame.
 effect_catalog::Reset(0);
 assert(!effect_catalog::Render(0,0,pixels,320,240,Clock));
 // An exhausted frame must not poison reset or borrow the buffer afterward.
 unsigned frames=0,limited=0;
 for(unsigned round=0;round<2;++round)for(unsigned id=1;id<342;++id)if(view::saver::Allowed(id)){
  effect_catalog::Reset(uint16_t(id-1));
  for(auto ms:{0u,125u,1000u,29999u,30000u,3599999u}){
   if(!effect_catalog::Render(uint16_t(id-1),ms,pixels,320,240))++limited;
   ++frames;assert(guarded.front()==0x5aad&&guarded.back()==0x5aad);
  }
 }
 effect_catalog::Reset(0);assert(effect_catalog::Render(0,0,pixels,320,240));
 std::printf("frames=%u work_limited=%u canaries=ok\n",frames,limited);
}
