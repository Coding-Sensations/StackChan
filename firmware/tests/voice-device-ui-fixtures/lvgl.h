#pragma once
#include "companion_ui_fixture.hpp"
constexpr int LV_EVENT_PRESS_LOST=4,LV_EVENT_RELEASED=5,LV_INDEV_TYPE_POINTER=1,LV_INDEV_STATE_PRESSED=1,LV_INDEV_STATE_RELEASED=0,LV_LABEL_LONG_WRAP=3,LV_LABEL_LONG_SCROLL_CIRCULAR=4;
struct lv_point_t{int x=0,y=0;};struct lv_area_t{int x1=0,y1=0,x2=0,y2=0;};
struct lv_indev_t{lv_point_t point;int type=LV_INDEV_TYPE_POINTER,state=LV_INDEV_STATE_RELEASED;lv_obj_t* active=nullptr;};
inline lv_indev_t* active_input=nullptr;inline lv_indev_t* event_input=nullptr;
inline lv_indev_t* lv_indev_active(){return active_input;}
inline lv_indev_t* lv_event_get_indev(lv_event_t*){return event_input;}
inline int lv_indev_get_type(lv_indev_t*p){return p->type;}inline int lv_indev_get_state(lv_indev_t*p){return p->state;}
inline lv_obj_t* lv_indev_get_active_obj(){return active_input?active_input->active:nullptr;}
inline lv_obj_t* lv_event_get_target_obj(lv_event_t*e){return e->target;}
inline bool lv_obj_is_visible(lv_obj_t*p){return Visible(p);}

inline void lv_indev_get_point(const lv_indev_t*p,lv_point_t*out){*out=p->point;}
inline void lv_obj_get_coords(lv_obj_t*p,lv_area_t*out){int x=0,y=0;for(auto*n=p;n;n=n->parent){x+=n->x;y+=n->y;}*out={x,y,x+p->w-1,y+p->h-1};}
inline uint32_t native_tick=0;inline uint32_t lv_tick_get(){return native_tick;}
