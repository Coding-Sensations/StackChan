#include "pet_app.h"
#include "pet_scene.hpp"
#include <apps/local_tools/local_tools_model.hpp>
#include <apps/stock_shell/state_storage_binding.hpp>
#include <apps/stock_shell/ir_storage_binding.hpp>
#include <apps/common/tool_visual.hpp>
#include <apps/common/common.h>
#include <assets/assets.h>
#include <esp_timer.h>
#include <array>
#include <atomic>
#include <cstdio>
namespace pet_game {
class PetApp final:public mooncake::AppAbility {
 local_tools_ui::Model model_;
 stock_tools::PersistentState<stock_tools::SavedPet> store_{*stock_tools::PetFiles()};uint8_t saved_phase_=0;
 lv_obj_t *root_=nullptr,*room_=nullptr,*creature_=nullptr,*cue_=nullptr,*info_=nullptr,*bowl_=nullptr,*spark_=nullptr,*sleep_=nullptr;
 std::array<lv_obj_t*,3> levels_{};std::array<lv_obj_t*,2> eyes_{};std::array<lv_obj_t*,3> mouth_{};
 std::atomic<int> action_{0};bool details_=false;Care care_=Care::None;uint64_t care_started_=0,render_at_=0;
 struct Callback {PetApp* app;int id;};std::array<Callback,7> callbacks_{};unsigned callbacks_used_=0;
 uint64_t Now()const{return uint64_t(esp_timer_get_time()/1000);}
 void Load(uint64_t now){if(store_.Load())model_.RestorePet(store_.Library().pet,now);else model_.Tick(now);saved_phase_=model_.pet.decay_phase_minutes;}
 bool Save(){stock_tools::SavedPet saved;saved.pet=model_.pet;bool ok=store_.Save(saved);if(ok)saved_phase_=model_.pet.decay_phase_minutes;return ok;}
 void Bind(lv_obj_t* obj,int id){auto& c=callbacks_[callbacks_used_++];c={this,id};lv_obj_add_event_cb(obj,[](lv_event_t* e){auto c=static_cast<Callback*>(lv_event_get_user_data(e));c->app->action_=c->id;},LV_EVENT_CLICKED,&c);}
 lv_obj_t* Button(const char* label,int x,int y,int width,int id,int height=40){auto b=stock_ui::Button(root_,label,x,y,width,height);Bind(b,id);return b;}
 lv_obj_t* Pixel(lv_obj_t* parent,int x,int y,int w,int h,unsigned color){auto o=lv_obj_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_pad_all(o,0,0);lv_obj_set_style_border_width(o,0,0);lv_obj_set_style_radius(o,0,0);lv_obj_set_style_bg_color(o,lv_color_hex(color),0);lv_obj_remove_flag(o,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE));return o;}
 void Page(){LvglLockGuard lock;lv_obj_clean(root_);callbacks_used_=0;levels_.fill(nullptr);eyes_.fill(nullptr);mouth_.fill(nullptr);room_=creature_=cue_=info_=bowl_=spark_=sleep_=nullptr;stock_ui::Page(root_);stock_ui::Text(root_,"POCKET PAL",8,10,164);Button(details_?"Play":"Info",174,2,68,98,36);Button("Back",246,2,66,99,36);
  if(details_){auto body=stock_ui::Card(root_,8,48,304,136);lv_obj_add_flag(body,LV_OBJ_FLAG_SCROLLABLE);info_=stock_ui::Text(body,"",8,4,272);lv_label_set_long_mode(info_,LV_LABEL_LONG_WRAP);Button("Save",8,192,148,14);Button("Reload SD",164,192,148,15);return;}
  const char* names[]={"Food","Joy","Energy"};for(unsigned i=0;i<3;++i){stock_ui::Text(root_,names[i],8+104*i,36,96);auto b=lv_bar_create(root_);lv_obj_set_pos(b,8+104*i,56);lv_obj_set_size(b,96,8);lv_bar_set_range(b,0,100);lv_obj_set_style_bg_color(b,lv_color_hex(stock_ui::Panel),0);lv_obj_set_style_bg_color(b,lv_color_hex(stock_ui::Accent),LV_PART_INDICATOR);levels_[i]=b;}
  room_=stock_ui::Card(root_,8,72,304,114);Bind(room_,12);cue_=stock_ui::Text(room_,"",8,2,288,true);
  Pixel(room_,16,102,272,2,0x31515a);Pixel(room_,122,100,68,6,0x254351);
  creature_=lv_obj_create(room_);lv_obj_set_size(creature_,72,72);lv_obj_set_style_bg_opa(creature_,LV_OPA_TRANSP,0);lv_obj_set_style_pad_all(creature_,0,0);lv_obj_set_style_border_width(creature_,0,0);lv_obj_remove_flag(creature_,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE));
  for(unsigned y=0;y<12;++y)for(unsigned x=0;x<12;++x)if(Sprite[y][x]=='A')Pixel(creature_,x*6,y*6,6,6,y<3?0xa3efe0:stock_ui::Accent);
  eyes_[0]=Pixel(creature_,18,26,8,8,stock_ui::Canvas);eyes_[1]=Pixel(creature_,46,26,8,8,stock_ui::Canvas);for(unsigned i=0;i<3;++i)mouth_[i]=Pixel(creature_,30+4*i,46,4,4,stock_ui::Canvas);
  bowl_=Pixel(room_,84,82,24,10,stock_ui::Warning);Pixel(bowl_,2,0,20,3,0xd7f7ed);spark_=stock_ui::Text(room_,"+  +",210,36,60);sleep_=stock_ui::Text(room_,"z Z",208,28,70,true);
  Button("Feed",8,192,96,11);Button("Play",112,192,96,12);Button("Rest",216,192,96,13);
 }
 void Render(uint64_t now){if(!root_)return;LvglLockGuard lock;const auto& pet=model_.pet;
  if(details_){char text[384];std::snprintf(text,sizeof(text),"Hunger %u / Happiness %u\nEnergy %u\n%s\n%s\nAging runs while powered on.\nNo offline calendar clock.",unsigned(pet.hunger),unsigned(pet.happiness),unsigned(pet.energy),stock_tools::IrStoreMessage(store_.Status()),model_.clock_error?"Clock/state error; aging paused":"Care uses the saved pet model");lv_label_set_text(info_,text);return;}
  lv_bar_set_value(levels_[0],100-std::min(100,int(pet.hunger)),LV_ANIM_OFF);lv_bar_set_value(levels_[1],std::min(100,int(pet.happiness)),LV_ANIM_OFF);lv_bar_set_value(levels_[2],std::min(100,int(pet.energy)),LV_ANIM_OFF);
  const auto f=Animate(pet,care_,care_started_,now);lv_obj_set_pos(creature_,f.x,f.y);for(auto eye:eyes_)lv_obj_set_size(eye,8,f.blink?3:8);
  for(unsigned i=0;i<3;++i){const int y=f.mood==Mood::Lonely?(i==1?44:48):(f.mood==Mood::Happy||f.spark?(i==1?50:46):46);lv_obj_set_pos(mouth_[i],30+int(i)*4,y);}
  auto show=[](lv_obj_t* o,bool visible){if(visible)lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);};show(bowl_,f.bowl);show(spark_,f.spark);show(sleep_,f.sleep);lv_label_set_text(cue_,model_.clock_error?"Aging paused: clock/state error":f.bowl?"Nom nom!":f.spark?"Let's play!":f.sleep?(care_==Care::Rest&&now>=care_started_&&now-care_started_<1600?"Resting...":"A nap would help"):Cue(f.mood));
 }
