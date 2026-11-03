#pragma once
// Inert LVGL boundary: tracks object ownership, inherited visibility, event
// callbacks and each lock release. No display, transport or hardware is used.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct lv_obj_t;
struct lv_event_t {lv_obj_t* target;void* user;int code=1;};
struct lv_timer_t {void (*callback)(lv_timer_t*);void* user;bool deleted=false;};
struct lv_obj_t {
 lv_obj_t* parent=nullptr;bool hidden=false,deleted=false,disabled=false;std::string text;
 int x=0,y=0,w=0,h=0;uint32_t color=0;int opacity=0;int scale=256;const void* image_source=nullptr;int radius=0;std::string kind="panel";void* user=nullptr;
 void (*callback)(lv_event_t*)=nullptr;void* callback_user=nullptr;void (*pressed_callback)(lv_event_t*)=nullptr;void* pressed_user=nullptr;
};
inline lv_obj_t screen,top;
inline std::vector<std::unique_ptr<lv_obj_t>> objects;
inline std::vector<std::unique_ptr<lv_timer_t>> timers;
inline std::function<void()> on_unlock;
inline unsigned lock_depth=0;
struct LvglLockGuard {
 LvglLockGuard(){++lock_depth;}
 ~LvglLockGuard(){if(!--lock_depth&&on_unlock)on_unlock();}
};
inline constexpr int LV_OBJ_FLAG_HIDDEN=1,LV_OBJ_FLAG_SCROLLABLE=2,
 LV_EVENT_CLICKED=1,LV_EVENT_PRESSED=3,LV_LABEL_LONG_DOT=1,LV_OPA_TRANSP=0,
 LV_STATE_DISABLED=1,lv_font_montserrat_16=16,lv_font_montserrat_14=14;
