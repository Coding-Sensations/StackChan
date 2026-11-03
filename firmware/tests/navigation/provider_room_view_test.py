from pathlib import Path
import subprocess,tempfile,json,os
root=Path(__file__).resolve().parents[3];png=root/'firmware/main/assets/mira_idle_frame.png'
fixture=r'''
#include "companion_ui_fixture.hpp"
inline constexpr int LV_LABEL_LONG_WRAP=2;
#include "companion/provider_room_view.hpp"
#include <cstdio>
int main(){using namespace companion::room;int cycles=0;
 for(auto size:{std::pair<int,int>{320,240},{480,320},{640,480}})for(auto provider:companion_modes::Catalog){screen.w=size.first;screen.h=size.second;
  auto owner=lv_obj_create(&screen);lv_obj_set_size(owner,screen.w,screen.h);View view;view.Open(owner,provider.provider);Snapshot snapshot;view.Render(snapshot,0);assert(Text("Not connected")&&!Button("Approve"));
  assert(view.Root()->w==view.Root()->h&&view.Root()->x>=0&&view.Root()->x+view.Root()->w<=screen.w&&view.Root()->h<=screen.h-24);
  snapshot.scene="desk_work";view.Render(snapshot,100);assert(!Text("Working"));
  snapshot.verified=true;view.Render(snapshot,100);assert(Text("Working"));
  for(const char* state:{"idle_decoration","error","completion","waiting_receipt","unknown"}){snapshot.scene=state;view.Render(snapshot,9000);assert(!Button("Approve"));}
  int calls=0;view.SetSubmit([&](const std::string& choice,const std::string& ticket){++calls;assert(choice=="approve"&&ticket=="ticket2");return true;});
  snapshot.scene="decision_sign";snapshot.ticket="ticket1";snapshot.summary="Apply reviewed fixture patch only";snapshot.request="r1";snapshot.operation=std::string(64,'a');view.Render(snapshot,1000);
  auto approve=Button("Approve");assert(approve&&Button("Deny"));Press(approve);
  snapshot.ticket="ticket2";view.Render(snapshot,1001);ReleaseClick(approve);assert(!calls);Click(approve);assert(calls==1&&approve->disabled);
  view.Visible(false);assert(!Button("Approve"));view.Detach();lv_obj_delete(owner);
  // root is the first live top-level object for this cycle, including nested ownership.
  assert(!LiveObjects());objects.clear();++cycles;
 }
 std::printf("%d provider worlds across three sizes; truthful states, square geometry, exact decision ticket and visibility lifecycle pass\n",cycles);
}'''
asm='.section __DATA,__const\n.globl _binary_mira_idle_frame_png_start\n_binary_mira_idle_frame_png_start:\n.incbin "'+str(png)+'"\n.globl _binary_mira_idle_frame_png_end\n_binary_mira_idle_frame_png_end:\n' if __import__('sys').platform=='darwin' else '.section .rodata\n.globl _binary_mira_idle_frame_png_start\n_binary_mira_idle_frame_png_start:\n.incbin "'+str(png)+'"\n.globl _binary_mira_idle_frame_png_end\n_binary_mira_idle_frame_png_end:\n'
with tempfile.TemporaryDirectory(prefix='stackchan-room-view-') as temp:
 p=Path(temp);(p/'lvgl.h').write_text('#include "companion_ui_fixture.hpp"\n');(p/'test.cpp').write_text('asm('+json.dumps(asm)+');\n'+fixture)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(p),'-I'+str(Path(__file__).parent),'-I'+str(root/'firmware/main'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
