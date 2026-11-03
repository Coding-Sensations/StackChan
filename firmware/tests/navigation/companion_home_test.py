"""Exercise the real swipe/Home state machines at inert UI/HAL boundaries."""
from pathlib import Path
import subprocess, tempfile
main=Path(__file__).resolve().parents[3]/'firmware/main'
s=(main/'apps/common/home_indicator/home_indicator.cpp').read_text()
classes=s[s.index('class HomeGesture {'):s.index('namespace view {')]
fixture=r'''
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <cstdio>
struct lv_obj_t {bool hidden=false;};struct lv_image_dsc_t{};
using lv_indev_state_t=int;struct lv_point_t{int x,y;};struct lv_indev_t{int state=0;lv_point_t point{};};
constexpr int LV_INDEV_STATE_REL=0,LV_INDEV_STATE_PR=1,LV_ALIGN_CENTER=0,LV_ALIGN_BOTTOM_MID=0,
 LV_OBJ_FLAG_CLICKABLE=0,LV_OBJ_FLAG_FLOATING=0,LV_OBJ_FLAG_SCROLLABLE=0,LV_OPA_COVER=255,LV_LABEL_LONG_WRAP=0;
int lv_font_montserrat_14;int lv_color_hex(uint32_t c){return c;}
std::string visible_status;bool overlay_hidden=true;unsigned created=0;
void lv_label_set_text(lv_obj_t*,const char* s){visible_status=s;}
lv_obj_t* lv_label_create(lv_obj_t*){static lv_obj_t l;return &l;}
void lv_obj_set_pos(lv_obj_t*,int,int){}void lv_obj_set_size(lv_obj_t*,int,int){}
void lv_label_set_long_mode(lv_obj_t*,int){}void lv_obj_set_style_text_font(lv_obj_t*,const int*,int){}
void lv_obj_set_style_text_color(lv_obj_t*,int,int){}
void lv_obj_set_style_bg_color(lv_obj_t*,int,int){}void lv_obj_set_style_bg_opa(lv_obj_t*,int,int){}
struct Signal{std::function<void()> callback;template<class F>void connect(F f){callback=f;}};
struct Container{lv_obj_t obj;Signal signal;explicit Container(lv_obj_t*){++created;}virtual ~Container()=default;
 lv_obj_t* get(){return &obj;}void align(int,int,int){}void setSize(int,int){}void setRadius(int){}
 void setBgOpa(int){}void setBorderWidth(int){}void addFlag(int){}void removeFlag(int){}
 void setPadding(int,int,int,int){}Signal& onClick(){return signal;}void setHidden(bool v){obj.hidden=v;overlay_hidden=v;}
 void moveForeground(){}void setPos(int,int){}void setAlign(int){}void setBgColor(int){}
 void setBorderColor(int){}void setShadowWidth(int){}
};
Signal* home_click=nullptr;
struct Button:Container{explicit Button(lv_obj_t* p):Container(p){home_click=&signal;}};
struct Image:Container{using Container::Container;void setSrc(lv_image_dsc_t*){}void setImageRecolorOpa(int){}void setImageRecolor(int){}};
struct Options{double bounce=0,visualDuration=0;};
struct AnimateValue{int v=0;Options o;Options& springOptions(){return o;}void teleport(int n){v=n;}void update(){}bool done(){return true;}int directValue(){return v;}void operator=(int n){v=n;}};
namespace assets{lv_image_dsc_t get_image(const char*){return {};}}
struct HAL{lv_indev_t input;lv_indev_t* lvTouchpad=&input;uint32_t now=0;uint32_t millis(){return now;}};
HAL& GetHAL(){static HAL h;return h;}
int lv_indev_get_state(lv_indev_t* p){return p->state;}void lv_indev_get_point(lv_indev_t* p,lv_point_t* out){*out=p->point;}
'''
tests=r'''
void touch(HomeIndicator& h,int state,int x,int y){GetHAL().input={state,{x,y}};h.update();}
void swipe(HomeIndicator& h){touch(h,LV_INDEV_STATE_PR,160,235);touch(h,LV_INDEV_STATE_REL,160,150);}
int main(){lv_obj_t parent;unsigned homes=0;const char* status="Stackchan - wake word on";
 for(unsigned i=0;i<20;++i){
  HomeIndicator h;h.onGoHome=[&]{++homes;};h.init(&parent,0,0,false,[&]{return status;});
  assert(overlay_hidden&&"companion must start with a clean face");
  touch(h,1,160,100);touch(h,0,160,10);assert(overlay_hidden); // non-bottom gesture ignored
  touch(h,1,160,235);touch(h,0,250,210);assert(overlay_hidden); // short/horizontal ignored
  swipe(h);assert(!overlay_hidden&&visible_status==status&&homes==i);
  home_click->callback();h.update();assert(homes==i+1);h.update();assert(homes==i+1);
  status="Stop pending; ownership retained";h.update();assert(visible_status==status);
  GetHAL().now+=3001;h.update();h.update();assert(overlay_hidden);
  swipe(h);assert(!overlay_hidden); // repeat reveal remains reachable
 }
 HomeIndicator legacy;legacy.init(&parent,0,0);assert(!overlay_hidden); // other apps preserve first-show behavior
 puts("20 clean entry/swipe/reveal/Home/status/auto-hide cycles passed");
}
'''
with tempfile.TemporaryDirectory(prefix='companion-home-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture+classes+tests)
 subprocess.run(['clang++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
