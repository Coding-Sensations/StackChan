#include "screensaver_setup.hpp"
#include <apps/app_launcher/view/screensaver_styles.hpp>
#include <apps/app_launcher/view/screensaver_library.hpp>
#include <settings.h>
#include <nvs.h>
#ifdef STACKCHAN_COMPANION_BOOT
#include <companion/settings_policy.hpp>
#endif
#include <esp_heap_caps.h>
#include <algorithm>
#include <array>
#include <cstring>
namespace setup_workers {
namespace {
class ScreensaverWorker final:public WorkerBase {
    view::saver::Library library_;
    view::saver::Selection marked_;
    bool managing_=false,show_hidden_=false;
    lv_obj_t *confirmation_=nullptr,*secondary_=nullptr,*third_=nullptr,*mode_=nullptr;
    lv_obj_t *screen_=nullptr,*root_=nullptr,*title_=nullptr,*canvas_=nullptr,*toggle_=nullptr,*order_=nullptr,*count_=nullptr;
    uint16_t *pixels_=nullptr,*thumbnail_=nullptr;
    bool preview_failed_=false;
    unsigned id_=0;uint32_t started_=0,last_=0;int action_=0;
    struct Callback {ScreensaverWorker* self;int action;};
    std::array<Callback,14> callbacks_{};
    lv_obj_t* button(const char* text,int x,int y,int w,int action,lv_obj_t* parent=nullptr){
        auto* b=lv_button_create(parent?parent:root_);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,28);
        auto* label=lv_label_create(b);lv_label_set_text(label,text);lv_obj_center(label);
        auto& cb=callbacks_[action];cb={this,action};
        lv_obj_add_event_cb(b,[](lv_event_t* e){auto* c=static_cast<Callback*>(lv_event_get_user_data(e));c->self->action_=c->action;},LV_EVENT_CLICKED,&cb);
        return label;
    }
    bool currentInView() const {return library_.inView(id_,managing_&&show_hidden_);}
    void message(const char* text){lv_label_set_text(count_,text);lv_obj_remove_flag(count_,LV_OBJ_FLAG_HIDDEN);}
    void labels(){
        const auto style=static_cast<view::saver::Style>(id_);
        if(!currentInView())lv_label_set_text(title_,"No hidden screensavers");
        else if(managing_)lv_label_set_text_fmt(title_,"%u marked | %s",marked_.count(),view::saver::Name(style));
        else lv_label_set_text_fmt(title_,"%u selected | %s",library_.effective().count(),view::saver::Name(style));
        lv_label_set_text(toggle_,managing_?(marked_.selected(id_)?"Unmark":"Mark"):(library_.selection.selected(id_)?"Deselect":"Select"));
        lv_label_set_text(secondary_,managing_?(show_hidden_?"Restore marked":"Hide marked"):"Only this");
        lv_label_set_text(third_,managing_?(show_hidden_?"Visible":"Hidden"):"Select all");
        lv_label_set_text(order_,managing_?"Back":(library_.selection.order==view::saver::Order::Random?"Random":"Sequential"));
        lv_label_set_text(mode_,managing_?"Clear marks":"Manage");
        lv_obj_add_flag(count_,LV_OBJ_FLAG_HIDDEN);
        if(!currentInView()){if(canvas_)lv_obj_add_flag(canvas_,LV_OBJ_FLAG_HIDDEN);message("Nothing hidden");}
        else if(preview_failed_||!pixels_||!thumbnail_)message("Preview unavailable");
    }
    void confirm(){
        if(!marked_.count())return;
        // Preview is paused while this modal is open. The list scrolls on the
        // small display; all names are included before explicit confirmation.
        confirmation_=lv_obj_create(root_);lv_obj_set_pos(confirmation_,0,0);lv_obj_set_size(confirmation_,320,192);
        lv_obj_set_style_pad_all(confirmation_,0,0);lv_obj_remove_flag(confirmation_,LV_OBJ_FLAG_SCROLLABLE);
        auto* list=lv_obj_create(confirmation_);lv_obj_set_pos(list,4,4);lv_obj_set_size(list,312,142);
        std::string text=show_hidden_?"Restore marked?\n":"Hide marked? Frees no storage.\n";
        text+="Staged until Save. Cancel keeps choices.\n";
        for(unsigned id=1;id<view::saver::SlideCount;++id)if(marked_.selected(id)){
            text+=view::saver::Name(static_cast<view::saver::Style>(id));text+='\n';
        }
        auto* names=lv_label_create(list);lv_obj_set_width(names,280);lv_label_set_text(names,text.c_str());
        button("Cancel",8,153,145,12,confirmation_);button("Confirm",166,153,145,13,confirmation_);
    }
    void restart(){preview_failed_=false;if(canvas_)lv_obj_remove_flag(canvas_,LV_OBJ_FLAG_HIDDEN);started_=GetHAL().millis();if(id_)effect_catalog::Reset(uint16_t(id_-1));labels();}
    bool save(){
        // Keep the saved playlist independent of the visibility mask.
        library_.selection.normalize();
        // A single bounded versioned key stores both set and order. Checked
        // NVS calls avoid the legacy Settings setter's abort-on-error behavior.
#ifdef STACKCHAN_COMPANION_BOOT
        if(!companion::SettingsWriteAllowed("screensaver","playlist"))return false;
#endif
        const auto encoded=library_.payload();nvs_handle_t handle=0;
        if(nvs_open("screensaver",NVS_READWRITE,&handle)!=ESP_OK)return false;
        auto error=nvs_set_str(handle,"playlist",encoded.c_str());
        if(error==ESP_OK)error=nvs_commit(handle);
        nvs_close(handle);if(error!=ESP_OK)return false;
        if(nvs_open("screensaver",NVS_READONLY,&handle)!=ESP_OK)return false;
        std::array<char,192> check{};size_t length=check.size();
        error=nvs_get_str(handle,"playlist",check.data(),&length);nvs_close(handle);
        return error==ESP_OK&&length==encoded.size()+1&&encoded==check.data();
    }
    void preview(uint32_t elapsed){
        if(confirmation_||!currentInView()||preview_failed_||!pixels_||!thumbnail_||!canvas_)return;
        // Complete the frame before scaling it. The old wall-clock cutoff could
        // expire during clears and permanently hide every otherwise valid preview.
        if(id_){if(!effect_catalog::Render(uint16_t(id_-1),elapsed,pixels_,320,240)){preview_failed_=true;lv_obj_add_flag(canvas_,LV_OBJ_FLAG_HIDDEN);message("Preview unavailable");return;}}
        else {
            // Local preview of the stock DVD slide, without loading a screen.
            std::fill_n(pixels_,320*240,uint16_t(0));
            unsigned x=(elapsed/10)%512,y=(elapsed/13)%384;if(x>256)x=512-x;if(y>192)y=384-y;
            for(unsigned row=0;row<48;++row)for(unsigned col=0;col<64;++col){
                bool eye=row>=17&&row<23&&((col>=14&&col<20)||(col>=44&&col<50));
                bool mouth=row>=29&&row<31&&col>=23&&col<41;
                pixels_[(y+row)*320+x+col]=(eye||mouth)?0:0xffff;
            }
        }
        for(unsigned y=0;y<120;++y)for(unsigned x=0;x<160;++x)thumbnail_[y*160+x]=pixels_[(y*2)*320+x*2];
        lv_obj_invalidate(canvas_);
    }
public:
    ScreensaverWorker(){
        Settings settings("screensaver");library_=view::saver::Library::fromPayload(settings.GetString("playlist"));
        id_=library_.effective().first();screen_=lv_screen_active();root_=lv_obj_create(screen_);
        lv_obj_set_pos(root_,0,24);lv_obj_set_size(root_,320,192);lv_obj_set_style_pad_all(root_,0,0);lv_obj_set_style_border_width(root_,0,0);lv_obj_set_style_radius(root_,0,0);
        lv_obj_set_style_bg_color(root_,lv_color_hex(0xEDF4FF),0);lv_obj_set_style_bg_opa(root_,LV_OPA_COVER,0);lv_obj_remove_flag(root_,LV_OBJ_FLAG_SCROLLABLE);
        title_=lv_label_create(root_);lv_obj_set_pos(title_,8,3);lv_obj_set_width(title_,304);lv_label_set_long_mode(title_,LV_LABEL_LONG_DOT);
        pixels_=static_cast<uint16_t*>(heap_caps_malloc(320*240*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
        thumbnail_=static_cast<uint16_t*>(heap_caps_malloc(160*120*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
        if(pixels_&&thumbnail_){std::memset(pixels_,0,320*240*2);std::memset(thumbnail_,0,160*120*2);canvas_=lv_canvas_create(root_);lv_canvas_set_buffer(canvas_,thumbnail_,160,120,LV_COLOR_FORMAT_RGB565);lv_obj_set_pos(canvas_,8,27);lv_obj_remove_flag(canvas_,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE));}
        toggle_=button("Select",178,27,134,1);secondary_=button("Only this",178,59,134,2);third_=button("Select all",178,91,134,3);order_=button("Sequential",178,123,134,4);
        button("<",8,157,45,5);button(">",58,157,45,6);mode_=button("Manage",108,157,118,8);button("Save",234,157,78,7);
        count_=lv_label_create(root_);lv_obj_set_pos(count_,8,27);lv_obj_set_size(count_,160,48);lv_obj_set_style_bg_color(count_,lv_color_hex(0xEDF4FF),0);lv_obj_set_style_bg_opa(count_,LV_OPA_COVER,0);lv_obj_set_style_text_font(count_,LV_FONT_DEFAULT,0);
        restart();preview(0);
        if(!pixels_||!thumbnail_)message("Preview unavailable");
    }
    ~ScreensaverWorker() override {
        // Delete canvas and every callback before freeing its borrowed pixels.
        if(root_&&lv_obj_is_valid(root_))lv_obj_delete(root_);
        if(pixels_)heap_caps_free(pixels_);
        if(thumbnail_)heap_caps_free(thumbnail_);
    }
    void update() override {
        if(!root_||lv_screen_active()!=screen_||!lv_obj_is_valid(root_)||!lv_obj_is_visible(root_))return;
        const int action=action_;action_=0;
        if(confirmation_){
            if(action!=12&&action!=13)return;
            if(action==13){
                for(unsigned id=1;id<view::saver::SlideCount;++id)if(marked_.selected(id))library_.setHidden(id,!show_hidden_);
                marked_.bits.fill(0);
            }
            lv_obj_delete(confirmation_);confirmation_=nullptr;
            if(!currentInView())id_=library_.adjacent(id_,true,show_hidden_);
            restart();return;
        }
        if(action==1){
            if(currentInView()){
                if(managing_){if(id_)marked_.set(id_,!marked_.selected(id_));}
                else library_.selection.set(id_,!library_.selection.selected(id_));
            }
        }
        else if(action==2){
            if(managing_)confirm();
            else {library_.selection.bits.fill(0);library_.selection.set(id_,true);}
        }
        else if(action==3){
            if(managing_){show_hidden_=!show_hidden_;marked_.bits.fill(0);id_=library_.adjacent(view::saver::SlideCount-1,true,show_hidden_);restart();}
            else for(unsigned id=0;id<view::saver::SlideCount;++id)if(library_.visible(id))library_.selection.set(id,true);
        }
        else if(action==4){
            if(managing_){managing_=false;show_hidden_=false;marked_.bits.fill(0);if(!currentInView())id_=library_.effective().first();restart();}
            else library_.selection.order=library_.selection.order==view::saver::Order::Random?view::saver::Order::Sequential:view::saver::Order::Random;
        }
        else if(action==5||action==6){id_=library_.adjacent(id_,action==6,managing_&&show_hidden_);restart();}
        else if(action==7){if(save())_is_done=true;else message("Save not verified. Retry Save.");return;}
        else if(action==8){if(managing_)marked_.bits.fill(0);else {managing_=true;show_hidden_=false;}}
        if(action)labels();
        const uint32_t now=GetHAL().millis();if(uint32_t(now-last_)>=60){last_=now;preview(uint32_t(now-started_));}
    }
};
}
std::unique_ptr<WorkerBase> MakeScreensaverWorker(){return std::make_unique<ScreensaverWorker>();}
}
