"""Run production entry/control bodies at inert UI, network and HAL boundaries.

Regressions caught: configured Stackchan constructs an intermediate gate; Mira
publishes partially assembled setup UI at a lock release; session Home is duplicated by a persistent footer. Also cover setup, busy ownership, cancellation and
repeated lifecycle cleanup. No device, microphone or call is opened.
"""
from pathlib import Path
import argparse
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[3]
main = repo / 'firmware/main'
parser = argparse.ArgumentParser()
parser.add_argument('--case', choices=['stackchan', 'mira', 'controls'])
args = parser.parse_args()

def body(source, signature):
    start = source.index(signature)
    begin = source.index('{', start)
    depth, end = 1, begin + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

common = r'''
#include "companion_ui_fixture.hpp"
#include <atomic>
#include <cstdio>
#include <mutex>
#include <optional>
namespace mooncake {
class AppAbility {
 struct Info {std::string name;};Info info_;
public:
 bool closed=false;virtual ~AppAbility()=default;
 Info& setAppInfo(){return info_;}Info getAppInfo(){return info_;}
 virtual void onOpen(){}virtual void onRunning(){}virtual void onClose(){}
 void close(){closed=true;}
};
}
'''
stock = (main / 'apps/stock_agent/stock_agent.cc').read_text()
stackchan = common + r'''
bool configured=true,network_ready=true,audio_busy=false,settings_requested=false;
unsigned preparations=0;
struct HAL {bool isAppConfiged(){return configured;}};
HAL& GetHAL(){static HAL h;return h;}
struct AppAiAgent:mooncake::AppAbility {};
namespace companion {
 enum class AudioOwner{None,Stock};
 AudioOwner CurrentAudioOwner(){return audio_busy?AudioOwner::Stock:AudioOwner::None;}
 bool NetworkWasStarted(){return true;}
}
namespace network_setup {bool NetworkWasStarted(){return true;}}
struct WifiManager {static WifiManager& GetInstance(){static WifiManager w;return w;}bool IsInitialized(){return true;}};
namespace runtime_lifecycle {bool PrepareVendorNetwork(){++preparations;return network_ready;}}
namespace stock_shell {void RequestSettings(){settings_requested=true;}}
namespace stock_agent {std::atomic<bool> start_requested{false};
''' + body(stock, 'lv_obj_t* Label(') + '\n' + body(stock, 'class StockAgentApp :') + r''';
}
int main(){using stock_agent::StockAgentApp;
 for(unsigned cycle=0;cycle<20;++cycle){
  configured=true;network_ready=true;audio_busy=false;stock_agent::start_requested=false;
  StockAgentApp app;app.onOpen();
  assert(!stock_agent::start_requested); // construction alone never starts audio/call
  assert(LiveObjects()==0 && "configured entry must not construct a temporary setup/retry gate");
  app.onRunning();assert(stock_agent::start_requested&&preparations==cycle+1);
  app.onClose();assert(!LiveObjects());
 }
 stock_agent::start_requested=false;configured=false;
 {StockAgentApp app;app.onOpen();assert(Button("Settings")&&Button("Back"));
  app.onRunning();assert(!stock_agent::start_requested);
  Click(Button("Settings"));app.onRunning();assert(settings_requested&&app.closed&&!stock_agent::start_requested);
  app.onClose();assert(!LiveObjects());}
 configured=true;settings_requested=false;
 {StockAgentApp app;app.onOpen();configured=false;app.onRunning();
  assert(!stock_agent::start_requested&&Button("Settings")&&Button("Back"));
  app.onClose();assert(!LiveObjects());}
 configured=true;network_ready=false;stock_agent::start_requested=false;
 {StockAgentApp app;app.onOpen();app.onRunning();assert(!stock_agent::start_requested);
  for(unsigned i=0;i<3;++i)app.onRunning();
  assert(Button("Back"));Click(Button("Back"));app.onRunning();assert(app.closed);
  network_ready=true;app.onRunning();assert(!stock_agent::start_requested);app.onClose();}
 network_ready=false;
 {StockAgentApp app;app.onOpen();app.onRunning();assert(!stock_agent::start_requested);
  network_ready=true;app.onRunning();assert(stock_agent::start_requested);app.onClose();}
 stock_agent::start_requested=false;audio_busy=true;
 {StockAgentApp app;app.onOpen();app.onRunning();assert(!stock_agent::start_requested);
  assert(Button("Retry"));audio_busy=false;app.onRunning();assert(!stock_agent::start_requested);
  Click(Button("Retry"));app.onRunning();assert(stock_agent::start_requested);app.onClose();}
 assert(!LiveObjects());puts("configured direct entry, setup, ownership wait, cancellation and retry passed");
}
'''

