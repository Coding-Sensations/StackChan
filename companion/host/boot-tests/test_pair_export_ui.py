"""Compile actual OpenView with LVGL doubles; no device/firmware execution."""
import pathlib,subprocess,tempfile
from test_tick_cleanup_stall import body
ROOT=pathlib.Path(__file__).resolve().parents[3]
def main():
    source=(ROOT/'firmware/main/companion/boot.cc').read_text()
    view=body(source,'void OpenView(lv_obj_t* parent)')
    fixture=r'''#include <atomic>
#include <cassert>
#include <string>
#include <vector>
#include <memory>
#include "companion/pair_ui_action.hpp"
struct lv_event_t{};
struct lv_obj_t{int x=0,y=0,w=320,h=240;std::string text;lv_obj_t* parent=nullptr;void(*callback)(lv_event_t*)=nullptr;bool button=false;};
std::vector<std::unique_ptr<lv_obj_t>> objects;
lv_obj_t* create(lv_obj_t* parent,bool button){auto p=std::make_unique<lv_obj_t>();p->parent=parent;p->button=button;auto value=p.get();objects.push_back(std::move(p));return value;}
lv_obj_t* lv_label_create(lv_obj_t* p){return create(p,false);}lv_obj_t* lv_button_create(lv_obj_t* p){return create(p,true);}
void lv_obj_set_style_bg_color(lv_obj_t*,int,int){}void lv_obj_set_style_text_color(lv_obj_t*,int,int){}
void lv_obj_set_width(lv_obj_t*,int){}void lv_obj_set_height(lv_obj_t*,int){}void lv_label_set_long_mode(lv_obj_t*,int){}
void lv_obj_align(lv_obj_t* p,int align,int x,int y){p->x=align==1?(320-p->w)/2+x:x;p->y=align==1?(240-p->h)/2+y:y;}void lv_obj_set_size(lv_obj_t* p,int w,int h){p->w=w;p->h=h;}void lv_obj_set_pos(lv_obj_t* p,int x,int y){p->x=x;p->y=y;}
void lv_obj_center(lv_obj_t*){}void lv_label_set_text(lv_obj_t* p,const char* text){p->text=text;}
void lv_obj_add_event_cb(lv_obj_t* p,void(*cb)(lv_event_t*),int,void*){p->callback=cb;}
int lv_color_hex(int n){return n;}constexpr int LV_LABEL_LONG_DOT=0,LV_ALIGN_TOP_LEFT=0,LV_ALIGN_CENTER=1,LV_EVENT_CLICKED=0;
struct LvglLockGuard{~LvglLockGuard(){}};
namespace companion{
std::atomic<bool> diagnostic_requested{true},pair_requested{true},connect_requested{true},listen_requested{true},mute_requested{true},confirm_requested{true},reject_requested{true};
struct Pairing{std::string code;std::string Code(){return code;}};
struct Runtime{Pairing numeric_pairing;PairUiAction pair_ui;lv_obj_t* status=nullptr;lv_obj_t* diagnostic_button=nullptr;lv_obj_t* pair_button=nullptr;const char* message=nullptr;};
Runtime& runtime(){static Runtime state;return state;}
ACTUAL_VIEW
}
int main(){using namespace companion;lv_obj_t root;OpenView(&root);unsigned buttons=0;
 for(auto& item:objects)if(item->button)++buttons;assert(buttons==2);
 bool pair=false,export_=false;for(auto& item:objects){pair|=item->text=="Pair";export_|=item->text=="Export";assert(item->text!="Connect"&&item->text!="Mute"&&item->text!="Allow this conversation");}assert(pair&&export_);assert(runtime().pair_button->y>=34+110);assert(runtime().pair_button->y+runtime().pair_button->h<=240);assert(runtime().pair_button->h>=44);
 assert(!pair_requested&&!connect_requested&&!listen_requested&&!confirm_requested&&!diagnostic_requested);
 runtime().pair_button->callback(nullptr);assert(runtime().pair_ui.Take("","",false)==PairUiAction::Action::Pair);assert(!connect_requested&&!listen_requested&&!confirm_requested);
 runtime().numeric_pairing.code="123456";runtime().pair_button->callback(nullptr);assert(runtime().pair_ui.Take("123456","",false)==PairUiAction::Action::Stale);runtime().pair_ui.Render("123456","",false);runtime().pair_button->callback(nullptr);assert(runtime().pair_ui.Take("123456","",false)==PairUiAction::Action::Confirm);assert(!pair_requested&&!connect_requested&&!listen_requested);
 runtime().diagnostic_button->callback(nullptr);assert(diagnostic_requested&&!pair_requested&&!connect_requested&&!listen_requested&&!confirm_requested);
}
'''.replace('ACTUAL_VIEW',view)
    with tempfile.TemporaryDirectory(prefix='stackchan-pair-export-ui-') as directory:
        p=pathlib.Path(directory);(p/'fixture.cpp').write_text(fixture)
        subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(ROOT/'firmware/main'),str(p/'fixture.cpp'),'-o',str(p/'fixture')],check=True)
        subprocess.run([str(p/'fixture')],check=True,timeout=5)
    print('PASS actual OpenView: Pair/Export only; Pair confirmation; no Connect/listen activation')
if __name__=='__main__':main()
