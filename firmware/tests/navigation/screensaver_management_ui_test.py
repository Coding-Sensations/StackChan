"""Run the production Settings worker/catalog against LVGL, clock and NVS boundaries.
No device I/O. ASan checks borrowed preview lifetime; NVS failures/consent are injected.
"""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]/'main'
source=(root/'apps/app_setup/workers/screensaver_setup.cpp').read_text()
source='\n'.join(line for line in source.splitlines() if not line.startswith('#include') and not line.startswith('#ifdef') and not line.startswith('#endif'))
fixture=r'''
#include "apps/app_launcher/view/screensaver_styles.hpp"
#include "apps/app_launcher/view/screensaver_library.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
using lv_obj_flag_t=int;
struct lv_obj_t;
struct lv_event_t{void* user;};
using Handler=void(*)(lv_event_t*);
struct lv_obj_t{lv_obj_t* parent=nullptr;std::vector<lv_obj_t*> children;std::string text;void* borrowed=nullptr;bool hidden=false;Handler handler=nullptr;void* user=nullptr;};
lv_obj_t screen;std::vector<lv_obj_t*> live;
constexpr int LV_EVENT_CLICKED=1,LV_OBJ_FLAG_HIDDEN=2,LV_OBJ_FLAG_SCROLLABLE=4,LV_OBJ_FLAG_CLICKABLE=8,LV_OPA_COVER=255,LV_LABEL_LONG_DOT=0,LV_COLOR_FORMAT_RGB565=0,MALLOC_CAP_SPIRAM=0,MALLOC_CAP_8BIT=0;
void* LV_FONT_DEFAULT=nullptr;
lv_obj_t* lv_screen_active(){return &screen;}
lv_obj_t* lv_obj_create(lv_obj_t* parent){auto* o=new lv_obj_t;o->parent=parent;if(parent)parent->children.push_back(o);live.push_back(o);return o;}
lv_obj_t* lv_label_create(lv_obj_t* p){return lv_obj_create(p);}
lv_obj_t* lv_button_create(lv_obj_t* p){return lv_obj_create(p);}
lv_obj_t* lv_canvas_create(lv_obj_t* p){return lv_obj_create(p);}
void lv_obj_delete(lv_obj_t* o){while(!o->children.empty())lv_obj_delete(o->children.back());if(o->parent){auto& c=o->parent->children;c.erase(std::find(c.begin(),c.end(),o));}live.erase(std::find(live.begin(),live.end(),o));delete o;}
bool lv_obj_is_valid(lv_obj_t* o){return std::find(live.begin(),live.end(),o)!=live.end();}
bool lv_obj_is_visible(lv_obj_t* o){return !o->hidden&&(!o->parent||o->parent==&screen||lv_obj_is_visible(o->parent));}
void lv_obj_add_flag(lv_obj_t* o,int flag){if(flag&LV_OBJ_FLAG_HIDDEN)o->hidden=true;}
void lv_obj_remove_flag(lv_obj_t* o,int flag){if(flag&LV_OBJ_FLAG_HIDDEN)o->hidden=false;}
void lv_obj_set_pos(lv_obj_t*,int,int){} void lv_obj_set_size(lv_obj_t*,int,int){} void lv_obj_set_width(lv_obj_t*,int){} void lv_obj_center(lv_obj_t*){} void lv_obj_invalidate(lv_obj_t*){}
void lv_obj_set_style_pad_all(lv_obj_t*,int,int){} void lv_obj_set_style_border_width(lv_obj_t*,int,int){} void lv_obj_set_style_radius(lv_obj_t*,int,int){}
uint32_t lv_color_hex(uint32_t x){return x;}
void lv_obj_set_style_bg_color(lv_obj_t*,uint32_t,int){} void lv_obj_set_style_bg_opa(lv_obj_t*,int,int){} void lv_obj_set_style_text_font(lv_obj_t*,void*,int){}
void lv_label_set_long_mode(lv_obj_t*,int){}
void lv_label_set_text(lv_obj_t* o,const char* s){o->text=s;}
void lv_label_set_text_fmt(lv_obj_t* o,const char* s,...){char text[2048];va_list args;va_start(args,s);vsnprintf(text,sizeof text,s,args);va_end(args);o->text=text;}
void lv_canvas_set_buffer(lv_obj_t* o,void* ptr,int,int,int){o->borrowed=ptr;}
void lv_obj_add_event_cb(lv_obj_t* o,Handler h,int,void* user){o->handler=h;o->user=user;}
void* lv_event_get_user_data(lv_event_t* e){return e->user;}
void* heap_caps_malloc(size_t n,int){return malloc(n);}
void heap_caps_free(void* p){for(auto* o:live)assert(o->borrowed!=p);free(p);}
static auto clock_start=std::chrono::steady_clock::now();static uint32_t offset=0;
struct Hal{uint32_t millis(){return offset+std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-clock_start).count();}};
Hal& GetHAL(){static Hal hal;return hal;}
int64_t esp_timer_get_time(){return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-clock_start).count();}
static std::string stored,pending;static int fail_stage=0;static bool consent=true;
struct Settings{Settings(const char*){}std::string GetString(const char*){return stored;}};
namespace companion{bool SettingsWriteAllowed(const char* ns,const char* key){assert(std::string(ns)=="screensaver"&&std::string(key)=="playlist");return consent;}}
using nvs_handle_t=int;constexpr int ESP_OK=0,NVS_READWRITE=1,NVS_READONLY=2;
int nvs_open(const char*,int mode,nvs_handle_t* h){*h=mode;return fail_stage==mode?-1:0;}
int nvs_set_str(nvs_handle_t,const char*,const char* value){pending=value;return fail_stage==3?-1:0;}
int nvs_commit(nvs_handle_t){if(fail_stage==4)return -1;stored=pending;return 0;}
void nvs_close(nvs_handle_t){}
int nvs_get_str(nvs_handle_t,const char*,char* out,size_t* size){if(fail_stage==5)return -1;if(*size<stored.size()+1)return -1;*size=stored.size()+1;memcpy(out,stored.c_str(),*size);return 0;}
namespace setup_workers{class WorkerBase{protected:bool _is_done=false;public:virtual ~WorkerBase()=default;virtual void update(){}bool isDone(){return _is_done;}};}
bool textContains(const char* text){for(auto* o:live)if(lv_obj_is_visible(o)&&o->text.find(text)!=std::string::npos)return true;return false;}
void click(setup_workers::WorkerBase& worker,const char* text){for(auto* o:live)if(o->text==text&&o->parent&&o->parent->handler&&lv_obj_is_visible(o)){lv_event_t event{o->parent->user};o->parent->handler(&event);worker.update();return;}fprintf(stderr,"Missing button: %s\n",text);abort();}
'''
checks=r'''
int main(){
 using namespace view::saver;
 auto original=Selection::defaults();original.set(2,false);original.order=Order::Random;stored=original.payload();
 {
  auto worker=setup_workers::MakeScreensaverWorker();
  click(*worker,"Manage");click(*worker,">");click(*worker,"Mark");click(*worker,"Hide marked");
  assert(textContains("no storage")&&textContains(Name(static_cast<Style>(1))));
  click(*worker,"Cancel");click(*worker,"Back");click(*worker,"Save");assert(worker->isDone());
  assert(Library::fromPayload(stored).visible(1));
 }
 assert(live.empty());
 {
  auto worker=setup_workers::MakeScreensaverWorker();click(*worker,"Manage");click(*worker,">");click(*worker,"Mark");click(*worker,"Hide marked");click(*worker,"Confirm");
  assert(stored==original.payload()||Library::fromPayload(stored).visible(1)); // changes staged until Save
  consent=false;click(*worker,"Save");assert(!worker->isDone()&&textContains("Save not verified"));assert(Library::fromPayload(stored).visible(1));
  consent=true;auto beforeFailures=stored;for(int fail:{1,3,4,2,5}){fail_stage=fail;click(*worker,"Save");assert(!worker->isDone()&&textContains("Save not verified"));if(fail==1||fail==3||fail==4)assert(stored==beforeFailures);}
  fail_stage=0;click(*worker,"Save");assert(worker->isDone());
 }
 assert(live.empty());auto saved=Library::fromPayload(stored);assert(!saved.visible(1)&&saved.selection.payload()==original.payload());
 {
  auto worker=setup_workers::MakeScreensaverWorker();click(*worker,"Manage");click(*worker,"Hidden");click(*worker,"Mark");click(*worker,"Restore marked");click(*worker,"Confirm");click(*worker,"Save");assert(worker->isDone());
 }
 assert(Library::fromPayload(stored).visible(1)&&Library::fromPayload(stored).selection.payload()==original.payload());
 // Closing with a staged visibility change writes nothing.
 auto before=stored;{
  auto worker=setup_workers::MakeScreensaverWorker();click(*worker,"Manage");click(*worker,">");click(*worker,"Mark");click(*worker,"Hide marked");click(*worker,"Confirm");
  offset+=61;worker->update();
 }
 assert(stored==before&&live.empty());
 // Preview owns two borrowed buffers and cleans them on repeated real worker exits.
 for(unsigned n=0;n<20;++n){auto worker=setup_workers::MakeScreensaverWorker();click(*worker,">");offset+=61;worker->update();}
 assert(live.empty());
stored = original.payload();
auto unchanged = stored;
{
    auto worker = setup_workers::MakeScreensaverWorker();
    click(*worker, "Manage");
    click(*worker, "Hidden");
    assert(textContains("Nothing hidden"));
    click(*worker, "Mark");
    click(*worker, "Restore marked");
    assert(!textContains("Restore marked?"));

    click(*worker, "Visible");
    click(*worker, ">");
    click(*worker, "Mark");
    click(*worker, ">");
    click(*worker, "Mark");
    click(*worker, "Hide marked");
    assert(textContains(Name(static_cast<Style>(1))));
    assert(textContains(Name(static_cast<Style>(2))));
    click(*worker, "Confirm");
    assert(stored == unchanged);
    click(*worker, "Save");
    assert(worker->isDone());
}
auto hidden = Library::fromPayload(stored);
assert(!hidden.visible(1) && !hidden.visible(2));
assert(hidden.selection.payload() == original.payload());
{
    auto worker = setup_workers::MakeScreensaverWorker();
    click(*worker, "Manage");
    click(*worker, "Hidden");
    click(*worker, "Mark");
    click(*worker, ">");
    click(*worker, "Mark");
    click(*worker, "Restore marked");
    click(*worker, "Confirm");
    assert(textContains("Nothing hidden"));
    click(*worker, "Save");
    assert(worker->isDone());
}
assert(live.empty());
auto restored = Library::fromPayload(stored);
assert(restored.visible(1) && restored.visible(2));
assert(restored.selection.payload() == original.payload());

// If every custom choice is hidden and DVD was not selected, playback falls
// back without adding DVD to the separately saved user's selection.
Library allHidden;allHidden.selection.bits.fill(0);allHidden.selection.set(1,true);
for(unsigned id=1;id<SlideCount;++id)allHidden.setHidden(id,true);
stored=allHidden.payload();
{
    auto worker=setup_workers::MakeScreensaverWorker();
    assert(textContains("DVD"));click(*worker,"Save");assert(worker->isDone());
}
auto fallback=Library::fromPayload(stored);
assert(!fallback.selection.selected(0)&&fallback.selection.selected(1));
assert(fallback.effective().count()==1&&fallback.effective().selected(0));
assert(live.empty());

puts("management UI: cancel, restore, consent, save failures, restart, clock and preview cleanup passed");
}
'''
with tempfile.TemporaryDirectory(prefix='screensaver-management-ui-') as temporary:
 path=Path(temporary);(path/'test.cpp').write_text(fixture+source+checks)
 subprocess.run(['clang++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-I'+str(root),str(path/'test.cpp'),str(root/'apps/app_launcher/view/effect_catalog/effect_catalog.cpp'),'-o',str(path/'test')],check=True)
 subprocess.run([str(path/'test')],check=True)
