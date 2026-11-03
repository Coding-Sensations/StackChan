#pragma once
#include <lvgl.h>
#include <string>
namespace companion {
// A sibling panel owns secondary actions. It is created after the primary
// widgets, so no primary hit target can cover it. Prompts always dismiss it.
class CompanionDetailsView {
    lv_obj_t* panel_=nullptr;
    bool visible_=false;
    std::string prompt_;
public:
    lv_obj_t* Open(lv_obj_t* parent){
        visible_=false;prompt_.clear();panel_=lv_obj_create(parent);
        lv_obj_set_pos(panel_,0,44);lv_obj_set_size(panel_,320,172);
        lv_obj_set_style_pad_all(panel_,0,0);lv_obj_set_style_border_width(panel_,0,0);
        lv_obj_set_style_bg_color(panel_,lv_color_hex(0x10232d),0);
        lv_obj_set_style_bg_opa(panel_,LV_OPA_COVER,0);
        lv_obj_remove_flag(panel_,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(panel_,LV_OBJ_FLAG_HIDDEN);return panel_;
    }
    void Hide(){visible_=false;if(panel_)lv_obj_add_flag(panel_,LV_OBJ_FLAG_HIDDEN);}
    void Toggle(){if(!panel_)return;if(visible_)Hide();else{visible_=true;lv_obj_remove_flag(panel_,LV_OBJ_FLAG_HIDDEN);}}
    void UpdatePrompt(const std::string& key){if(!key.empty()&&key!=prompt_)Hide();prompt_=key;}
    void Detach(){panel_=nullptr;visible_=false;}
};
}
