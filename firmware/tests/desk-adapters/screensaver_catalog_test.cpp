#include "effect_catalog/effect_catalog.hpp"
#include "screensaver_filter.hpp"
#include "effect_catalog/raster.hpp"
#include <algorithm>
#include <cassert>
#include <vector>
int main(){
 using namespace view::saver;
 assert(effect_catalog::Count()==341);
 std::vector<uint16_t> pixels(320*240,0x5aad);
 for(auto id:ExcludedSlides){
  assert(!effect_catalog::Usable(uint16_t(id-1)));
  effect_catalog::Reset(uint16_t(id-1)); // Removed/reset entry is a safe tombstone.
  for(auto ms:{0u,125u,1000u,29999u,30000u}){
   assert(!effect_catalog::Render(uint16_t(id-1),ms,pixels.data(),320,240));
   assert(std::all_of(pixels.begin(),pixels.end(),[](auto p){return p==0x5aad;}));
  }
 }
 for(unsigned id=1;id<342;++id)if(Allowed(id)){
  assert(effect_catalog::Usable(uint16_t(id-1)));
  effect_catalog::Reset(uint16_t(id-1));
  assert(effect_catalog::Render(uint16_t(id-1),0,pixels.data(),320,240));
 }
 effect_catalog::Reset(0);assert(effect_catalog::Render(0,0,pixels.data(),320,240));
 assert(std::count(pixels.begin(),pixels.end(),uint16_t(0))>65000); // True black remains black.
 assert(!effect_catalog::Render(33,0,pixels.data(),320,240)); // User explicitly hid Crackberg.
 Raster gray;gray.bind(pixels.data(),320,240);gray.beginFrame(1000000);gray.fillScreen(21130);gray.unbind();
 assert(std::count(pixels.begin(),pixels.end(),uint16_t(21130))==int(pixels.size())); // Preserve exact intentional gray.
 assert(!effect_catalog::Render(65535,0,pixels.data(),320,240));
}
