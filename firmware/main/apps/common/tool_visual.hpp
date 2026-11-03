#pragma once
#include <lvgl.h>
// Lightweight LVGL styling only. Caller owns callbacks, locks and object lifetime.
// Active indicators must be supplied from actual model/worker state.
namespace stock_ui {
inline constexpr unsigned Canvas=0x101820,Panel=0x182732,ButtonFill=0x254351,Ink=0xe6f0f4,Accent=0x58dfcc,Muted=0x90a7b4,Warning=0xffb95c;
inline void Page(lv_obj_t* page){lv_obj_set_size(page,320,240);lv_obj_set_style_pad_all(page,0,0);lv_obj_remove_flag(page,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_bg_color(page,lv_color_hex(Canvas),0);lv_obj_set_style_text_color(page,lv_color_hex(Ink),0);lv_obj_set_style_border_width(page,0,0);lv_obj_set_style_text_font(page,&lv_font_montserrat_16,0);}
inline void Active(lv_obj_t* obj,bool active){lv_obj_set_style_bg_color(obj,lv_color_hex(active?Accent:ButtonFill),0);lv_obj_set_style_text_color(obj,lv_color_hex(active?Canvas:Ink),0);}
inline lv_obj_t* Text(lv_obj_t* page,const char* text,int x,int y,int width,bool muted=false){auto label=lv_label_create(page);lv_label_set_text(label,text);lv_obj_set_pos(label,x,y);lv_obj_set_width(label,width);lv_obj_set_style_text_color(label,lv_color_hex(muted?Muted:Ink),0);return label;}
inline lv_obj_t* Button(lv_obj_t* page,const char* text,int x,int y,int width,int height=40,bool active=false){auto button=lv_button_create(page);lv_obj_set_pos(button,x,y);lv_obj_set_size(button,width,height);lv_obj_set_style_radius(button,6,0);lv_obj_set_style_pad_all(button,0,0);lv_obj_set_style_border_width(button,0,0);lv_obj_set_style_shadow_width(button,0,0);Active(button,active);auto label=lv_label_create(button);lv_label_set_text(label,text);lv_obj_center(label);return button;}
inline lv_obj_t* Card(lv_obj_t* page,int x,int y,int width,int height){auto card=lv_obj_create(page);lv_obj_set_pos(card,x,y);lv_obj_set_size(card,width,height);lv_obj_set_style_pad_all(card,0,0);lv_obj_set_style_bg_color(card,lv_color_hex(Panel),0);lv_obj_set_style_border_width(card,0,0);lv_obj_set_style_radius(card,6,0);lv_obj_remove_flag(card,LV_OBJ_FLAG_SCROLLABLE);return card;}
inline lv_obj_t* Status(lv_obj_t* page,const char* text,int y=220){auto label=Text(page,text,8,y,304,true);lv_label_set_long_mode(label,LV_LABEL_LONG_SCROLL_CIRCULAR);return label;}
}
