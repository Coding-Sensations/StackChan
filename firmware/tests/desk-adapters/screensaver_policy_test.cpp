#include "slideshow_policy.hpp"
#include <cassert>
using namespace view::saver;
int main(){
 static_assert(SlideDurationMs==30000);
 static_assert(AllowedCount==140); // Stable Moire ID226 remains a tombstone.
 assert(!Allowed(226)&&Allowed(225)); // Moire2 stays selectable.
 Selection legacy;legacy.bits.fill(255);legacy.order=Order::Random;
 auto migrated=Selection::fromPayload(legacy.payload());assert(migrated.count()==AllowedCount&&migrated.order==Order::Random);
 for(auto id:ExcludedSlides){assert(!migrated.selected(id));migrated.set(id,true);assert(!migrated.selected(id));assert(Allowed(AdjacentAllowed(id,true))&&Allowed(AdjacentAllowed(id,false)));}
 Selection only_removed;for(auto id:ExcludedSlides)only_removed.bits[id/8]|=1u<<(id%8);
 auto fallback=Selection::fromPayload(only_removed.payload());assert(fallback.count()==1&&fallback.selected(0));
 assert(!Allowed(34)&&Allowed(167)&&Allowed(300)&&!Allowed(311)); // Explicit hidden Crackberg/Vines removed; unrequested effects stay.
 assert(AdjacentAllowed(12,true)==20&&AdjacentAllowed(14,false)==10);

 auto all=Selection::defaults();assert(all.count()==AllowedCount&&all.selected(0)&&all.selected(340));
 assert(Selection::decode(all.encode(),0).encode()==all.encode());
 assert(Selection::fromPayload(all.payload()).count()==AllowedCount);
 auto random_all=all;random_all.order=Order::Random;
 assert(Selection::fromPayload(random_all.payload()).order==Order::Random);
 assert(Selection::fromPayload("2:0:"+all.encode()).count()==AllowedCount);
 assert(Selection::fromPayload("1:9:"+all.encode()).order==Order::Sequential);
 assert(Selection::decode("",0).count()==AllowedCount&&Selection::decode("bogus",9).count()==AllowedCount);
 Selection empty;auto zero=empty.encode();auto safe=Selection::decode(zero,1);assert(safe.count()==1&&safe.selected(0)&&safe.order==Order::Random);
 Selection single;single.set(340,true);Slideshow slides;assert(slides.start(single,0)==340);assert(!slides.advance(30000,[]{return 7u;})&&slides.current()==340);
 Selection some;some.set(0,true);some.set(2,true);some.set(340,true);assert(slides.start(some,100)==0);
 assert(!slides.advance(30099,[]{return 0u;}));assert(slides.advance(30100,[]{return 0u;})&&slides.current()==2);
 assert(slides.advance(60100,[]{return 0u;})&&slides.current()==340);assert(slides.advance(90100,[]{return 0u;})&&slides.current()==0);
 some.order=Order::Random;slides.start(some,0);unsigned prior=0;for(unsigned n=1;n<=100;++n){assert(slides.advance(n*30000,[n]{return n*1237u;}));assert(some.selected(slides.current())&&slides.current()!=prior);prior=slides.current();}
 slides.start(some,0xfffffff0);assert(slides.advance(29984,[]{return 0u;})); // millis wrap
 assert(!MayEnterIdle(29999,true,false,false));assert(MayEnterIdle(30000,true,false,false));
 for(unsigned page=0;page<8;++page){assert(!MayEnterIdle(60000,false,false,false));assert(!MayEnterIdle(60000,true,true,false));assert(!MayEnterIdle(60000,true,false,true));assert(!MayEnterIdle(0,true,false,false));}
 Selection edit=all;for(unsigned i=0;i<342;++i)edit.set(i,false);assert(edit.count()==0);edit.normalize();assert(edit.count()==1&&edit.selected(0));
}