public:
 PetApp(){setAppInfo().name="PET";static auto icon=assets::get_image("icon_dance.bin");setAppInfo().icon=&icon;}
 void onOpen()override{{LvglLockGuard lock;root_=lv_obj_create(lv_screen_active());}action_=0;details_=false;care_=Care::None;Load(Now());Page();Render(Now());}
 void onRunning()override{const auto now=Now();model_.Tick(now);const auto a=action_.exchange(0);if(a==99){Save();close();return;}if(a==98){details_=!details_;Page();}
  if(a==11)model_.pet.Feed();else if(a==12)model_.pet.Play();else if(a==13)model_.pet.Rest();else if(a==15)Load(now);
  if(a>=11&&a<=13){care_=a==11?Care::Feed:a==12?Care::Play:Care::Rest;care_started_=now;Save();}else if(a==14)Save();
  if(!model_.clock_error&&model_.pet.decay_phase_minutes!=saved_phase_){Save();saved_phase_=model_.pet.decay_phase_minutes;}
  if(a||now>=render_at_){Render(now);render_at_=now+160;}
 }
 void onClose()override{Save();LvglLockGuard lock;if(root_)lv_obj_delete(root_);root_=nullptr;room_=creature_=cue_=info_=bowl_=spark_=sleep_=nullptr;action_=0;callbacks_used_=0;}
};
}
namespace local_tools_ui {std::unique_ptr<mooncake::AppAbility> MakePetApp(){return std::make_unique<pet_game::PetApp>();}}
