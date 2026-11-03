"""Exercise actual JointApp close/poll code and Mooncake teardown. No device I/O."""
from pathlib import Path
import subprocess,tempfile
repo=Path(__file__).resolve().parents[3]
main=repo/'firmware/main'
source=(main/'apps/stock_shell/feature_apps.cc').read_text()
def method(signature):
 start=source.index(signature);begin=source.index('{',start);depth=1;end=begin+1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end].replace('override','override')
close=method('void onClose()override')
usb=(main/'companion/direct_usb_pcm/usb_pcm_app.cc').read_text()
request=usb[usb.index('bool RequestClose()override'):usb.index(' void onRunning()override')].strip()

pending=method('void ShowPending(const char* text)')
helper=method('bool CloseUsbView()') if 'bool CloseUsbView()' in source else ''
running=method('void onRunning()override');running=running[:running.index('  auto& s=State();')]+ '\n }'
opened=method('void onOpen()override');opened=opened[:opened.index('  usb_pending_=false;')]+'  ++opens;\n }'
fixture=r'''
#include <mooncake.h>
#include "companion/direct_usb_pcm/usb_pcm_app.h"
#include "apps/stock_shell/shell_teardown.hpp"
#include "companion/public_status_owner.hpp"
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>
#include <functional>
struct lv_obj_t{};constexpr int LV_OBJ_FLAG_HIDDEN=1;
static int deleted=0,child_destroyed=0,child_running=0,stops=0,globals=0;
static bool usb_closed=false,regular_closed=true;static unsigned orphan_panels=0;
struct LvglLockGuard{};
namespace view {unsigned destroyed=0;void update_home_indicator(){}void destroy_home_indicator(){++destroyed;}}

void lv_obj_delete(lv_obj_t* p){if(p)++deleted;}
void lv_label_set_text(lv_obj_t*,const char*){}
namespace stock_ui{lv_obj_t* Card(lv_obj_t* parent,int,int,int,int){static lv_obj_t node;if(!parent)++orphan_panels;return &node;}lv_obj_t* Text(lv_obj_t*,const char*,int,int,int){static lv_obj_t node;return &node;}}
void lv_obj_clean(lv_obj_t*){}void lv_obj_add_flag(lv_obj_t*,int){}void lv_obj_remove_flag(lv_obj_t*,int){}
namespace companion{void OpenView(lv_obj_t*){}}
struct ActorView{bool closed;};
ActorView ReadView(){return {usb_closed};}void StopFromPage(){++stops;}
struct FakeUsb:direct_usb_pcm::Page{
 bool closing_=false;
 std::function<void()> returned;
 explicit FakeUsb(std::function<void()> callback={}):returned(std::move(callback)){}
 ~FakeUsb(){++child_destroyed;}

''' + request + r'''

 void onClose()override{++stops;}
 void onRunning()override{++child_running;}
};
namespace direct_usb_pcm{std::unique_ptr<Page> MakeApp(std::function<void()> callback){return std::make_unique<FakeUsb>(std::move(callback));}}
class JointApp:public mooncake::AppAbility{
public:
 std::unique_ptr<direct_usb_pcm::Page> usb_view_=std::make_unique<FakeUsb>();
 bool usb_pending_=false,usb_active_=true,usb_return_=false,closing_=false;
 lv_obj_t node,*frame_=&node,*root_=&node,*native_root_=&node,*native_controls_=&node,*native_actions_=&node,*status_=&node,*picker_=&node,*pending_panel_=nullptr,*pending_text_=nullptr;
 std::vector<int> callbacks_,native_callbacks_;unsigned opens=0,pending=0;
 JointApp(){companion::PublicStatusOwner().Set(this,[](void*){return false;});}
 ~JointApp(){companion::PublicStatusOwner().Clear(this);}
 bool CloseAll(){return regular_closed;}
 void ClearPending(){}void RenderNativeControls(){}void ShowControls(bool){}
'''
checks=r'''
int main(){
 {JointApp parent;parent.open();parent.baseUpdate();assert(parent.currentState()==mooncake::AppAbility::StateRunning);
  parent.close();parent.baseUpdate();
  assert(parent.usb_view_&&parent.frame_&&child_destroyed==0&&deleted==0);
  assert(parent.currentState()!=mooncake::AppAbility::StateSleeping&&!companion::PublicStatusOwner().Idle());
  for(unsigned tick=0;tick<10;++tick){parent.baseUpdate();assert(parent.frame_&&parent.usb_view_);}
  assert(!view::destroyed);assert(stops&&child_running&&parent.opens==1&&static_cast<FakeUsb*>(parent.usb_view_.get())->closing_); // re-open does not reset or rebuild the parent
  usb_closed=true;
  for(unsigned tick=0;tick<5;++tick)parent.baseUpdate();
  assert(parent.currentState()==mooncake::AppAbility::StateSleeping&&!parent.usb_view_&&!parent.frame_);
  assert(child_destroyed==1&&deleted==1&&companion::PublicStatusOwner().Idle()&&view::destroyed==1);
 }
 // A forced whole-shell teardown must not uninstall an asynchronous close owner.
 usb_closed=false;deleted=child_destroyed=0;
 {mooncake::Mooncake shell;auto parent=std::make_unique<JointApp>();int id=shell.installApp(std::move(parent));shell.openApp(id);shell.update();
  bool destroyed=false;
  bool finished=stock_shell::TearDownShell<LvglLockGuard>(shell,[]{++globals;},[&]{destroyed=true;});
  assert(!finished&&!destroyed&&!globals&&!child_destroyed&&!deleted);
  for(unsigned tick=0;tick<8;++tick){assert(!stock_shell::TearDownShell<LvglLockGuard>(shell,[]{++globals;},[&]{destroyed=true;}));assert(!destroyed&&!deleted&&!child_destroyed);}
  usb_closed=true;regular_closed=false;
  assert(!stock_shell::TearDownShell<LvglLockGuard>(shell,[]{++globals;},[&]{destroyed=true;}));assert(!destroyed&&!deleted);
  regular_closed=true;
  assert(stock_shell::TearDownShell<LvglLockGuard>(shell,[]{++globals;},[&]{destroyed=true;}));
  assert(destroyed&&globals==1&&child_destroyed==1&&deleted==1);
 }
 // A canceled pending-open must not create an orphan panel or start Mira.
 usb_closed=true;regular_closed=false;
 {JointApp parent;parent.usb_view_.reset();parent.frame_=parent.root_=parent.native_root_=parent.native_controls_=parent.native_actions_=parent.status_=parent.picker_=nullptr;
  const int before=stops;
  parent.open();parent.close();parent.baseUpdate();
  assert(!orphan_panels&&parent.opens==0);
  for(unsigned tick=0;tick<10;++tick){parent.baseUpdate();assert(parent.currentState()!=mooncake::AppAbility::StateSleeping&&parent.opens==0&&!orphan_panels&&stops==before);}
  regular_closed=true;
  for(unsigned tick=0;tick<5;++tick)parent.baseUpdate();
  assert(parent.currentState()==mooncake::AppAbility::StateSleeping);
  parent.open();parent.baseUpdate();assert(parent.opens==1);
 }
 puts("nested USB parent close and forced shell destruction retain owners until checked close");
}
'''
mooncake=repo/'firmware/components/mooncake/src'
if not (mooncake/'mooncake.h').exists():
 raise SystemExit('Missing pinned Mooncake dependency; run firmware/fetch_repos.py before the host suite')
with tempfile.TemporaryDirectory(prefix='stackchan-nested-usb-close-') as tmp:
 tmp=Path(tmp);(tmp/'test.cpp').write_text(fixture+pending+helper+opened+running+close+'\n};\n'+checks)
 subprocess.run(['clang++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-Wno-sign-compare','-I'+str(main),'-I'+str(mooncake),str(tmp/'test.cpp'),*[str(p) for p in mooncake.rglob('*.cpp')],'-o',str(tmp/'test')],check=True)
 subprocess.run([str(tmp/'test')],check=True)