#define LV_SYMBOL_LIST "LIST_ICON"
inline bool Visible(lv_obj_t* p){return p&&!p->deleted&&!p->hidden&&(!p->parent||Visible(p->parent));}
inline lv_obj_t* lv_obj_create(lv_obj_t* parent){auto p=std::make_unique<lv_obj_t>();p->parent=parent;auto raw=p.get();objects.push_back(std::move(p));return raw;}
inline lv_obj_t* lv_label_create(lv_obj_t* p){auto o=lv_obj_create(p);o->kind="text";return o;}
inline lv_obj_t* lv_button_create(lv_obj_t* p){auto o=lv_obj_create(p);o->kind="button";return o;}
inline void lv_obj_delete(lv_obj_t* p){assert(p&&!p->deleted);for(auto& child:objects)if(child->parent==p&&!child->deleted)lv_obj_delete(child.get());p->deleted=true;}
inline void lv_obj_remove_flag(lv_obj_t* p,int f){if(f==LV_OBJ_FLAG_HIDDEN)p->hidden=false;}
inline void lv_obj_add_flag(lv_obj_t* p,int f){if(f==LV_OBJ_FLAG_HIDDEN)p->hidden=true;}
inline void lv_obj_set_pos(lv_obj_t* p,int x,int y){p->x=x;p->y=y;}
inline void lv_obj_set_size(lv_obj_t* p,int w,int h){p->w=w;p->h=h;}
inline void lv_obj_set_width(lv_obj_t* p,int w){p->w=w;}
inline void lv_obj_set_height(lv_obj_t* p,int h){p->h=h;}
inline void lv_label_set_text(lv_obj_t* p,const char* text){assert(p&&!p->deleted);p->text=text;}
inline void lv_obj_set_user_data(lv_obj_t* p,void* data){p->user=data;}
inline void* lv_obj_get_user_data(lv_obj_t* p){return p->user;}
inline void* lv_event_get_user_data(lv_event_t* e){return e->user;}
inline lv_obj_t* lv_event_get_target(lv_event_t* e){return e->target;}
inline int lv_event_get_code(lv_event_t* event){return event->code;}
inline void lv_obj_add_event_cb(lv_obj_t* p,void (*callback)(lv_event_t*),int code,void* user){if(code==LV_EVENT_PRESSED){p->pressed_callback=callback;p->pressed_user=user;}else{p->callback=callback;p->callback_user=user;}}
inline void Press(lv_obj_t* p){if(p->pressed_callback){lv_event_t e{p,p->pressed_user,LV_EVENT_PRESSED};p->pressed_callback(&e);}}
inline void ReleaseClick(lv_obj_t* p){assert(Visible(p)&&p->callback&&!p->disabled);lv_event_t e{p,p->callback_user,LV_EVENT_CLICKED};p->callback(&e);}
inline void Click(lv_obj_t* p){assert(Visible(p)&&p->callback&&!p->disabled);Press(p);ReleaseClick(p);}
inline lv_obj_t* Text(const std::string& text){for(auto& p:objects)if(Visible(p.get())&&p->text==text)return p.get();return nullptr;}
inline lv_obj_t* Button(const std::string& text){auto label=Text(text);return label&&label->parent&&label->parent->kind=="button"?label->parent:nullptr;}
inline unsigned LiveObjects(){return std::count_if(objects.begin(),objects.end(),[](auto& p){return !p->deleted;});}
inline lv_obj_t* lv_screen_active(){return &screen;}
inline lv_obj_t* lv_layer_top(){return &top;}
inline int lv_color_hex(uint32_t c){return c;}
// This inert fixture already applies sizes immediately; real rendering is tested separately.
inline void lv_obj_update_layout(lv_obj_t*){}
inline int lv_obj_get_width(lv_obj_t* p){return p->w;}
inline int lv_obj_get_height(lv_obj_t* p){return p->h;}
inline lv_obj_t* lv_obj_get_child(lv_obj_t* p,int index){for(auto& c:objects)if(c->parent==p&&!c->deleted){if(index--==0)return c.get();}return nullptr;}
inline void lv_obj_add_state(lv_obj_t* p,int){p->disabled=true;}
inline void lv_obj_remove_state(lv_obj_t* p,int){p->disabled=false;}
inline void lv_obj_center(lv_obj_t* p){if(p->parent){p->x=(p->parent->w-p->w)/2;p->y=(p->parent->h-p->h)/2;}}
inline void lv_label_set_long_mode(lv_obj_t*,int){}
inline void lv_obj_set_style_pad_all(lv_obj_t*,int,int){}
inline void lv_obj_set_style_bg_color(lv_obj_t* p,int color,int){p->color=color;}
inline void lv_obj_set_style_text_color(lv_obj_t*,int,int){}
inline void lv_obj_set_style_text_font(lv_obj_t*,const int*,int){}
inline void lv_obj_set_style_bg_opa(lv_obj_t* p,int opacity,int){p->opacity=opacity;}
inline void lv_obj_set_style_border_width(lv_obj_t*,int,int){}
inline void lv_obj_set_style_shadow_width(lv_obj_t*,int,int){}
inline lv_timer_t* lv_timer_create(void (*callback)(lv_timer_t*),int,void* user){auto timer=std::make_unique<lv_timer_t>();timer->callback=callback;timer->user=user;auto p=timer.get();timers.push_back(std::move(timer));return p;}
inline void lv_timer_delete(lv_timer_t* p){assert(!p->deleted);p->deleted=true;}
inline void* lv_timer_get_user_data(lv_timer_t* p){return p->user;}
inline void TickTimers(){for(auto& p:timers)if(!p->deleted)p->callback(p.get());}

inline constexpr int LV_RADIUS_CIRCLE=32767;
inline void lv_obj_remove_style_all(lv_obj_t*){}
inline void lv_obj_set_style_radius(lv_obj_t* p,int radius,int){p->radius=radius;}

#define LV_USE_LODEPNG 1
inline constexpr int LV_IMAGE_HEADER_MAGIC=0x19,LV_COLOR_FORMAT_RAW=1,LV_COLOR_FORMAT_ARGB8888=2;
struct lv_image_header_t {int magic=0,cf=0,w=0,h=0,stride=0;};
struct lv_image_dsc_t {lv_image_header_t header;uint32_t data_size=0;const unsigned char* data=nullptr;};
inline lv_obj_t* lv_image_create(lv_obj_t* p){auto o=lv_obj_create(p);o->kind="image";return o;}
inline void lv_image_set_src(lv_obj_t* p,const lv_image_dsc_t* s){p->image_source=s;p->w=s->header.w;p->h=s->header.h;}
inline void lv_image_set_pivot(lv_obj_t*,int,int){}
inline void lv_image_set_scale(lv_obj_t* p,int s){p->scale=s;}

inline constexpr int LV_ALIGN_CENTER=0;
inline void lv_obj_align(lv_obj_t* p,int align,int x,int y){assert(align==LV_ALIGN_CENTER);lv_obj_center(p);p->x+=x;p->y+=y;}
inline void lv_image_set_antialias(lv_obj_t*,bool){}
inline void lv_obj_set_style_opa(lv_obj_t* p,int opacity,int){p->opacity=opacity;}

inline bool lv_obj_has_flag(lv_obj_t* p,int flag){return flag==LV_OBJ_FLAG_HIDDEN&&p->hidden;}
