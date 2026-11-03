#include "apps/app_launcher/view/screensaver_styles.hpp"
#include "apps/app_launcher/view/screensaver_library.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <sstream>
#include <vector>

// Old persisted bitmaps must be constructed without Selection::set, which
// intentionally refuses removed IDs in the new firmware.
static std::string oldBits(std::initializer_list<unsigned> ids) {
    std::array<uint8_t,43> bytes{};
    for(auto id:ids)bytes[id/8]|=uint8_t(1u<<(id%8));
    constexpr char hex[]="0123456789abcdef";
    std::string out;
    for(auto byte:bytes){out+=hex[byte>>4];out+=hex[byte&15];}
    return out;
}
static uint64_t clockCalls=0;
static uint64_t clockNow(){++clockCalls;return 0;}

int main() {
    using namespace view::saver;
    // ID226 is Moire (catalog index225); Moire2 is a separate retained ID225.
    assert(!Allowed(226)&&"reported Moire must be removed from the stable selection policy");
    assert(SlideCount==342&&effect_catalog::Count()==341);
    assert(std::string(effect_catalog::Name(223))=="Moebiusgears");
    assert(std::string(effect_catalog::Name(224))=="Moire2");
    assert(std::string(effect_catalog::Name(225))=="Unavailable");
    assert(std::string(effect_catalog::Name(226))=="Morph3d");
    assert(std::string(effect_catalog::Name(227))=="Munch");
    assert(std::string(effect_catalog::Name(339))=="Bouboule");
    assert(effect_catalog::Usable(224)&&!effect_catalog::Usable(225)&&effect_catalog::Usable(226));
    assert(FromStored(226)==Style::Dvd);
    assert(static_cast<unsigned>(FromStored(225))==225&&static_cast<unsigned>(FromStored(227))==227);

    std::istringstream options(CatalogOptions());std::string name;bool retained=false;
    while(std::getline(options,name)){assert(name!="Moire");retained|=name=="Moire2";}
    assert(retained);
    auto defaults=Selection::defaults();
    // Defaults include DVD and every implemented retained effect. Stable catalog
    // tombstones must remain unselected after the separately approved pruning.
    unsigned usable=1;assert(defaults.selected(0));
    for(unsigned id=1;id<SlideCount;++id){const bool implemented=effect_catalog::Usable(uint16_t(id-1));assert(defaults.selected(id)==implemented);usable+=implemented;}
    assert(defaults.count()==usable&&!defaults.selected(226)&&defaults.selected(225)&&defaults.selected(227));
    Library library;
    assert(!library.visible(226)&&!library.inView(226,true));
    library.setHidden(226,true);assert(!library.hidden.selected(226));
    assert(library.adjacent(225,true,false)==227&&library.adjacent(227,false,false)==225);
    assert(AdjacentAllowed(225,true)==227&&AdjacentAllowed(227,false)==225);

    auto only=Selection::fromPayload("1:1:"+oldBits({226}));
    assert(only.count()==1&&only.selected(0)&&!(only.bits[28]&4));
    assert(only.order==Order::Random);
    Slideshow fallback;assert(fallback.start(only,0)==0);
    assert(!fallback.advance(30000,[]{return 0;}));

    const std::string saved="2:1:"+oldBits({1,225,226,227,340})+":"+oldBits({225,226});
    auto migrated=Library::fromPayload(saved);
    assert(migrated.selection.count()==4&&!migrated.selection.selected(226));
    for(auto id:{1u,225u,227u,340u})assert(migrated.selection.selected(id));
    assert(migrated.selection.order==Order::Random);
    assert(migrated.hidden.selected(225)&&!migrated.hidden.selected(226));
    auto effective=migrated.effective();assert(effective.count()==3&&!effective.selected(225));
    for(auto id:{1u,227u,340u})assert(effective.selected(id));
    auto roundtrip=Library::fromPayload(migrated.payload());
    assert(roundtrip.selection.payload()==migrated.selection.payload());
    assert(roundtrip.hidden.encode()==migrated.hidden.encode());
    const auto all_removed=Library::fromPayload("2:0:"+oldBits({226})+":"+oldBits({226}));
    assert(all_removed.effective().count()==1&&all_removed.effective().selected(0));

    for(auto mode:{Order::Sequential,Order::Random}){
        auto selection=Selection::fromPayload("1:0:"+oldBits({225,226,227}));selection.order=mode;
        Slideshow show;assert(show.start(selection,0)==225);
        for(unsigned step=1;step<=20;++step){
            assert(show.advance(step*30000,[]{return 12345;}));
            assert(show.current()==225||show.current()==227);
        }
    }
    // A stale direct preview/runtime dispatch must reject the tombstone before
    // touching the framebuffer or clock, even with unlimited wall-clock time.
    std::vector<uint16_t> pixels(320*240,0x5aad);
    effect_catalog::Reset(225);
    assert(!effect_catalog::Render(225,0,pixels.data(),320,240,clockNow));
    assert(!clockCalls&&std::all_of(pixels.begin(),pixels.end(),[](auto p){return p==0x5aad;}));
    puts("Moire ID226 removed; other stable IDs, saved selections/order/visibility and DVD fallback retained");
}
