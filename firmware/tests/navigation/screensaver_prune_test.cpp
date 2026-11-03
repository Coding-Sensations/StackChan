#include "apps/app_launcher/view/screensaver_library.hpp"
#include "apps/app_launcher/view/screensaver_styles.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <vector>
int main(){
 using namespace view::saver;
 static_assert(SlideCount==342); // Never renumber persisted choices.
 std::vector<uint16_t> pixels(320*240,0xa55a);
 for(unsigned id:{157u,220u}){
  assert(!Allowed(id));
  assert(!effect_catalog::Usable(uint16_t(id-1)));
  assert(std::strcmp(effect_catalog::Name(uint16_t(id-1)),"Unavailable")==0);
  effect_catalog::Reset(uint16_t(id-1));
  assert(!effect_catalog::Render(uint16_t(id-1),1000,pixels.data()));
  assert(std::all_of(pixels.begin(),pixels.end(),[](auto p){return p==0xa55a;}));
  // A legacy selection containing only this removed ID falls back safely to DVD.
  Selection old;old.bits[id/8]|=uint8_t(1u<<(id%8));
  auto restored=Library::fromPayload(old.payload());
  assert(restored.effective().count()==1&&restored.effective().selected(0));
  assert(!restored.inView(id,false)&&!restored.inView(id,true));
  assert(FromStored(int(id))==Style::Dvd);
 }
 assert(AllowedCount==140); // DVD + 139 retained routines after the verified hidden list.
 assert(effect_catalog::Count()==341);
 assert(std::strcmp(effect_catalog::Name(157),"Hyperball")==0);
 assert(std::strcmp(effect_catalog::Name(218),"Menger")==0);
 assert(std::strcmp(effect_catalog::Name(221),"Mirrorblob")==0);
 assert(std::strstr(CatalogOptions(),"Hydrostat")==nullptr);
 assert(std::strstr(CatalogOptions(),"Metaballs")==nullptr);
}
