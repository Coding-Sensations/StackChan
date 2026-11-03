#include <build_profiles/features.hpp>
#include "product_controls.h"
#if STACKCHAN_BUILD_MUSIC
#include "midi_binding.hpp"
#include "controls_session.hpp"
#include <apps/local_tools/local_audio.h>
#endif
#include <apps/common/common.h>
#include <assets/assets.h>
#include <board.h>
#include <lvgl.h>
#include <atomic>
#include <cstdio>
namespace usb_composite_v2 {
#if STACKCHAN_BUILD_MUSIC
namespace {
MidiBinding midi;
bool Note(uint8_t c,uint8_t n,uint8_t v){return midi.Receive(local_tools_ui::AudioTools(),c,n,v);}
void Silence(){midi.Silence(local_tools_ui::AudioTools());}
bool ArmMidi(bool arm){if(arm)midi.Arm();else midi.Disarm(local_tools_ui::AudioTools());return true;}
}
Integration BindProductCallbacks(Integration binding){binding.local_note=Note;binding.local_silence=Silence;binding.local_arm=ArmMidi;return binding;}
#endif
class ControlsApp:public mooncake::AppAbility {
 lv_obj_t* root_=nullptr;lv_obj_t* status_=nullptr;std::atomic<int> action_{0};bool closing_=false,release_pending_=false,keyboard_owned_=false;
#if STACKCHAN_BUILD_MUSIC
 ControlsSession session_;
#endif
#if STACKCHAN_BUILD_MUSIC
 struct MidiIO {TinyUsbBridge* bridge;bool Ready(){return bridge&&bridge->State()==Connection::Ready;}bool Connected(){return bridge&&bridge->State()!=Connection::Stopped&&bridge->State()!=Connection::Detached&&bridge->State()!=Connection::Suspended;}bool PollStopped(){return local_tools_ui::AudioTools().PollStopped();}bool Start(){return local_tools_ui::AudioTools().Start([]{return Board::GetInstance().GetAudioCodec();},local_tools_ui::AudioMode::Synth,120,69,0);}void RequestStop(){local_tools_ui::AudioTools().RequestStop();}void Disarm(){if(bridge)bridge->ArmLocalMidi(false);else midi.Disarm(local_tools_ui::AudioTools());}bool Arm(){return bridge&&bridge->ArmLocalMidi(true);}bool Running(){return local_tools_ui::AudioTools().Snapshot().running;}bool Fault(){return local_tools_ui::AudioTools().Snapshot().fault!=local_tools_ui::AudioFault::None;}};
#endif
#if STACKCHAN_BUILD_MUSIC
 const char* notice_="Tap to arm keyboard or open MIDI";
#else
 const char* notice_="Tap to arm keyboard";
#endif
 void Button(const char* name,int x,int y,int width,int id){auto o=lv_button_create(root_);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,width,32);lv_obj_set_user_data(o,this);auto l=lv_label_create(o);lv_label_set_text(l,name);lv_obj_center(l);lv_obj_add_event_cb(o,[](lv_event_t* e){auto* self=static_cast<ControlsApp*>(lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e))));self->action_=int(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));},LV_EVENT_CLICKED,reinterpret_cast<void*>(intptr_t(id)));}
#if STACKCHAN_BUILD_MUSIC
 void Stop(){MidiIO io{CompositeBridge()};session_.Stop(io);if(keyboard_owned_&&io.bridge){io.bridge->ReleaseKeyboard();keyboard_owned_=false;release_pending_=false;}}
 void Render(){char text[384];auto s=local_tools_ui::AudioTools().Snapshot();if(!CompositeBridge())std::snprintf(text,sizeof(text),"USB controls unavailable\nComposite USB was not started.\nKeyboard and MIDI are inactive.\nNo key or MIDI command was sent.\nComposite firmware integration\nand Mac enumeration test pending.");else if(session_.Owned()&&s.cleanup_failed)std::snprintf(text,sizeof(text),"Audio closure failed; tap Stop to retry");else if(session_.Owned()&&s.stopping)std::snprintf(text,sizeof(text),"Waiting for owned audio closure");else if(session_.Owned()&&s.starting)std::snprintf(text,sizeof(text),"Opening exclusive local synth");else if(session_.Owned()&&s.fault!=local_tools_ui::AudioFault::None)std::snprintf(text,sizeof(text),"Audio failed/busy; stop before retry");else {const auto state=CompositeBridge()->State();const char* connection=state==Connection::Ready?"connected / ready":state==Connection::Settling?"connected / release pending":state==Connection::Suspended?"suspended":state==Connection::Detached?"waiting for USB host":"stopped";std::snprintf(text,sizeof(text),"USB %s\n%s\nMIDI synth %s / rejected %u",connection,notice_,session_.Armed()&&s.running?"listening":"stopped",midi.Rejected());}LvglLockGuard lock;lv_label_set_text(status_,text);}
