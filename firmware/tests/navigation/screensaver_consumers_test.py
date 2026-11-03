"""Catch a preview deadline being applied to complete-frame UI consumers.

Compile actual playback/error/preview method bodies with the real catalog.
Only LVGL display objects and the ESP clock are inert boundary fixtures.
The clock models 25us between raster checkpoints; this is not device profiling.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / 'main'


def method(source, signature):
    start = source.index(signature)
    begin = source.index('{', start)
    depth, end = 1, begin + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


playback = (root / 'apps/app_launcher/view/screensaver.cpp').read_text()
preview = (root / 'apps/app_setup/workers/screensaver_setup.cpp').read_text()
methods = '\n'.join(method(playback, signature) for signature in (
    'void Screensaver::showEffectError()', 'void Screensaver::onRender(float dt)'))
preview_method = method(preview, 'void preview(uint32_t elapsed)')

fixture = r'''
#include "apps/app_launcher/view/screensaver_styles.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
using namespace view;
static const struct{int width=64,height=48;} _logo_size;
static uint64_t tick=0;
int64_t esp_timer_get_time(){tick+=25;return tick;}
struct lv_obj_t{bool hidden=false;unsigned invalidations=0;std::string text;};
constexpr int LV_OBJ_FLAG_HIDDEN=1;
void lv_obj_add_flag(lv_obj_t* object,int){object->hidden=true;}
void lv_obj_invalidate(lv_obj_t* object){++object->invalidations;}
void lv_label_set_text(lv_obj_t* object,const char* text){object->text=text;}
lv_obj_t* lv_label_create(lv_obj_t*){return new lv_obj_t;}
void lv_obj_set_width(lv_obj_t*,int){}
uint32_t lv_color_hex(uint32_t c){return c;}
void lv_obj_set_style_text_color(lv_obj_t*,uint32_t,int){}
void lv_obj_center(lv_obj_t*){}
struct Screen{lv_obj_t object;lv_obj_t* get(){return &object;}};
struct Transform{struct{double x=0,y=0;}position;};
struct GameObject{int groupId=0;template<class T>T* get(){static T t;return &t;}};
struct World{template<class F>void forEachObject(F){}};
struct Logo{void setPos(int,int){}};
struct Screensaver{
 view::saver::Style _style=view::saver::Style::Dvd;
 bool _effect_failed=false,active=true;
 lv_obj_t* _effect_canvas=nullptr;uint16_t* _effect_pixels=nullptr;
 lv_obj_t* _effect_error=nullptr;
 uint32_t _animation_ms=0;int _logo_id=666;
 std::unique_ptr<Screen> _screen=std::make_unique<Screen>();
 std::unique_ptr<Logo> _logo=std::make_unique<Logo>();
 ~Screensaver(){delete _effect_error;}
 World& getWorld(){static World world;return world;}
 bool isActiveScreen()const{return active;}
 void showEffectError();void onRender(float dt);
};
struct Preview{
 bool preview_failed_=false;
 lv_obj_t* confirmation_=nullptr;
 bool currentInView()const{return true;}
 void message(const char* text){lv_label_set_text(count_,text);}
 uint16_t* pixels_=nullptr;uint16_t* thumbnail_=nullptr;
 lv_obj_t* canvas_=nullptr;lv_obj_t* count_=nullptr;
 unsigned id_=0;
'''

checks = r'''
int main(){
 unsigned selectable=0,playback_visible=0,preview_visible=0,painted=0;
 for(unsigned id=1;id<342;++id)if(view::saver::Allowed(id)){
  ++selectable;
  std::vector<uint16_t> guarded(320*240+2,0xa55a),thumbnail(160*120);
  for(auto elapsed:{0u,1000u,29999u}){
   lv_obj_t canvas,count;
   Screensaver player;player._style=static_cast<view::saver::Style>(id);
   player._effect_canvas=&canvas;player._effect_pixels=guarded.data()+1;player._animation_ms=elapsed;
   effect_catalog::Reset(uint16_t(id-1));tick=0;
   player.onRender(0);
   if(elapsed==0&&!canvas.hidden&&!player._effect_failed&&canvas.invalidations)++playback_visible;
   assert(guarded.front()==0xa55a&&guarded.back()==0xa55a);
   if(!canvas.hidden){
    assert(!player._effect_error);
    const auto first_frame=guarded;
    player.onRender(0.06f);
    assert(!player._effect_failed&&player._animation_ms==elapsed+60);
    // Visible but frozen consumers must fail too. Carousel changes geometry each
    // frame; intentionally static catalog entries have no such requirement.
    if(std::string(effect_catalog::Name(uint16_t(id-1)))=="Carousel")assert(guarded!=first_frame);
   }
   Preview preview;preview.id_=id;preview.canvas_=&canvas;preview.count_=&count;
   preview.pixels_=guarded.data()+1;preview.thumbnail_=thumbnail.data();
   canvas={};effect_catalog::Reset(uint16_t(id-1));tick=0;
   preview.preview(elapsed);
   if(elapsed==0&&!canvas.hidden&&!preview.preview_failed_&&canvas.invalidations)++preview_visible;
   if(!canvas.hidden){
    const auto first_frame=guarded;
    preview.preview(elapsed+60);assert(!preview.preview_failed_);
    if(std::string(effect_catalog::Name(uint16_t(id-1)))=="Carousel")assert(guarded!=first_frame);
   }
   if(std::any_of(thumbnail.begin(),thumbnail.end(),[](uint16_t v){return v!=0;}))++painted;
   assert(guarded.front()==0xa55a&&guarded.back()==0xa55a);
  }
 }
 std::printf("selectable=%u playback_visible=%u preview_visible=%u painted_samples=%u\n",selectable,playback_visible,preview_visible,painted);std::fflush(stdout);
 assert(selectable==view::saver::AllowedCount-1);
 assert(playback_visible==selectable);assert(preview_visible==selectable);assert(painted>selectable);
 // Invalid modes remain actual errors; valid slow timing must not reach this UI.
 lv_obj_t canvas;std::vector<uint16_t> pixels(320*240);
 Screensaver invalid;invalid._style=static_cast<view::saver::Style>(65535);
 invalid._effect_canvas=&canvas;invalid._effect_pixels=pixels.data();invalid.onRender(0);
 assert(invalid._effect_failed&&canvas.hidden&&invalid._effect_error&&!invalid._effect_error->text.empty());
 // Inactive screens and missing preview storage must not render into borrowed buffers.
 Screensaver inactive;inactive.active=false;inactive._effect_pixels=pixels.data();inactive.onRender(0);
 Preview missing;lv_obj_t absent;missing.canvas_=&absent;missing.preview(0);assert(!absent.invalidations);
}
'''

with tempfile.TemporaryDirectory(prefix='stackchan-screensaver-consumers-') as temporary:
    path = Path(temporary)
    (path / 'test.cpp').write_text(fixture + preview_method + '\n};\n' + methods + checks)
    subprocess.run(['clang++', '-std=c++17', '-O1', '-g', '-fsanitize=address,undefined',
                    '-I' + str(root), str(path / 'test.cpp'),
                    str(root / 'apps/app_launcher/view/effect_catalog/effect_catalog.cpp'),
                    '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
