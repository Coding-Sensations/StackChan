#include "muse_codec_fixture.hpp"
#include "voice_device_ui_fixture.hpp"
#include <companion/muse_voice_setup_ui.hpp>
#include <thread>
#include <companion/muse_direct_local_ui.cc>
struct NetworkUse:LocalNetworkUse{bool*valid;explicit NetworkUse(bool&v):valid(&v){}bool Current()override{return *valid;}};
struct Port:LocalPort{
 lv_indev_t pointer;bool foreground=true,network=true,ready=true,idle=true,accept=true;int stages=0,stops=0,clocks=0;uint64_t generation=0,stopped=0;muse_native_review::Stamp stamp{1000,1760000000000LL};VoiceAdmissionScope staged;std::function<void()> hook,prepare_hook;uint64_t active_generation=0;
 uint64_t Now()override{return stamp.mono_us/1000;}bool Foreground()override{return foreground;}
 bool Identity(LocalIdentity&o)override{o={"synthetic-device","hatch-link:00:00:00:00:00:01","00:00:00:00:00:01","1.6.0"};return true;}
 bool Network(LocalNetwork&o)override{o={};o.valid=network;o.epoch=7;o.ssid[0]='X';o.bssid[5]=1;return network;}
 bool NetworkMatches(const LocalNetwork&o)override{return network&&o.valid&&o.epoch==7;}std::unique_ptr<LocalNetworkUse> Borrow(const LocalNetwork&)override{return std::make_unique<NetworkUse>(network);}
 bool PairIdle()override{return true;}bool ChatIdle()override{return true;}bool PreparePair(const PairingGrant&,std::string_view)override{return false;}bool PrepareChat(const SessionGrant&)override{return false;}void Stop(LocalMode)override{}bool Text(const std::string&,const std::string&,const std::string&)override{return false;}bool RandomId(std::string&)override{return false;}
 void* VoiceInput()override{return &pointer;}bool VoiceClock(muse_native_review::Stamp&out)override{++clocks;if(hook){auto f=std::move(hook);f();}out=stamp;return true;}bool VoiceIdentity(uint64_t&g,std::string&id)override{g=++generation;id=call;return true;}bool VoiceReady()override{return ready;}bool VoiceIdle()override{return idle;}bool PrepareVoice(const VoiceAdmissionScope&s)override{++stages;staged=s;if(accept){idle=false;active_generation=s.budget.generation;}if(prepare_hook)prepare_hook();return accept;}void StopVoice(uint64_t g)override{++stops;stopped=g;if(g==active_generation){idle=true;active_generation=0;}}
};
using Setup=VoiceAdmissionSetupUi<LocalPort>;
static lv_obj_t* SetupButton(const char*label){for(auto&object:objects){auto*b=object.get();if(b->kind=="button"&&Visible(b)){auto*l=lv_obj_get_child(b,0);if(l&&l->text==label)return b;}}return nullptr;}
static void Touch(Port&p,const char*label){auto*b=SetupButton(label);assert(b);Tap(b,p.pointer);}
static void VmText(Port&p,const std::string&value){Touch(p,"Enter VM");for(char c:value){char label[2]={c,0};unsigned pages=0;while(!SetupButton(label)){Touch(p,"Next keys");assert(++pages<8);}Touch(p,label);}Touch(p,"Done");}
static void Vm(Port&p){VmText(p,"vm-1");}
static void Review(Port&p){Touch(p,"Review device/Wi-Fi");unsigned pages=0;while(Button("Next")){Touch(p,"Next");assert(++pages<50);}}
static void Confirm(Port&p,unsigned mask=7){Vm(p);Review(p);for(unsigned i=0;i<3;++i){if(mask&(1u<<i))Touch(p,"Allow");Touch(p,i==2?"Continue":"Next permission");}}
int main(){
#if !STACKCHAN_MUSE_RAM_ONLY
 Reset();screen.w=320;screen.h=240;Port denied;Setup closed(denied);assert(!closed.Open(&screen)&&!denied.stages&&!denied.clocks&&!LiveObjects());assert(closed.Close());return 0;
#endif
 Reset();screen.w=320;screen.h=240;Port port;Setup ui(port);assert(ui.Open(&screen)&&!port.stages);Vm(port);Review(port);for(unsigned i=0;i<3;++i){Touch(port,"Allow");Touch(port,i==2?"Continue":"Next permission");}assert(Button("Prepare admission"));Click(Button("Prepare admission"));assert(!port.stages);Touch(port,"Prepare admission");assert(port.stages==1&&port.staged.node=="synthetic-device"&&port.staged.vm=="vm-1"&&port.staged.credential_current()&&port.staged.network_current()&&port.staged.voice_current());assert(ui.Close()&&port.stops==1&&port.stopped==port.staged.budget.generation&&!port.staged.voice_current()&&!LiveObjects());
 {Port p;Setup other(p);assert(other.Open(&screen));Vm(p);Review(p);for(unsigned i=0;i<3;++i){Touch(p,"Allow");Touch(p,i==2?"Continue":"Next permission");}p.prepare_hook=[&]{assert(!other.Close());assert(!p.staged.voice_current());};Touch(p,"Prepare admission");assert(other.Poll()&&other.Closed()&&!p.staged.voice_current()&&!LiveObjects());}
 
 for(unsigned mask=0;mask<8;++mask){Port p;Setup other(p);assert(other.Open(&screen));Confirm(p,mask);assert(bool(Button("Prepare admission"))==(mask==7));assert(!p.stages);assert(other.Close()&&!LiveObjects());}
 for(unsigned fault=0;fault<5;++fault){Port p;Setup other(p);assert(other.Open(&screen));Confirm(p);auto*button=Button("Prepare admission");lv_indev_t foreign;
  if(fault==0)Tap(button,foreign);
  if(fault==1)Up(button,p.pointer);
  if(fault==2){Down(button,p.pointer);native_tick+=10001;Up(button,p.pointer);}
  if(fault==3){Down(button,p.pointer);assert(other.Poll());Touch(p,"Back");Up(button,p.pointer);}
  if(fault==4){Down(button,p.pointer);active_input=event_input=&p.pointer;lv_event_t lost{button,button->callback_user,LV_EVENT_PRESS_LOST};button->callback(&lost);active_input=event_input=nullptr;Up(button,p.pointer);}
  assert(!p.stages&&other.Close()&&!LiveObjects());}
 for(unsigned fault=0;fault<5;++fault){Port p;Setup other(p);assert(other.Open(&screen));Confirm(p);if(fault==0)p.ready=false;if(fault==1)p.network=false;if(fault==2)p.foreground=false;if(fault==3)p.stamp.mono_us=-1;if(fault==4)p.accept=false;Touch(p,"Prepare admission");assert((fault==4?p.stages==1:p.stages==0));assert(other.Close()&&!LiveObjects());if(fault==4)assert(!p.staged.voice_current());}
 for(unsigned fault=0;fault<4;++fault){Port p;Setup other(p);assert(other.Open(&screen));Confirm(p);Touch(p,"Prepare admission");assert(p.staged.voice_current());if(fault==0)p.network=false;if(fault==1)p.foreground=false;if(fault==2)p.stamp.mono_us=p.staged.budget.expires_us;if(fault==3)--p.stamp.wall_ms;assert(!p.staged.voice_current());p.network=p.foreground=true;p.stamp={1000,1760000000000LL};assert(!p.staged.voice_current());assert(other.Close()&&!LiveObjects());}
 {Port p;Setup old(p);assert(old.Open(&screen));Confirm(p);Touch(p,"Prepare admission");auto first=p.staged;auto old_generation=first.budget.generation;p.idle=true;Setup next(p);assert(next.Open(&screen));Confirm(p);Touch(p,"Prepare admission");assert(p.staged.budget.generation!=old_generation&&p.staged.voice_current());assert(old.Close()&&p.active_generation==p.staged.budget.generation&&p.staged.voice_current());assert(!first.voice_current()&&next.Close()&&!LiveObjects());}
 {Port p;VoiceAdmissionScope retained;{Setup other(p);assert(other.Open(&screen));Confirm(p);Touch(p,"Prepare admission");retained=p.staged;assert(other.Close());}assert(!retained.voice_current()&&!LiveObjects());}
 {Port p;Setup other(p);assert(other.Open(&screen));Confirm(p);Touch(p,"Prepare admission");std::atomic<bool> running{true};std::thread worker([&]{while(running.load())(void)p.staged.voice_current();});assert(other.Close());running.store(false);worker.join();assert(!p.staged.voice_current()&&!LiveObjects());}

 for(const char*mode:{"Pair device","Prepare text chat"}){Port p;LocalSetupUi parent(p);parent.Open(&screen);Touch(p,"Prepare voice admission");Confirm(p);Touch(p,"Prepare admission");auto old=p.staged;assert(old.voice_current()&&!p.idle);Touch(p,"Return to Settings");parent.Poll();assert(!parent.Visible());parent.Open(&screen);assert(parent.Visible());Touch(p,mode);assert(!old.voice_current()&&p.idle&&p.stops==1&&p.stopped==old.budget.generation);parent.Close();assert(!LiveObjects());}
 {struct MetadataPort:Port{bool Identity(LocalIdentity&o)override{o={std::string(65,'n'),std::string(53,'d'),"00:00:00:00:00:01","1.6.0"};return true;}bool Network(LocalNetwork&o)override{Port::Network(o);o.ssid={65,92,66,255,0};return true;}}p;Setup other(p);assert(other.Open(&screen));const std::string vm(128,'v');VmText(p,vm);Touch(p,"Review device/Wi-Fi");for(const auto&item:std::vector<std::pair<std::string,std::string>>{{"Device",std::string(53,'d')},{"Node",std::string(65,'n')},{"VM",vm},{"Wi-Fi","A\\x5cB\\xff"},{"BSSID","00:00:00:00:00:01"}}){for(size_t i=0;i<item.second.size();i+=18){std::string heading=item.first+" ("+std::to_string(i/18+1)+"/"+std::to_string((item.second.size()+17)/18)+")";assert(Text(heading)&&Text(item.second.substr(i,18)));Touch(p,"Next");}}assert(Text("Separate admission permissions")&&!p.stages);assert(other.Close()&&!LiveObjects());}
 std::puts("PASS native-input admission issuer with synthetic local port; no real permission/token/runtime action");}