#else
 void Stop(){auto* b=CompositeBridge();if(keyboard_owned_&&b){b->ReleaseKeyboard();keyboard_owned_=false;release_pending_=false;}}
 void Render(){auto* b=CompositeBridge();char text[256];if(!b)std::snprintf(text,sizeof(text),"USB controls unavailable\nComposite USB was not started.\nKeyboard is inactive.\nNo key command was sent.");else {const auto state=b->State();const char* connection=state==Connection::Ready?"connected / ready":state==Connection::Settling?"connected / release pending":state==Connection::Suspended?"suspended":state==Connection::Detached?"waiting for USB host":"stopped";std::snprintf(text,sizeof(text),"USB %s\n%s",connection,notice_);}LvglLockGuard lock;lv_label_set_text(status_,text);}
#endif
public:
 ControlsApp(){setAppInfo().name="USB CONTROLS";static auto icon=assets::get_image("icon_dance.bin");setAppInfo().icon=&icon;}
 void onOpen()override{LvglLockGuard lock;root_=lv_obj_create(lv_screen_active());lv_obj_set_size(root_,320,240);lv_obj_set_style_pad_all(root_,0,0);lv_obj_remove_flag(root_,LV_OBJ_FLAG_SCROLLABLE);Button("Close",240,0,72,99);if(CompositeBridge()){Button("Arm keyboard",8,38,150,1);Button("Space",164,38,148,2);Button("Release / disarm",8,76,150,3);
#if STACKCHAN_BUILD_MUSIC
 Button("Computer MIDI",8,114,150,4);
#endif
 Button("Stop",164,114,148,5);}status_=lv_label_create(root_);lv_obj_set_pos(status_,8,CompositeBridge()?158:40);lv_obj_set_width(status_,304);lv_label_set_text(status_,"USB controls unavailable - composite not started");closing_=release_pending_=keyboard_owned_=false;action_=0;}
#if STACKCHAN_BUILD_MUSIC
 void onRunning()override{auto* b=CompositeBridge();MidiIO io{b};session_.Tick(io);if(b){b->Service();if(release_pending_){b->ReleaseKeyboard();release_pending_=false;}}const int a=action_.exchange(0);if(a==99){closing_=true;Stop();}else if(a==5){Stop();notice_="Stop requested";}else if(b&&a==1){const bool armed=b->ArmKeyboard(true);if(armed)keyboard_owned_=true;notice_=armed?"Keyboard armed by physical tap":"Wait for mounted/released USB";}else if(b&&a==2){std::array<uint8_t,6> keys{0x2c,0,0,0,0,0};if(keyboard_owned_&&b->Keys(0,keys)){b->Service();release_pending_=true;notice_="Space queued; release requested next tick";}else notice_="Arm keyboard before sending";}else if(b&&a==3){if(keyboard_owned_)b->ReleaseKeyboard();keyboard_owned_=false;notice_="Keyboard release requested; disarmed";}else if(b&&a==4){notice_=session_.Start(io)?"Opening local MIDI synth":"USB not ready or audio busy";}if(b)b->Service();if(closing_&&!session_.Owned()&&(!b||b->State()!=Connection::Settling)){close();return;}Render();}
#else
 void onRunning()override{auto* b=CompositeBridge();if(b){b->Service();if(release_pending_){b->ReleaseKeyboard();release_pending_=false;}}const int a=action_.exchange(0);if(a==99){closing_=true;Stop();}else if(a==5){Stop();notice_="Stop requested";}else if(b&&a==1){const bool armed=b->ArmKeyboard(true);if(armed)keyboard_owned_=true;notice_=armed?"Keyboard armed by physical tap":"Wait for mounted/released USB";}else if(b&&a==2){std::array<uint8_t,6> keys{0x2c,0,0,0,0,0};if(keyboard_owned_&&b->Keys(0,keys)){b->Service();release_pending_=true;notice_="Space queued; release requested next tick";}else notice_="Arm keyboard before sending";}else if(b&&a==3){if(keyboard_owned_)b->ReleaseKeyboard();keyboard_owned_=false;notice_="Keyboard release requested; disarmed";}if(b)b->Service();if(closing_&&(!b||b->State()!=Connection::Settling)){close();return;}Render();}
#endif
 void onClose()override{Stop();LvglLockGuard lock;if(root_)lv_obj_delete(root_);root_=status_=nullptr;action_=0;}
};
std::unique_ptr<mooncake::AppAbility> MakeControlsApp(){return std::make_unique<ControlsApp>();}
}
