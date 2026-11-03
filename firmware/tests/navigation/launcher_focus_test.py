"""Execute the actual production View restoration block with inert UI boundaries."""
from pathlib import Path
import subprocess,sys,tempfile

root=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(__file__).resolve().parents[3]
source=(root/'firmware/main/apps/app_launcher/view/view.cpp').read_text()
start=source.index('    bool need_restore')
end=source.index('    // If first create',start)
block=source[start:end]
fixture=r'''
#include <algorithm>
#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <cstdio>
#if __has_include("apps/app_launcher/focus_policy.hpp")
#include "apps/app_launcher/focus_policy.hpp"
#endif
namespace mclog{template<class...T>void tagInfo(T...) {}}
namespace uitk{template<class T>T clamp(T x,T a,T b){return std::max(a,std::min(x,b));}}
constexpr int _center_copy_index=2,_icon_gap=320,LV_ANIM_OFF=0;
std::string _tag="fixture";
struct HAL {int warm=-1,cleared=0;int getWarmRebootTarget(){return warm;}void clearWarmRebootRequest(){warm=-1;++cleared;}} hal;
HAL& GetHAL(){return hal;}
struct Props {int appID;};
struct Panel{int x=0;void scrollBy(int value,int,int){x-=value;}};
struct Jump{int slot=-1;void jumpTo(int value){slot=value;}};
struct Result{int x;int slot;int clears;};
Result real_view_restore(const std::vector<int>& ids,std::optional<int> preferredAppID,int old_pixel,int warm=-1){
 std::vector<Props> appPorps;for(auto id:ids)appPorps.push_back({id});
 int _last_clicked_icon_pos_x=old_pixel;
 auto _panel=std::make_unique<Panel>();
 auto _dynamic_bg_color=std::make_unique<Jump>();auto _page_indicator=std::make_unique<Jump>();auto _dynamic_icon_label=std::make_unique<Jump>();
 int _state=0;constexpr int STATE_NORMAL=1;hal.warm=warm;hal.cleared=0;
'''
checks=r'''
 assert(_dynamic_bg_color->slot==_page_indicator->slot&&_page_indicator->slot==_dynamic_icon_label->slot);
 return {_panel->x,_page_indicator->slot,hal.cleared};
}
int main(){
 std::vector<int> root{-1000,-1001,-1002,-1003,-1004,-1005,-1006};
 // Home always means root Companions, regardless of prior menu size/copy/index.
 unsigned checks=0;
 for(int count:{1,2,6,7,12})for(int copy=0;copy<5;++copy)for(int index=0;index<count;++index){
  auto home=real_view_restore(root,-1000,(copy*count+index)*320);assert(home.x==2*7*320&&home.slot==14);++checks;
 }
 // Category entry starts its first child, not the root's previous pixel offset.
 for(int count:{1,2,6,7,12}){
  std::vector<int> children;for(int index=0;index<count;++index)children.push_back(index+1);
  auto child=real_view_restore(children,1,(2*7+6)*320);assert(child.x==2*count*320);++checks;
 }
 // Back keeps semantic parent category identity rather than a Back-tile offset.
 for(int index=0;index<7;++index){auto back=real_view_restore(root,root[index],20*320);assert(back.x==(14+index)*320);++checks;}
 auto stale_warm=real_view_restore(root,-1000,20*320,999);assert(stale_warm.x==14*320&&stale_warm.clears==1);++checks;
 auto legacy_warm=real_view_restore(root,std::nullopt,-1,3);assert(legacy_warm.x==17*320&&legacy_warm.clears==1);++checks;
 // Fresh shell and unknown preferred identity fall back deterministically.
 auto fresh=real_view_restore(root,-1000,-1);assert(fresh.x==14*320);++checks;
 auto unknown=real_view_restore(root,99999,20*320);assert(unknown.x==14*320);++checks;
 #if __has_include("apps/app_launcher/focus_policy.hpp")
 auto empty=launcher_focus::Select({},-1000,999);assert(empty.index==0&&!empty.immediate);++checks;
 auto cold=launcher_focus::Select(root,std::nullopt,-1);assert(cold.index==0&&!cold.immediate);++checks;
 auto absent=launcher_focus::Select({-1006},-1000,999);assert(absent.index==0&&absent.immediate);++checks;
 #endif
 std::printf("%u actual View focus/history checks passed\n",checks);
}
'''
with tempfile.TemporaryDirectory(prefix='stackchan-home-focus-') as temp:
    path=Path(temp);(path/'test.cpp').write_text(fixture+'\n'+block+'\n'+checks)
    subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(root/'firmware/main'),str(path/'test.cpp'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
