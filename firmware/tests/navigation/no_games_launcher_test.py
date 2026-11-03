"""Execute the real launcher methods with UI/hardware boundaries replaced by inert fixtures."""
from pathlib import Path
import subprocess, tempfile, sys
root=Path(__file__).resolve().parents[2]/'main'
source=Path(sys.argv[1]).read_text() if len(sys.argv)>1 else (root/'apps/app_launcher/app_launcher.cpp').read_text()
methods=[]
for name in ('onLauncherOpen','onLauncherRunning','onLauncherClose','create_launcher_view','handle_category_navigation'):
 start=source.index('void AppLauncher::'+name+'(');begin=source.index('{',start);depth=1;end=begin+1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 methods.append(source[start:end])
fixture=r'''
#include "apps/app_launcher/category_routes.hpp"
#include <cassert>
#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <cstdio>
struct LvglLockGuard {};
namespace mclog {template<class... T>void tagInfo(T...) {}}
struct lv_obj_t{};lv_obj_t screen;lv_obj_t* lv_screen_active(){return &screen;}
namespace mooncake {struct AppProps_t {struct Info{std::string name;}info;int appID;};}
struct View {std::vector<mooncake::AppProps_t> shown;std::function<void(int)> onAppClicked;int next=-1;std::optional<int> focus;
 void init(std::vector<mooncake::AppProps_t> p,std::optional<int> preferred=std::nullopt){shown=std::move(p);focus=preferred;}void update(){if(next!=-1){auto id=next;next=-1;onAppClicked(id);}}};
namespace view {using LauncherView=View;}
struct Startup{void update(){}bool isDone(){return true;}};
namespace setup_workers{using StartupWorker=Startup;}
struct StackChan{void update(){}};StackChan& GetStackChan(){static StackChan s;return s;}
struct AppLauncher {
 std::unique_ptr<View> _view;std::unique_ptr<int> _screensaver;std::unique_ptr<Startup> _startup_worker;
 std::optional<launcher_categories::Group> _category;lv_obj_t* _launcher_screen=nullptr;
 int _pending_navigation=-1;unsigned _screensaver_timecount=0;bool _startup_checked=false;
 int opened=-1;
 std::vector<mooncake::AppProps_t> installed={{{"Mira"},1},{{"Stackchan"},2},{{"USB CONTROLS"},3},{{"Settings"},4},{{"AVATAR"},5},{{"GAMES"},6},{{"Hermes"},7},{{"Muse"},8},{{"Claude"},9},{{"Codex"},10},{{"MUSIC & AUDIO"},11},{{"TOOLS"},12},{{"NETWORK TOOLS"},13}};
 auto getAppProps(){return installed;}auto getAppInfo(){return installed.front().info;}
 void openApp(int id){opened=id;}void screensaver_update(){}
 void onLauncherOpen();void onLauncherRunning();void onLauncherClose();void create_launcher_view(int preferredAppID=launcher_categories::Token(launcher_categories::Group::Companions));void handle_category_navigation();
};
'''
checks=r'''
int main(){AppLauncher l;for(int round=0;round<10;++round){
 l.onLauncherOpen();assert(!l._category);assert(l._view->shown.front().info.name=="Companions");assert(l._view->focus==launcher_categories::Token(launcher_categories::Group::Companions));
 l._view->next=launcher_categories::Token(launcher_categories::Group::Companions);l.onLauncherRunning();
 assert(l._category==launcher_categories::Group::Companions);assert(l._view->shown.size()==7);assert(l._view->focus==1);
 assert(l._view->shown[0].info.name=="Mira"&&l._view->shown[1].info.name=="Stackchan");
 l.onLauncherRunning();assert(l._category==launcher_categories::Group::Companions);
 const int routes[]={1,2,7,8,9,10};l._view->next=routes[round%6];l.onLauncherRunning();assert(l.opened==routes[round%6]);
 l.onLauncherClose();assert(!l._view);l.onLauncherOpen();assert(!l._category);
 assert(l._view->shown.front().info.name=="Companions");l.onLauncherClose();
 }
 for(int index=0;index<7;++index){
  const auto group=static_cast<launcher_categories::Group>(index);
  l.onLauncherOpen();l._view->next=launcher_categories::Token(group);l.onLauncherRunning();
  if(l._category){
   assert(l._view->focus==l._view->shown.front().appID);
   l._view->next=launcher_categories::Back;l.onLauncherRunning();
   assert(!l._category&&l._view->focus==launcher_categories::Token(group));
  }
  l.onLauncherClose();l.onLauncherOpen();
  assert(!l._category&&l._view->focus==launcher_categories::Token(launcher_categories::Group::Companions));
  l.onLauncherClose();
 }
 l.installed.push_back({{"PET"},14});
 l.installed.erase(std::remove_if(l.installed.begin(),l.installed.end(),[](auto& p){return p.info.name=="GAMES"||p.info.name=="PET";}),l.installed.end());
 for(int round=0;round<10;++round){
  l.onLauncherOpen();
  bool music=false,avatar=false,settings=false;
  for(const auto& p:l._view->shown){assert(p.appID!=launcher_categories::Token(launcher_categories::Group::Games));assert(p.info.name!="Games");music|=p.info.name=="Music & Audio";avatar|=p.info.name=="Avatar & Motion";settings|=p.info.name=="Settings";}
  assert(music&&avatar&&settings);
  l._view->next=launcher_categories::Token(launcher_categories::Group::Companions);l.onLauncherRunning();assert(l._view->shown.size()==7);
  l._view->next=1;l.onLauncherRunning();assert(l.opened==1);l.onLauncherClose();
 }
 std::puts("10 full + 10 no-games real launcher cycles and category Home/Back routes passed");}
'''
with tempfile.TemporaryDirectory(prefix='stackchan-launcher-test-') as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(fixture+'\n'+'\n'.join(methods)+'\n'+checks)
 subprocess.run(['clang++','-std=c++17','-DSTACKCHAN_COMPANION_BOOT=1','-fsanitize=address,undefined','-I'+str(root),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
