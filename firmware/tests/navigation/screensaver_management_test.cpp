#include "apps/app_launcher/view/screensaver_library.hpp"
#include <cassert>
using namespace view::saver;
int main() {
    auto legacy=Selection::defaults();legacy.set(2,false);legacy.order=Order::Random;
    auto library=Library::fromPayload(legacy.payload());
    assert(library.selection.payload()==legacy.payload());
    assert(library.hidden.count()==0&&library.visible(0));
    library.setHidden(1,true);library.setHidden(2,true);library.setHidden(0,true);
    assert(!library.visible(1)&&library.visible(0));
    auto restart=Library::fromPayload(library.payload());
    assert(restart.selection.payload()==legacy.payload());
    assert(!restart.effective().selected(1)&&!restart.effective().selected(2));
    restart.setHidden(1,false);restart.setHidden(2,false);
    assert(restart.effective().selected(1)&&!restart.effective().selected(2));
    for(unsigned id=1;id<SlideCount;++id)restart.setHidden(id,true);
    restart.selection.set(0,false);
    assert(restart.effective().count()==1&&restart.effective().selected(0));
    auto persisted=Library::fromPayload(restart.payload());
    assert(!persisted.selection.selected(0)&&persisted.effective().selected(0));
    // An invalid hidden suffix must retain a valid user's playlist, not select all.
    auto damaged=Library::fromPayload(library.payload().substr(0,91)+"oops");
    assert(damaged.selection.payload()==legacy.payload()&&damaged.hidden.count()==0);
    assert(Library::fromPayload("").effective().count()==AllowedCount);
    for(auto id:ExcludedSlides){library.setHidden(id,true);assert(!library.hidden.selected(id));}
    assert(library.adjacent(0,true,false)!=1);
    library.setHidden(1,true);assert(library.adjacent(0,true,true)==1);
    assert(library.inView(1,true)&&!library.inView(0,true));
}
