"""Compile the real provider app with inert LVGL and HAL boundaries."""
from pathlib import Path
import subprocess,tempfile,json
root=Path(__file__).resolve().parents[3]
fixture=Path(__file__).with_name('companion_ui_fixture.hpp').parent
with tempfile.TemporaryDirectory(prefix='stackchan-provider-fixture-') as tmp:
    p=Path(tmp)
    files={
      'mooncake.h':'''#pragma once
#include "companion_ui_fixture.hpp"
namespace mooncake {class AppAbility {public: struct Info {std::string name;const void* icon=nullptr;} info;bool closed=false;virtual ~AppAbility()=default;virtual void onOpen(){}virtual void onRunning(){}virtual void onClose(){};Info& setAppInfo(){return info;}Info getAppInfo(){return info;}void close(){closed=true;}};}
''',
      'lvgl.h':'#pragma once\n#include "companion_ui_fixture.hpp"\ninline constexpr int LV_LABEL_LONG_WRAP=3,LV_LABEL_LONG_SCROLL_CIRCULAR=4;\n',
      'esp_timer.h':'#pragma once\ninline long long esp_timer_get_time(){return 1000000;}\n',
      'hal/hal.h':'#pragma once\n#include "companion_ui_fixture.hpp"\n',
      'assets/assets.h':'#pragma once\nnamespace assets {inline int get_image(const char*){return 1;}}\n',
      'apps/common/home_indicator/home_indicator.h':'''#pragma once
#include "companion_ui_fixture.hpp"
namespace view {inline std::function<void()> home;template<class F>void create_home_indicator(F f,unsigned,unsigned,lv_obj_t*,bool){home=f;}inline void destroy_home_indicator(){home={};}}
'''}
    for name,data in files.items():
        dest=p/name;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(data)
    (p/'test.cpp').write_text('''#include "apps/stock_shell/provider_companion_app.cc"
#include <cstdio>
namespace companion::muse {struct InertLocalPort final:LocalPort{uint64_t Now()override{return 1000;}bool Foreground()override{return true;}bool Identity(LocalIdentity&)override{return false;}bool Network(LocalNetwork&)override{return false;}bool NetworkMatches(const LocalNetwork&)override{return false;}std::unique_ptr<LocalNetworkUse> Borrow(const LocalNetwork&)override{return {};}bool PairIdle()override{return true;}bool ChatIdle()override{return true;}bool PreparePair(const PairingGrant&,std::string_view)override{assert(false);return false;}bool PrepareChat(const SessionGrant&)override{assert(false);return false;}void Stop(LocalMode)override{}bool Text(const std::string&,const std::string&,const std::string&)override{assert(false);return false;}bool RandomId(std::string&)override{return false;}};LocalPort& DeviceLocalPort(){static InertLocalPort p;return p;}}
namespace companion::muse {bool prepared=false,visible=false;uint32_t ticket=0,received=0;int starts=0,native_starts=0;bool native_prepared=false;SessionSnapshot native_snapshot;
bool NativeSessionPrepared(){return native_prepared;}bool StartPreparedNativeSession(){++native_starts;native_prepared=false;native_snapshot.stage=SessionStage::Starting;return true;}SessionSnapshot ViewNativeSession(){return native_snapshot;}
void DeviceProviderVisible(bool value){visible=value;if(!value){prepared=false;ticket=0;native_prepared=false;native_snapshot={};}}bool DevicePairingPrepared(){return prepared;}bool StartPreparedDevicePairing(){++starts;prepared=false;return true;}uint32_t DevicePhysicalTicket(){return ticket;}bool QueueDevicePhysicalPress(uint32_t captured){received=captured;return captured==ticket;}}
namespace companion::room {bool fixture_muse_prepared=false;int fixture_muse_connects=0;void PollMuseRoom(){}void StopMuseRoom(){fixture_muse_prepared=false;}bool MuseRoomPrepared(){return fixture_muse_prepared;}bool ConnectPreparedMuseRoom(){++fixture_muse_connects;fixture_muse_prepared=false;return true;}void SelectProvider(Provider){}Snapshot ViewProvider(Provider,uint64_t){return {};}bool QueueDecision(Provider,const std::string&,const std::string&,uint64_t){return false;}void DisconnectProvider(Provider){}}
int main(){using namespace companion_modes;screen.w=320;screen.h=240;
 assert(!stock_tools::MakeProviderCompanionApp(Provider::Mira));
 assert(!stock_tools::MakeProviderCompanionApp(Provider::StackChan));
 assert(!stock_tools::MakeProviderCompanionApp(static_cast<Provider>(99)));
 for(const auto& descriptor:Catalog){
  for(const auto& other:Catalog)if(descriptor.provider!=other.provider){assert(std::string(descriptor.label)!=other.label);assert(std::string(descriptor.world_key)!=other.world_key);}
  if(descriptor.route!=Route::AdapterRequired)continue;
  for(int n=0;n<20;++n){
   auto app=stock_tools::MakeProviderCompanionApp(descriptor.provider);assert(app&&app->getAppInfo().name==descriptor.label);
   app->onOpen();assert(Text(descriptor.label)&&Text("Not connected")&&Button("Settings")&&!Button("Back"));
   Click(Button("Settings"));app->onRunning();assert(Text(descriptor.setup)&&Button("Back"));
   assert(Text("Audio/camera status unverified")&&!Button("Pair")&&!Button("Connect")&&!Button("Approve"));
   for(const auto& node:objects)if(Visible(node.get())&&node->callback){assert(node->x>=0&&node->y>=0&&node->x+node->w<=320&&node->y+node->h<=240);}
   Click(Button("Back"));app->onRunning();assert(Text("Not connected")&&!Button("Back"));view::home();
   app->onRunning();assert(app->closed);app->onClose();assert(!LiveObjects()&&!view::home);
  }
 }
 auto muse=stock_tools::MakeProviderCompanionApp(Provider::Muse);muse->onOpen();Click(Button("Settings"));muse->onRunning();assert(!Button("Connect Muse")&&companion::room::fixture_muse_connects==0);
 companion::room::fixture_muse_prepared=true;muse->onRunning();assert(Button("Connect Muse")&&companion::room::fixture_muse_connects==0);
 Click(Button("Connect Muse"));assert(companion::room::fixture_muse_connects==0);muse->onRunning();assert(companion::room::fixture_muse_connects==1&&!Button("Connect Muse"));
 companion::muse::prepared=true;muse->onRunning();assert(Button("Pair Muse device")&&companion::muse::starts==0);
 Click(Button("Pair Muse device"));assert(companion::muse::starts==0);muse->onRunning();assert(companion::muse::starts==1&&!Button("Pair Muse device"));
 companion::muse::ticket=42;muse->onRunning();assert(Button("Confirm Muse pairing"));Click(Button("Confirm Muse pairing"));companion::muse::ticket=43;muse->onRunning();assert(companion::muse::received==42);
 companion::muse::ticket=0;companion::muse::native_prepared=true;muse->onRunning();assert(Button("Connect direct Muse")&&companion::muse::native_starts==0);Click(Button("Connect direct Muse"));assert(companion::muse::native_starts==0);muse->onRunning();assert(companion::muse::native_starts==1&&!Button("Connect direct Muse"));
 companion::muse::native_snapshot.stage=companion::muse::SessionStage::ReplyEnded;companion::muse::native_snapshot.reply_ended=true;companion::muse::native_snapshot.caption="Fixture reply";muse->onRunning();assert(Text("Reply received")&&Text("Reply ended")&&Text("Fixture reply")&&!Text("Completed"));
 view::home();muse->onRunning();muse->onClose();assert(!LiveObjects()&&!companion::muse::visible&&companion::muse::ticket==0);
 puts("80 distinct provider setup cycles, real production factory, Back/swipe cleanup and truthful no-runtime controls plus prepared-only explicit Muse Connect gate pass");
}''')
    png=root/'firmware/main/assets/mira_idle_frame.png'
    section='.section __DATA,__const\n' if __import__('sys').platform=='darwin' else '.section .rodata\n'
    assembly=section+'.globl _binary_mira_idle_frame_png_start\n_binary_mira_idle_frame_png_start:\n.incbin "'+str(png)+'"\n.globl _binary_mira_idle_frame_png_end\n_binary_mira_idle_frame_png_end:\n'
    code=(p/'test.cpp').read_text();(p/'test.cpp').write_text('asm('+json.dumps(assembly)+');\n'+code)
    subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(p),'-I'+str(fixture),'-I'+str(root/'firmware/main'),'-I'+str(root/'firmware/components/muse_direct_protocol'),'-I'+str(root/'firmware/components/muse_direct_protocol/pairing'),'-I'+str(root/'firmware/components/muse_direct_protocol/noise_core/include'),'-I'+str(root/'firmware/tests/vendor/cjson'),str(p/'test.cpp'),str(root/'firmware/main/companion/muse_direct_local_ui.cc'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
