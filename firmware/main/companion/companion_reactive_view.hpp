#pragma once
#include "desk_reactive_avatar.hpp"
#include "companion_character_layout.hpp"
#include "mira_character_image.hpp"
#include "provider_room_view.hpp"
#include <lvgl.h>
#include <cstring>
#include <cstdio>
namespace companion::desk {
// Passive projection of the exact existing Mira idle frame. It takes no native
// Avatar lock, audio owner, renderer task or capture permission. Calls stay under
// the existing HAL LvglLockGuard. The approved artwork is not redrawn/animated.
class CompanionReactiveView {
    lv_obj_t *root_=nullptr,*image_=nullptr,*privacy_=nullptr;
    CharacterLayout layout_{};room::View room_;room::Snapshot snapshot_;bool room_selected_=false;uint64_t now_=0;std::function<bool(const std::string&,const std::string&)> submit_;
    static bool Is(const char* a,const char* b){return std::strcmp(a,b)==0;}
public:
    void Detach(){room_.Detach();root_=image_=privacy_=nullptr;room_selected_=false;snapshot_={};submit_={};}
    void SelectRoom(bool yes){if(yes!=room_selected_)room_.Visible(false);room_selected_=yes;if(yes&&root_&&!room_.Root()){room_.Open(root_,companion_modes::Provider::Mira);room_.SetSubmit(submit_);}}
    void SetDecisionSink(std::function<bool(const std::string&,const std::string&)> sink){submit_=std::move(sink);room_.SetSubmit(submit_);}
    void SetProviderSnapshot(room::Snapshot snapshot,uint64_t now){snapshot_=std::move(snapshot);now_=now;}
    void Open(lv_obj_t* parent){
        room_selected_=false;
        layout_=CharacterScreen(lv_obj_get_width(parent),lv_obj_get_height(parent));
        if(!layout_.supported)return;
        root_=lv_obj_create(parent);lv_obj_remove_style_all(root_);lv_obj_set_pos(root_,0,0);lv_obj_set_size(root_,layout_.face.w,layout_.face.h);
        lv_obj_set_style_bg_color(root_,lv_color_hex(0x000000),0);lv_obj_set_style_bg_opa(root_,LV_OPA_COVER,0);lv_obj_remove_flag(root_,LV_OBJ_FLAG_SCROLLABLE);
        image_=lv_image_create(root_);lv_image_set_src(image_,MiraCharacterImage());lv_image_set_pivot(image_,160,120);
        const int sx=layout_.face.w*256/320,sy=layout_.face.h*256/240;lv_image_set_scale(image_,sx<sy?sx:sy);lv_obj_center(image_);
        privacy_=lv_label_create(parent);lv_obj_set_pos(privacy_,layout_.privacy.x,layout_.privacy.y);lv_obj_set_size(privacy_,layout_.privacy.w,layout_.privacy.h);
        lv_obj_set_style_text_font(privacy_,&lv_font_montserrat_14,0);lv_label_set_long_mode(privacy_,LV_LABEL_LONG_DOT);
        lv_obj_set_style_bg_color(privacy_,lv_color_hex(0x000000),0);lv_obj_set_style_bg_opa(privacy_,200,0);lv_obj_set_style_text_color(privacy_,lv_color_hex(0xffffff),0);
        lv_label_set_text(privacy_,"Audio unverified");
        // Resolve deferred geometry before the room reads its viewport.
        lv_obj_update_layout(root_);room_.Open(root_,companion_modes::Provider::Mira);room_.SetSubmit(submit_);
    }
    void Render(const ReactiveFrame& f,bool controls_or_prompt){
        if(!root_||!privacy_)return;
        const bool mic_on=Is(f.status.mic,"Listening: capture active");
        const bool wake_on=Is(f.status.mic,"Local wake capture active");
        const bool speaker_on=Is(f.status.output,"Playback active");
        const bool stopped=Is(f.status.stop,"Stop pending")||Is(f.status.stop,"Stop unconfirmed");
        const char* status=Is(f.status.stop,"Stop pending")?"Stopping audio":
            Is(f.status.stop,"Stop unconfirmed")?"Stop unconfirmed":
            (mic_on||wake_on)&&speaker_on?"Mic + speaker on":
            mic_on?"Mic on":wake_on?"Wake mic on":speaker_on?"Speaker on":
            Is(f.status.mic,"Mic off confirmed")&&Is(f.status.output,"Output off confirmed")?"Muted":"Audio unverified";
        lv_label_set_text(privacy_,status);
        if(stopped){lv_obj_set_pos(privacy_,8,layout_.privacy.y);lv_obj_set_size(privacy_,layout_.face.w-16,20);}
        else{lv_obj_set_pos(privacy_,layout_.privacy.x,layout_.privacy.y);lv_obj_set_size(privacy_,layout_.privacy.w,layout_.privacy.h);}
        // Decision presentation is transient; only SelectRoom changes user preference.
        const bool decision_current=snapshot_.verified&&snapshot_.scene=="decision_sign";
        auto shown=snapshot_;
        // These statuses come from the existing checked codec/lease mailbox,
        // never socket connectivity. Background local-wake capture stays room mode.
        const bool off_confirmed=Is(f.status.mic,"Mic off confirmed")&&Is(f.status.output,"Output off confirmed");
        shown.local_communication_current=!room_selected_&&!decision_current&&!stopped&&(mic_on||speaker_on||off_confirmed);
        shown.communication=!room_selected_&&!decision_current&&!stopped?(speaker_on?room::Communication::Speaking:mic_on?room::Communication::Listening:room::Communication::Idle):room::Communication::Idle;
        if(room_.Root()){room_.Visible(f.apply_face&&!controls_or_prompt&&!stopped);room_.Render(shown,now_);lv_obj_add_flag(image_,LV_OBJ_FLAG_HIDDEN);}
        else lv_obj_remove_flag(image_,LV_OBJ_FLAG_HIDDEN);
        if(!f.apply_face||controls_or_prompt)lv_obj_add_flag(root_,LV_OBJ_FLAG_HIDDEN);else lv_obj_remove_flag(root_,LV_OBJ_FLAG_HIDDEN);
    }
};
}
