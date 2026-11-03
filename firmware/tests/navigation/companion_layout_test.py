"""Execute production wrapper rendering at an inert LVGL boundary.

Catches independent wrapper controls painting over native consent controls.
No firmware, transport, display or hardware is exercised.
"""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'firmware/main/apps/stock_shell/feature_apps.cc').read_text()
def body(signature):
 start=s.index(signature);end=s.index('{',start)+1;depth=1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
fixture=r'''
#include "companion_ui_fixture.hpp"
#include <cstdio>
inline void lv_obj_clean(lv_obj_t* p){for(auto& c:objects)if(c->parent==p&&!c->deleted)lv_obj_delete(c.get());}
struct Link{};struct Runtime{Link link;};Runtime& State(){static Runtime s;return s;}
bool staged=false,character_visible=false;namespace companion{bool CompanionCharacterVisible(){return character_visible;}unsigned FeatureAudioEpoch(Link&){return 1;}bool FeatureAudioStaged(Link&){return staged;}}
struct App{
 lv_obj_t *native_controls_=lv_obj_create(&screen),*native_actions_=lv_obj_create(&screen);
 lv_obj_t *native_root_=lv_obj_create(&screen),*root_=lv_obj_create(&screen);bool controls_visible_=false;
 unsigned rendered_epoch_=0;std::vector<int> native_callbacks_;
 void Button(lv_obj_t* p,const char* t,int x,int y,int w,int,std::vector<int>&){auto b=lv_button_create(p);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,40);auto l=lv_label_create(b);lv_label_set_text(l,t);}
 ACTUAL
};
bool overlap(lv_obj_t* a,lv_obj_t* b){return a->x<b->x+b->w&&a->x+a->w>b->x&&a->y<b->y+b->h&&a->y+a->h>b->y;}
int main(){for(bool stage:{false,true}){staged=stage;App app;lv_obj_set_size(app.native_root_,320,240);app.RenderNativeControls();
 // The home navigation must never contain transport/consent actions.
 assert(!Text("USB Mira") || Button("USB Mira")->parent==app.native_actions_);
 // Secondary page actions must leave 0..44 for navigation and stay on-screen.
 std::vector<lv_obj_t*> actions;
 for(auto& o:objects)if(!o->deleted&&o->parent==app.native_actions_&&o->w){
  assert(o->x>=0&&o->y>=44&&o->x+o->w<=320&&o->y+o->h<=240);actions.push_back(o.get());}
 for(auto a:actions)for(auto b:actions)if(a!=b)assert(!overlap(a,b));
 assert(Text("USB Mira"));if(stage){assert(Text("Connect")&&Text("Listen"));}
 character_visible=true;app.ShowControls(false);assert(!Visible(app.native_controls_)&&Visible(app.native_root_)&&!Visible(app.native_actions_)&&!Visible(app.root_));assert(!Text("USB Mira"));character_visible=false;app.ShowControls(false);assert(Visible(app.native_controls_));
 app.ShowRoutes();assert(!Visible(app.native_root_)&&Visible(app.native_actions_)&&!Visible(app.root_));assert(Text("USB Mira"));
 app.ShowControls(true);assert(!Visible(app.native_root_)&&!Visible(app.native_actions_)&&Visible(app.root_));
 app.ShowControls(false);assert(Visible(app.native_root_)&&!Text("USB Mira"));
 lv_obj_delete(app.native_controls_);lv_obj_delete(app.native_actions_);lv_obj_delete(app.native_root_);lv_obj_delete(app.root_);
 }puts("wrapper secondary-page bounds and staged action isolation passed");}
'''.replace('ACTUAL',body('void RenderNativeControls()')+'\n'+body('void ShowControls(bool visible)')+'\n'+body('void ShowRoutes()'))
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(Path(__file__).parent),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
