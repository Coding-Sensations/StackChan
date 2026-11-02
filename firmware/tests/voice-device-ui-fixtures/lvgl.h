#pragma once
#include "companion_ui_fixture.hpp"
constexpr int LV_EVENT_PRESS_LOST=4,LV_INDEV_TYPE_POINTER=1,LV_INDEV_STATE_PRESSED=1,LV_INDEV_STATE_RELEASED=0,LV_LABEL_LONG_WRAP=3;
struct lv_indev_t{int type=LV_INDEV_TYPE_POINTER,state=LV_INDEV_STATE_RELEASED;lv_obj_t* active=nullptr;};
inline lv_indev_t* active_input=nullptr;inline lv_indev_t* event_input=nullptr;
inline lv_indev_t* lv_indev_active(){return active_input;}
inline lv_indev_t* lv_event_get_indev(lv_event_t*){return event_input;}
inline int lv_indev_get_type(lv_indev_t*p){return p->type;}inline int lv_indev_get_state(lv_indev_t*p){return p->state;}
inline lv_obj_t* lv_indev_get_active_obj(){return active_input?active_input->active:nullptr;}
inline lv_obj_t* lv_event_get_target_obj(lv_event_t*e){return e->target;}
inline bool lv_obj_is_visible(lv_obj_t*p){return Visible(p);}
