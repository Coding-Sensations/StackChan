#pragma once
#include "review.hpp"
#include "lvgl.h"
namespace muse_review {
// Owner serializes LVGL/model calls and invokes Poll on every foreground tick,
// plus Close before provider exit. No worker, timer, or retained event queue.
class Dialog {
 Model& model_;Source& source_;lv_obj_t* root_;lv_obj_t* title_;lv_obj_t* value_;lv_obj_t* status_;lv_obj_t* buttons_[4]{};lv_obj_t* labels_[4]{};
 struct Binding {Dialog* self;unsigned action;uint64_t pressed=0;unsigned page=0;};Binding binding_[4]{};
 static void Event(lv_event_t*e){auto*b=static_cast<Binding*>(lv_event_get_user_data(e));auto&u=*b->self;auto code=lv_event_get_code(e);if(code==LV_EVENT_PRESSED){b->pressed=u.model_.Revision();b->page=u.model_.Page();return;}if(code==LV_EVENT_PRESS_LOST){b->pressed=0;return;}if(code!=LV_EVENT_CLICKED)return;auto r=b->pressed;b->pressed=0;if(!r||r!=u.model_.Revision())return;
 if(b->action==3){u.Close();return;}if(!u.model_.Poll(u.source_.Now())){u.Render();return;}
 if(!u.model_.Reviewed()){if(b->action==0)u.model_.Acknowledge(r,b->page);}else if(b->action==2){u.model_.Finish(r);}else {auto v=u.model_.Receipt();bool mic=u.mic_,speaker=u.speaker_;if(b->action==0)mic=!mic;if(b->action==1)speaker=!speaker;if(!v.valid&&u.model_.OptIn(r,mic,speaker)){u.mic_=mic;u.speaker_=speaker;}}
 u.Render();}
 bool mic_=false,speaker_=false;
 static lv_obj_t*Label(lv_obj_t*p,int x,int y,int w,const char*s){auto*l=lv_label_create(p);lv_obj_set_pos(l,x,y);lv_obj_set_width(l,w);lv_label_set_text(l,s);return l;}
 void Render(){for(auto&b:binding_)b.pressed=0;for(unsigned i=0;i<3;++i)lv_obj_add_flag(buttons_[i],LV_OBJ_FLAG_HIDDEN);
 if(!model_.Active()){mic_=speaker_=false;lv_label_set_text(title_,"Voice review unavailable");lv_label_set_text(value_,"");lv_label_set_text(status_,"Microphone off / output off");return;}
 if(!model_.Reviewed()){char text[25],heading[64];unsigned f,p,n;model_.PageText(text,f,p,n);const char*names[]={"Device","Call","Session","Thread","Source generation","Source expiry"};std::snprintf(heading,sizeof heading,"%s (%u/%u)",names[f],p,n);lv_label_set_text(title_,heading);lv_label_set_text(value_,text);lv_label_set_text(status_,"Read this full page, then Next");lv_label_set_text(labels_[0],"Next");lv_obj_remove_flag(buttons_[0],LV_OBJ_FLAG_HIDDEN);return;}
 auto r=model_.Receipt();if(!model_.Active()){Render();return;}
 lv_label_set_text(title_,r.valid?"Local review saved":"All target fields reviewed");lv_label_set_text(value_,r.valid?"Native voice unavailable":"Choose audio directions");lv_label_set_text(status_,"Local review grants no authority");if(r.valid)return;
 for(unsigned i=0;i<3;++i)lv_obj_remove_flag(buttons_[i],LV_OBJ_FLAG_HIDDEN);lv_label_set_text(labels_[0],mic_?"Mic: on":"Mic: off");lv_label_set_text(labels_[1],speaker_?"Out: on":"Out: off");lv_label_set_text(labels_[2],"Save review");}
 public:
 Dialog(lv_obj_t*parent,Model&m,Source&s):model_(m),source_(s){root_=lv_obj_create(parent);lv_obj_set_pos(root_,0,0);lv_obj_set_size(root_,320,240);lv_obj_set_style_pad_all(root_,0,0);lv_obj_set_style_border_width(root_,0,0);lv_obj_set_style_radius(root_,0,0);lv_obj_remove_flag(root_,LV_OBJ_FLAG_SCROLLABLE);
 title_=Label(root_,10,8,300,"");value_=Label(root_,10,43,300,"");status_=Label(root_,10,86,300,"");for(unsigned i=0;i<4;++i){buttons_[i]=lv_button_create(root_);lv_obj_set_pos(buttons_[i],i==3?110:5+105*i,i==3?193:137);lv_obj_set_size(buttons_[i],i==3?100:100,38);labels_[i]=lv_label_create(buttons_[i]);lv_label_set_text(labels_[i],i==3?"Cancel":"");lv_obj_center(labels_[i]);binding_[i]={this,i,0,0};lv_obj_add_event_cb(buttons_[i],Event,LV_EVENT_ALL,&binding_[i]);}Render();}
 ~Dialog(){Close();lv_obj_delete(root_);}Dialog(const Dialog&)=delete;Dialog&operator=(const Dialog&)=delete;
 bool Open(){mic_=speaker_=false;bool ok=model_.Open(source_.Now());Render();return ok;}
 void Close(){model_.Cancel();Render();}void Poll(){if(model_.Active()&&!model_.Poll(source_.Now()))Render();}
 lv_obj_t*Value(){return value_;}lv_obj_t*Action(unsigned i){return i<4?buttons_[i]:nullptr;}
};
}