joint = (main / 'apps/stock_shell/feature_apps.cc').read_text()
mira = common + r'''
namespace view {std::function<void()> home;template<class F>void create_home_indicator(F f,unsigned,unsigned,lv_obj_t* p,bool show){assert(p==lv_layer_top()&&!show);home=f;} }
namespace companion {
 struct FeatureUiAction{};struct PublicStatus {template<class F>void Set(void*,F){}};
 PublicStatus& PublicStatusOwner(){static PublicStatus s;return s;}
 bool CompanionCharacterVisible(){return false;}
 void OpenView(lv_obj_t* parent){
  LvglLockGuard lock;auto label=lv_label_create(parent);lv_label_set_text(label,"Connect");
 }
}
struct Runtime {bool CloseFeatures(){return true;}};
Runtime& State(){static Runtime s;return s;}
class JointApp:public mooncake::AppAbility {
public:
 bool closing_=false,controls_visible_=false,ready_ui_=false;
 unsigned rendered_epoch_=0;bool usb_pending_=false,usb_active_=false,usb_return_=false;
 std::mutex action_mutex_;std::optional<companion::FeatureUiAction> action_;
 std::string local_notice_,scenes_result_;std::vector<int> scenes_;
 lv_obj_t *frame_=nullptr,*root_=nullptr,*native_root_=nullptr,*native_controls_=nullptr,*native_actions_=nullptr;
 bool PublicStatusIdle(){return true;}
 // Child rendering and external runtime boundaries remain inert; the production
 // onOpen and ShowControls own the visibility contract under test.
 void Render(){LvglLockGuard lock;auto label=lv_label_create(root_);lv_label_set_text(label,"Pair scopes");}
 void RenderNativeControls(){LvglLockGuard lock;auto label=lv_label_create(native_controls_);lv_label_set_text(label,"Back");}
''' + body(joint, 'void ShowControls(bool visible)') + '\n' + body(joint, 'void onOpen()override') + r'''
};
int main(){
 screen.w=320;screen.h=240;
 for(unsigned cycle=0;cycle<20;++cycle){
  JointApp app;unsigned partial_frames=0,final_frames=0;
  on_unlock=[&]{
   if(app.frame_&&Visible(app.frame_)){
    if(!Text("Back")||Text("Pair scopes"))++partial_frames;else ++final_frames;
   }
  };
  app.onOpen();on_unlock={};
  assert(!partial_frames&&"Mira must build offscreen until its final companion mode is ready");
  assert(final_frames&&Text("Back")&&!Text("Pair scopes"));
  assert(Visible(app.native_root_)&&!Visible(app.root_));
  app.ShowControls(true);assert(Text("Pair scopes")&&!Text("Connect"));
  app.ShowControls(false);assert(!Text("Pair scopes")&&Text("Connect"));
  app.usb_pending_=true;view::home();assert(app.closing_&&!app.usb_pending_);
  view::home={};lv_obj_delete(app.frame_);assert(!LiveObjects());
 }
 puts("20 Mira constructions published only final companion mode at lock boundaries");
}
'''
mira = mira.replace('namespace companion {\n struct FeatureUiAction', '''namespace stock_ui {void Page(lv_obj_t*){}}
namespace companion {
 struct FeatureUiAction''')

controls = common + r'''
enum State {kDeviceStateWifiConfiguring,kDeviceStateActivating,kDeviceStateIdle,
 kDeviceStateConnecting,kDeviceStateListening,kDeviceStateSpeaking,kDeviceStateAudioTesting,kDeviceStateFatalError};
State state=kDeviceStateIdle;bool home_requested=false;unsigned stops=0;
struct Application {static Application& GetInstance(){static Application a;return a;}State GetDeviceState(){return state;}};
namespace runtime_lifecycle {bool HomeRequested(){return home_requested;}}
namespace view {
 std::function<void()> home;std::function<const char*()> status;
 void create_home_indicator(std::function<void()> stop,unsigned,unsigned,lv_obj_t* parent,bool show,std::function<const char*()> text){
  assert(parent==lv_layer_top()&&!show);home=stop;status=text;
 }
}
namespace stock_agent {std::atomic<bool> slow_stop{false};
void ReturnHome(){home_requested=true;++stops;}
''' + body(stock, 'const char* SessionText()') + body(stock, 'void SessionControls()') + r'''
}
int main(){
 for(unsigned cycle=0;cycle<20;++cycle){
  home_requested=false;stock_agent::SessionControls();
  assert(!LiveObjects()&&"the companion session must not paint a persistent footer");
  assert(std::string(view::status())=="Stackchan - wake word on");
  view::home();assert(stops==cycle+1&&home_requested);
  stock_agent::slow_stop=true;assert(std::string(view::status())=="Stop pending; ownership retained");
 }
 puts("20 session entries retained swipe Home, accurate transient status and no persistent footer");
}
'''

cases = {'stackchan': stackchan, 'mira': mira, 'controls': controls}
for name, source in cases.items():
    if args.case and args.case != name:
        continue
    with tempfile.TemporaryDirectory(prefix='stackchan-entry-'+name+'-') as temporary:
        tmp = Path(temporary)
        (tmp / 'test.cpp').write_text(source)
        (tmp / 'lvgl.h').write_text('#include "companion_ui_fixture.hpp"\n')
        subprocess.run(['clang++', '-std=c++17', '-O1', '-g',
                        '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                        '-I'+str(tmp), '-I'+str(Path(__file__).parent),
                        '-I'+str(main), str(tmp/'test.cpp'), '-o', str(tmp/'test')], check=True)
        subprocess.run([str(tmp/'test')], check=True)
