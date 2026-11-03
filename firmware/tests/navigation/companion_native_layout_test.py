"""Actual OpenView plus Details lifecycle; inert runtime/LVGL boundaries."""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[3]
s=(root/'firmware/main/companion/boot.cc').read_text()
def body(sig):
 start=s.index(sig);end=s.index('{',start)+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
fixture=r'''
#include "companion_ui_fixture.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <climits>
int scroll_y=0;constexpr int LV_ANIM_OFF=0;
const char* lv_label_get_text(lv_obj_t* p){return p->text.c_str();}
lv_obj_t* lv_obj_get_parent(lv_obj_t* p){return p->parent;}
void lv_obj_scroll_to_y(lv_obj_t*,int y,int){scroll_y=y;}
inline constexpr int LV_OPA_COVER=255,LV_EVENT_LONG_PRESSED=2,LV_SIZE_CONTENT=-1,LV_LABEL_LONG_WRAP=2;
#include "companion/companion_details_view.hpp"
#include "companion/companion_initial_status.hpp"
#ifndef STACKCHAN_COMPANION_REMEMBERED_RUNTIME
#define STACKCHAN_COMPANION_REMEMBERED_RUNTIME 0
#endif
#include "companion/companion_header_layout.hpp"
#include "companion/companion_reactive_view.hpp"
#include "companion/provider_room_runtime.h"
namespace companion::room {void SelectProvider(Provider){}bool QueueDecision(Provider,const std::string&,const std::string&,uint64_t){return false;}}
namespace companion {
struct Closing {bool Pending(){return false;}};
struct Reactive{bool selected=false,stopping=false;bool Selected(){return selected;}void Select(bool value){selected=value;}void RequestStop(){stopping=true;}desk::ReactiveFrame SnapshotCurrent(uint32_t){desk::ReactiveFrame f;f.apply_face=selected;if(stopping)f.status.stop="Stop pending";return f;}};

struct LocalWake{void PublishControl(lv_obj_t*,uint64_t){}void QueueExplicitLocalWakeOptIn(lv_obj_t*,uint64_t){}};
struct Pair{unsigned clicks=0;void Reset(){}void Click(){++clicks;}};
struct Socket{enum class State{ConsentRequired};void PublishCameraPhysicalControls(lv_obj_t*,lv_obj_t*,uint64_t){}void CaptureCameraLocalChoice(lv_obj_t*,uint64_t){}State GetState(){return State::ConsentRequired;}unsigned Epoch(){return 3;}};
using CompanionSocket=Socket;
struct Intent{void CapturePhysicalListen(int){}};
struct Runtime {Closing closing_view;Pair pair_ui;desk::CompanionReactiveView reactive_view;Reactive reactive;LocalWake local_wake;Socket socket;Intent local_intent;CompanionDetailsView details_view;
 lv_obj_t *status=nullptr,*diagnostic_button=nullptr,*public_status_button=nullptr,*remember_button=nullptr,*connect_button=nullptr,*listen_button=nullptr,*camera_allow_button=nullptr,*camera_decline_button=nullptr,*local_wake_button=nullptr,*pair_button=nullptr,*public_status_root=nullptr;
 lv_obj_t *status_panel=nullptr,*connection_badge=nullptr,*presentation_toggle=nullptr,*details_toggle=nullptr,*settings_button=nullptr;
 int view_width=320,view_height=240;bool view_supported=false,character_surface=false,presentation_prompt=false,remembered_view_pending=false;
 bool grant_connected=false;uint64_t local_view_generation=0;const char* message=nullptr;static int LocalIntentClock(void*){return 0;}};
Runtime& runtime(){static Runtime r;return r;}
bool diagnostic_requested=false,public_status_requested=false,pair_requested=false,connect_requested=false,listen_requested=false,remember_requested=false,forget_requested=false,mute_requested=false,confirm_requested=false,reject_requested=false,physical_mute_requested=false;
unsigned listen_epoch_requested=0,remembered_listen_requested=0;
namespace wake_runtime{bool kLocalWakeOptInHookEnabled=false;}
int boot_requests=0;bool start_allowed=false;uint64_t esp_timer_get_time(){return 1000000;}
void RenderClosingCompanionView(){}bool RememberedRuntimeStopped(){return true;}bool RememberedStartAllowed(){return start_allowed;}void BootRememberedRuntime(){++boot_requests;}void ClosePublicStatus(){}bool RememberedRuntimePresent(){return false;}const char* RememberedStatusText(){return nullptr;}
unsigned CaptureRememberedListen(){return 0;}bool RouteRememberedListen(unsigned,bool){return false;}
void CaptureRememberedLocalProcessingGesture(lv_obj_t*,uint64_t,unsigned){}void RegisterRememberedPhysicalControl(lv_obj_t*,uint64_t){}void RegisterRememberedCameraControls(lv_obj_t*,lv_obj_t*,uint64_t){}bool RememberedTransportActive(){return false;}void CaptureRememberedCameraChoice(lv_obj_t*,uint64_t){}void InterruptRememberedRuntimeImmediately(){}
ACTUAL
}
bool overlaps(lv_obj_t* a,lv_obj_t* b){return a->x<b->x+b->w&&a->x+a->w>b->x&&a->y<b->y+b->h&&a->y+a->h>b->y;}
void check(lv_obj_t* parent){std::vector<lv_obj_t*> buttons;
 for(auto& o:objects)if(Visible(o.get())&&o->parent==parent&&o->callback){assert(o->x>=0&&o->y>=0&&o->x+o->w<=parent->w&&o->y+o->h<=parent->h);buttons.push_back(o.get());}
 for(auto a:buttons)for(auto b:buttons)if(a!=b)assert(!overlaps(a,b));
}

void snapshot(const std::string& path){
 std::ofstream out(path);out<<"[";bool first=true;
 for(auto& p:objects)if(Visible(p.get())&&p->w>=0&&(p->h>=0||p->kind=="text")){
  if(!first)out<<",";first=false;int x=p->x,y=p->y;for(auto parent=p->parent;parent&&parent!=&screen;parent=parent->parent){x+=parent->x;y+=parent->y;}
  out<<"{\"x\":"<<x<<",\"y\":"<<y<<",\"w\":"<<p->w<<",\"h\":"<<p->h<<",\"color\":"<<p->color<<",\"opacity\":"<<p->opacity<<",\"scale\":"<<p->scale<<",\"radius\":"<<p->radius<<",\"kind\":\""<<p->kind<<"\",\"text\":\"";
  for(char c:p->text){if(c=='\n')out<<"\\n";else if(c=='"'||c=='\\')out<<'\\'<<c;else out<<c;}out<<"\"}";
 }out<<"]";
}
lv_obj_t* character(){for(auto& p:objects)if(!p->deleted&&p->parent==&screen&&p->x==0&&p->y==0&&p->w==screen.w&&p->h==screen.h)return p.get();return nullptr;}
void removeView(){using namespace companion;runtime().details_view.Detach();runtime().reactive_view.Detach();for(auto& o:objects)if(o->parent==&screen&&!o->deleted)lv_obj_delete(o.get());}
int main(int argc,char** argv){using namespace companion;
 assert(std::string(InitialViewStatus(false,nullptr)).find("disabled")!=std::string::npos);
 assert(std::string(InitialViewStatus(true,nullptr)).find("unavailable")!=std::string::npos);
 for(const char* state:{"Saved Mac - connecting Wi-Fi","Saved Mac - authenticating; muted","Saved Mac connected - tap Listen","Saved Mac - retry waiting; muted","Saved pairing expired - muted"})assert(std::string(InitialViewStatus(true,state))==state);
 int cycles=0;
 for(auto size:{std::pair<int,int>{320,240},{320,320},{480,320},{640,480}})for(bool wake:{false,true})for(int cycle=0;cycle<5;++cycle){
 screen.w=size.first;screen.h=size.second;wake_runtime::kLocalWakeOptInHookEnabled=wake;runtime().reactive.stopping=false;OpenView(&screen);++cycles;
 assert(runtime().reactive.Selected()&&runtime().view_supported);
 auto face=character();assert(face&&Visible(face)&&face->w*face->h>screen.w*screen.h/2);
 assert(!Button("Pair")&&!Button("Connect")&&!Button("Listen")&&!Button("Mira")&&!Button("Tools")&&Button("Settings"));
 assert(Text("Audio unverified")&&Text("Audio unverified")->y==screen.h-24);check(&screen);
 for(auto& o:objects)if(Visible(o.get())&&o->parent==&screen&&o->callback&&o->y<44)assert(o.get()==Button("Settings")); // wrapper owns188..320
 lv_obj_t* art=nullptr;
 for(auto& o:objects)if(o->parent==face&&o->kind=="image")art=o.get();
 assert(art&&art->image_source&&art->w==320&&art->h==240);
 const auto source=static_cast<const lv_image_dsc_t*>(art->image_source);assert(source->data_size==60156&&source->data[0]==137&&source->data[1]==80&&source->data[2]==78&&source->data[3]==71);
 const int drawn_w=art->w*art->scale/256,drawn_h=art->h*art->scale/256;
 assert(drawn_w<=face->w&&drawn_h<=face->h); // aspect fit, unchanged PNG bytes
 const bool save=argc>1&&screen.w==320&&screen.h==240&&!wake&&cycle==0;
 if(save)snapshot(std::string(argv[1])+"-idle.json");
 desk::ReactiveFrame privacy;privacy.apply_face=true;
 privacy.status.mic="Local wake capture active";runtime().reactive_view.Render(privacy,false);assert(Text("Wake mic on"));
 privacy.status.mic="Listening: capture active";runtime().reactive_view.Render(privacy,false);assert(Text("Mic on"));
 privacy.status.output="Playback active";runtime().reactive_view.Render(privacy,false);assert(Text("Mic + speaker on"));
 privacy.status.mic="Mic off confirmed";runtime().reactive_view.Render(privacy,false);assert(Text("Speaker on"));
 privacy.status.output="Output off confirmed";runtime().reactive_view.Render(privacy,false);assert(Text("Muted"));
 privacy.status.output="Playback completion unverified";runtime().reactive_view.Render(privacy,false);assert(Text("Audio unverified"));
 privacy.status.stop="Stop unconfirmed";runtime().reactive_view.Render(privacy,false);assert(Text("Stop unconfirmed"));
 privacy={};privacy.apply_face=true;runtime().reactive_view.Render(privacy,false);
 const int prior_boot=boot_requests;
 Click(Button("Settings"));assert(!runtime().reactive.Selected()&&!Visible(face)&&Button("Mira")&&Button("Pair")&&Button("Connect")&&Button("Listen"));check(&screen);
 assert(boot_requests==prior_boot);
 assert(Button("Tools")&&!Button("Settings"));
 SetViewStatus(runtime().status,"Long error");scroll_y=42;SetViewStatus(runtime().status,"Long error");assert(scroll_y==42);SetViewStatus(runtime().status,"New consent prompt");assert(scroll_y==0);
 if(save)snapshot(std::string(argv[1])+"-controls.json");
 auto clicks=runtime().pair_ui.clicks;Click(Button("Pair"));assert(runtime().pair_ui.clicks==clicks+1);
 Click(Button("Connect"));assert(connect_requested);connect_requested=false;
 Click(Button("Listen"));assert(listen_epoch_requested==3);listen_epoch_requested=0;
 Click(Button("Mira"));assert(runtime().reactive.Selected()&&Visible(face)&&!Button("Pair"));
 runtime().details_view.UpdatePrompt("pair:123456");UpdateViewPresentation("Compare code: 123456",true,false);
 assert(!Visible(face)&&Button("Pair")&&!Button("Connect")&&!Button("Listen")&&runtime().presentation_toggle->disabled);
 Click(Button("Tools"));assert(Button("Mute")&&Button("Export")&&Button("Forget"));auto panel=Button("Mute")->parent;check(panel);
 assert(panel->y+panel->h<=screen.h-24); // privacy remains outside every settings panel
 runtime().details_view.UpdatePrompt("pair:123456");assert(Button("Mute"));
 runtime().details_view.UpdatePrompt("camera:allow?");UpdateViewPresentation("Allow camera?",false,true);
 assert(!Button("Mute")&&!Visible(face)&&Button("Allow camera")&&Button("Decline camera")&&!Button("Pair"));
 if(save)snapshot(std::string(argv[1])+"-camera.json");
 Click(Button("Tools"));runtime().details_view.UpdatePrompt("camera:allow?");assert(Button("Mute"));
 Click(Button("Mute"));assert(physical_mute_requested&&mute_requested);physical_mute_requested=mute_requested=false;assert(!Button("Mute"));
 runtime().details_view.UpdatePrompt("");UpdateViewPresentation("Saved Mac - authenticating; muted",false,false);
 runtime().reactive.stopping=false;UpdateViewPresentation("Saved Mac - authenticating; muted",false,false);
 assert(Visible(face)&&!runtime().presentation_toggle->disabled&&Text("Saved Mac - authenticating; muted"));
 desk::ReactiveFrame extreme;extreme.apply_face=true;extreme.eye_x=INT_MAX;extreme.eye_y=INT_MIN;extreme.mouth_weight=INT_MAX;runtime().reactive_view.Render(extreme,false);
 assert(art->image_source==source); // level telemetry never redraws/replaces approved art
 runtime().reactive.RequestStop();UpdateViewPresentation("Stop unconfirmed",false,false,true);
 assert(!Visible(face)&&!Button("Tools")&&!Button("Mira")&&!Button("Controls")&&!Button("Pair")&&!Button("Connect")&&!Button("Listen"));
 assert(Text("Stopping audio"));
 if(save)snapshot(std::string(argv[1])+"-closing.json");
 removeView();assert(!LiveObjects());
 }
 for(auto size:{std::pair<int,int>{240,320},{200,200}}){screen.w=size.first;screen.h=size.second;const int prior_boot=boot_requests;start_allowed=true;OpenView(&screen);start_allowed=false;
 assert(!runtime().view_supported&&boot_requests==prior_boot&&!Button("Settings")&&!Button("Tools"));
 for(auto& p:objects)if(Visible(p.get()))assert(p->x>=0&&p->y>=0&&p->x+p->w<=screen.w&&p->y+p->h<=screen.h);removeView();}
 screen.w=320;screen.h=240;start_allowed=true;const int prior_boot=boot_requests;OpenView(&screen);assert(boot_requests==prior_boot&&Visible(character()));BeginRememberedViewConnection();assert(boot_requests==prior_boot+1);BeginRememberedViewConnection();Click(Button("Settings"));Click(Button("Mira"));assert(boot_requests==prior_boot+1);start_allowed=false;removeView();
 start_allowed=true;const int cancelled=boot_requests;OpenView(&screen);CancelRememberedViewConnection();BeginRememberedViewConnection();assert(boot_requests==cancelled);start_allowed=false;removeView();
 std::printf("%d character-first cycles across four supported sizes, two fallback sizes, approved Mira artwork, touch modes, prompts, privacy and cleanup pass; no runtime/capture IO\n",cycles);
}
'''.replace('ACTUAL',body('void SetViewStatus(')+'\n'+body('void UpdateViewPresentation(')+'\n'+body('void HeaderRect(')+'\n'+body('void CancelRememberedViewConnection(')+'\n'+body('void BeginRememberedViewConnection(')+'\n'+body('void OpenView(lv_obj_t* parent)'))
png=root/'firmware/main/assets/mira_idle_frame.png'
import hashlib
assert hashlib.sha256(png.read_bytes()).hexdigest()=='93432bf8c86216c9c164d026ab96568cfd966a760a55e12da165af5a35e4ce79'
section='.section __DATA,__const\n' if __import__('sys').platform=='darwin' else '.section .rodata\n'
asm=section+'.globl _binary_mira_idle_frame_png_start\n_binary_mira_idle_frame_png_start:\n.incbin "'+str(png)+'"\n.globl _binary_mira_idle_frame_png_end\n_binary_mira_idle_frame_png_end:\n'
fixture='asm('+__import__('json').dumps(asm)+');\n'+fixture
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(fixture);(p/'lvgl.h').write_text('#include "companion_ui_fixture.hpp"\n')
 for enabled in [0,1]:
  subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-DSTACKCHAN_COMPANION_REMEMBERED_RUNTIME='+str(enabled),'-I'+str(p),'-I'+str(Path(__file__).parent),'-I'+str(root/'firmware/main'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
  args=[str(p/'test')]
  if os.environ.get('STACKCHAN_UI_SIMULATION_OUTPUT'):args.append(os.environ['STACKCHAN_UI_SIMULATION_OUTPUT'])
  subprocess.run(args,check=True)
