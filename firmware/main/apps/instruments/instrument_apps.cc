#include "instrument_apps.h"
#include "synth_view.hpp"
#include <apps/common/tool_visual.hpp>
#include "instrument_projects.hpp"
#include "instrument_sd.h"
#include <apps/local_tools/local_audio.h>
#include <apps/common/common.h>
#include <apps/stock_shell/ir_storage_binding.hpp>
#include <assets/assets.h>
#include <board.h>
#include <lvgl.h>
#include <atomic>
#include <cstdio>
#include <memory>
#include <vector>
namespace instrument_ui {
using namespace stackchan_instruments;
using local_tools_ui::AudioTools;
namespace {
enum class StorageJob { Save, Load, Bank };
struct StorageResult { bool success=false;Project project;std::shared_ptr<local_tools_ui::PreparedInstrumentBank> bank;std::string notice="Storage stopped"; };
bool Store(StorageJob job,unsigned slot,Project project,const std::shared_ptr<StorageResult>& result){
 if(AudioTools().StopRequested()){result->notice="Storage stopped";return false;}
 stock_tools::InstrumentProjectStore store(stock_tools::InstrumentProjectFiles());
 if(job!=StorageJob::Bank&&!store.Load()){result->notice=stock_tools::IrStoreMessage(store.Status());return false;}
 char name[16];std::snprintf(name,sizeof(name),"Project%u",slot);
 if(job==StorageJob::Save){result->success=store.Save(name,project,true);result->notice=result->success?"Project saved on SD":std::string("Project save failed: ")+stock_tools::IrStoreMessage(store.Status());return result->success;}
 if(job==StorageJob::Load){auto saved=store.Library().Find(name);if(!saved){result->notice="No saved project in this slot";return false;}project=*saved;}
 auto& files=stock_tools::InstrumentProjectFiles();const auto identity=files.Identity();if(identity.empty()){result->notice="SD identity unavailable";return false;}
 auto bank=std::make_shared<local_tools_ui::PreparedInstrumentBank>();bank->pad_ids=project.pad_ids;
 for(unsigned pad=0;pad<4;++pad){if(AudioTools().StopRequested()){result->notice="Storage stopped";return false;}if(!stock_tools::ReadStackChanInstrumentSample(project.pad_ids[pad],bank->wavs[pad])){result->notice="SD sample read failed; check sample pack";return false;}WavView view;if(!ParseWav(reinterpret_cast<const uint8_t*>(bank->wavs[pad].data()),bank->wavs[pad].size(),view)){result->notice="Sample must be mono PCM16 24 kHz";return false;}}
 if(files.Identity()!=identity){result->notice="SD changed while loading bank";return false;}
 result->project=project;result->bank=std::move(bank);result->success=true;result->notice=job==StorageJob::Load?"Project and SD bank loaded; tap Play":"SD bank loaded; tap Play";return true;
}
}
class InstrumentApp:public mooncake::AppAbility {
 struct Callback {InstrumentApp* app;int action;bool note=false,chord=false;};
 Kind kind_;Project project_=DefaultProject();std::shared_ptr<const local_tools_ui::PreparedInstrumentBank> bank_;
 lv_obj_t* root_=nullptr;lv_obj_t* status_=nullptr;std::array<lv_obj_t*,16> steps_{};std::array<lv_obj_t*,4> pads_{};std::array<lv_point_precise_t,65> waveform_points_{};std::vector<std::unique_ptr<Callback>> callbacks_;
 std::atomic<int> action_{0};std::atomic<bool> owned_{false},command_failed_{false};
 bool closing_=false,projects_=false,sustain_=false,job_running_=false,pending_storage_=false;StorageJob job_=StorageJob::Bank;unsigned slot_=1,pad_=0,base_note_=60;
 std::shared_ptr<StorageResult> storage_;std::string notice_="Stopped; tap Play. USB keyboard host not enabled";
 const char* Name()const{return kind_==Kind::Synth?"SYNTH":kind_==Kind::Groovebox?"GROOVEBOX":"PAD SAMPLER";}
 bool Send(Command command){if(!owned_)return false;const bool ok=AudioTools().InstrumentCommand(command);if(!ok)command_failed_=true;return ok;}
 lv_obj_t* Button(const char* text,int x,int y,int width,int height,int action,bool note=false,bool chord=false){
  auto button=stock_ui::Button(root_,text,x,y,width,height);
  auto context=std::make_unique<Callback>();*context={this,action,note,chord};
  lv_obj_add_event_cb(button,[](lv_event_t* e){auto c=static_cast<Callback*>(lv_event_get_user_data(e));auto code=lv_event_get_code(e);if(c->note||c->chord){if(code!=LV_EVENT_PRESSED&&code!=LV_EVENT_RELEASED&&code!=LV_EVENT_PRESS_LOST)return;const bool down=code==LV_EVENT_PRESSED;if(c->app->kind_==Kind::Synth)lv_obj_set_style_outline_width(lv_event_get_target_obj(e),down?2:0,0);Command command;command.kind=down?CommandKind::NoteOn:CommandKind::NoteOff;command.a=0;command.c=100;if(c->chord){for(unsigned interval:std::array<unsigned,4>{0,4,7,12}){command.b=unsigned(c->action)+interval;c->app->Send(command);}}else{command.b=unsigned(c->action);c->app->Send(command);}return;}if(code==LV_EVENT_CLICKED)c->app->action_=c->action;},LV_EVENT_ALL,context.get());callbacks_.push_back(std::move(context));return button;
 }
 void Label(const char* text,int x,int y,int width){stock_ui::Text(root_,text,x,y,width);}
 void Panic(){Command c;c.kind=CommandKind::Panic;Send(c);sustain_=false;}
 void SynthPage(){
  lv_obj_set_style_bg_color(root_,lv_color_hex(0x101820),0);lv_obj_set_style_text_color(root_,lv_color_hex(0xe6f0f4),0);
  Label("SYNTH",8,8,66);Button("Play",78,2,46,30,1);Button("Stop",128,2,42,30,2);Button("Project",174,2,68,30,90);Button("Close",246,2,66,30,99);
  Label("OSC / shape preview",8,36,190);
  const auto preview=synth_view::Shape(project_.preset.waveform);for(unsigned i=0;i<preview.size();++i)waveform_points_[i]={lv_value_precise_t(preview[i].x),lv_value_precise_t(preview[i].y)};
  auto wave=lv_line_create(root_);lv_obj_set_pos(wave,synth_view::Preview.x,synth_view::Preview.y);lv_obj_set_size(wave,synth_view::Preview.w,synth_view::Preview.h);lv_line_set_points(wave,waveform_points_.data(),waveform_points_.size());lv_obj_set_style_line_color(wave,lv_color_hex(0x58dfcc),0);lv_obj_set_style_line_width(wave,2,0);
  char text[24];std::snprintf(text,sizeof(text),"LEVEL %u%%",unsigned(project_.preset.gain)*100/8192);Label(text,208,40,104);
  Button("-",208,72,48,30,14);Button("+",264,72,48,30,15);
  Button(project_.preset.waveform==Waveform::Triangle?"Triangle":"Square",8,108,92,32,12);Button("-",108,108,32,32,10);std::snprintf(text,sizeof(text),"C%u",base_note_/12-1);Label(text,144,116,28);Button("+",172,108,32,32,11);Button(sustain_?"Hold ON":"Hold OFF",208,108,104,32,13);
  const char* names[]={"C","D","E","F","G","A","B","Cmaj"};
  for(unsigned i=0;i<8;++i){const auto r=synth_view::White(i);auto key=Button(names[i],r.x,r.y,r.w,r.h,int(base_note_+synth_view::WhiteNotes[i]),i!=7,i==7);if(i==7){callbacks_.back()->action=int(base_note_);}lv_obj_set_style_bg_color(key,lv_color_hex(i==7?0x9be8d9:0xe6f0f4),0);lv_obj_set_style_text_color(key,lv_color_hex(0x101820),0);lv_obj_set_style_text_font(key,&lv_font_montserrat_14,0);lv_obj_set_style_radius(key,2,0);lv_obj_set_style_pad_all(key,0,0);lv_obj_set_style_outline_color(key,lv_color_hex(0xffb95c),0);auto label=lv_obj_get_child(key,0);lv_obj_align(label,LV_ALIGN_BOTTOM_MID,0,-5);}
  const char* sharps[]={"C#","D#","F#","G#","A#"};for(unsigned i=0;i<5;++i){const auto r=synth_view::Black(i);auto key=Button(sharps[i],r.x,r.y,r.w,r.h,int(base_note_+synth_view::BlackNotes[i]),true);lv_obj_set_style_bg_color(key,lv_color_hex(0x182732),0);lv_obj_set_style_text_color(key,lv_color_hex(0xe6f0f4),0);lv_obj_set_style_text_font(key,&lv_font_montserrat_14,0);lv_obj_set_style_radius(key,2,0);lv_obj_set_style_pad_all(key,0,0);lv_obj_set_style_outline_color(key,lv_color_hex(0xffb95c),0);}
  status_=lv_label_create(root_);lv_obj_set_pos(status_,8,220);lv_obj_set_width(status_,304);lv_label_set_long_mode(status_,LV_LABEL_LONG_SCROLL_CIRCULAR);lv_label_set_text(status_,notice_.c_str());
 }
 const char* PadName(unsigned pad)const{static const char* names[]={"Kick","Snare","Hat","Clap","Tom","Rim","Shaker","Cymbal"};return names[project_.pad_ids[pad]];}
 void Page(){
  LvglLockGuard lock;lv_obj_clean(root_);callbacks_.clear();steps_.fill(nullptr);pads_.fill(nullptr);status_=nullptr;stock_ui::Page(root_);
  if(kind_==Kind::Synth&&!projects_){SynthPage();return;}
  Label(Name(),8,8,164);Button(projects_?"Back":"Project",174,2,68,30,90);Button("Close",246,2,66,30,99);
  if(projects_){char text[32];std::snprintf(text,sizeof(text),"Project%u",slot_);Label(text,8,48,104);Button("Slot -",120,38,88,36,70);Button("Slot +",220,38,92,36,71);Button("Save SD",8,82,140,40,72);Button("Load SD",164,82,148,40,73);Button("Load SD bank",8,128,140,40,74);Button("Built-in drums",164,128,148,40,75);std::snprintf(text,sizeof(text),"Pad %u / sample %u",pad_+1,project_.pad_ids[pad_]);Label(text,8,184,180);Button("Pad",192,178,52,32,76);Button("ID +",252,178,60,32,77);}
  else{
   Button("Play",8,36,62,40,1);Button("Stop",76,36,62,40,2);
   if(kind_==Kind::Groovebox){Button("-",144,36,40,40,20);char text[40];std::snprintf(text,sizeof(text),"%u",project_.bpm);Label(text,196,48,56);Button("+",264,36,48,40,21);std::snprintf(text,sizeof(text),"%s pattern / tap a step",PadName(pad_));Label(text,8,80,304);
    for(unsigned i=0;i<16;++i){std::snprintf(text,sizeof(text),"%u",i+1);steps_[i]=Button(text,8+(i%8)*38,102+(i/8)*30,34,26,100+int(i));lv_obj_set_style_text_font(steps_[i],&lv_font_montserrat_14,0);stock_ui::Active(steps_[i],project_.steps[i].pad_mask&(1u<<pad_));lv_obj_set_style_outline_color(steps_[i],lv_color_hex(stock_ui::Warning),0);}
    for(unsigned i=0;i<4;++i){pads_[i]=Button(PadName(i),8+i*78,168,72,44,200+int(i));stock_ui::Active(pads_[i],i==pad_);}
   }else{Label(bank_?"SD bank / tap a drum":"Built-in drums / tap a drum",8,78,304);for(unsigned i=0;i<4;++i){pads_[i]=Button(PadName(i),8+(i%2)*156,102+(i/2)*56,148,50,200+int(i));stock_ui::Active(pads_[i],i==pad_);}}
  }
  status_=stock_ui::Status(root_,notice_.c_str());
 }
 void Render(){if(!status_)return;auto state=AudioTools().Snapshot();char text[192];
  if(job_running_)std::snprintf(text,sizeof(text),"SD operation; audio stopped");
  else if(pending_storage_||closing_)std::snprintf(text,sizeof(text),state.cleanup_failed?"Audio cleanup failed; tap Stop to retry":"Waiting for audio closure");
  else if(command_failed_.exchange(false))notice_="Command rejected; release/Stop before retry";
  if(!job_running_&&!pending_storage_&&!closing_){if(!owned_&&(state.running||state.starting||state.stopping))std::snprintf(text,sizeof(text),"Audio busy in another app");else if(state.cleanup_failed)std::snprintf(text,sizeof(text),"Cleanup failed; tap Stop to retry");else if(owned_&&state.starting)std::snprintf(text,sizeof(text),"Preparing bank / opening codec");else if(owned_&&state.fault!=local_tools_ui::AudioFault::None)std::snprintf(text,sizeof(text),"Audio failed; tap Stop. %s",notice_.c_str());else if(owned_&&state.running&&kind_==Kind::Synth)std::snprintf(text,sizeof(text),"Playing | %u voices | reject %llu",state.synth_voices,static_cast<unsigned long long>(state.rejected));else if(owned_&&state.running)std::snprintf(text,sizeof(text),"%u BPM %s | %u synth/%u pads | reject %llu",project_.bpm,state.sequence_running?"playing":"ready",state.synth_voices,state.sample_voices,static_cast<unsigned long long>(state.rejected));else std::snprintf(text,sizeof(text),"%s",notice_.c_str());}
  LvglLockGuard lock;lv_label_set_text(status_,text);if(kind_==Kind::Groovebox&&!projects_)for(unsigned i=0;i<steps_.size();++i)if(steps_[i])lv_obj_set_style_outline_width(steps_[i],owned_&&state.running&&!state.stopping&&state.sequence_running&&state.sequence_step==int(i)?2:0,0);
 }
 void Stop(){if(owned_)AudioTools().RequestStop();}
 void Changed(){Panic();if(bank_&&bank_->pad_ids!=project_.pad_ids)bank_.reset();Command c;c.kind=CommandKind::Project;c.project=project_;Send(c);notice_=kind_==Kind::Synth?"Sound updated; play a key":"Pattern edited; tap Play";Page();}
public:
 explicit InstrumentApp(Kind kind):kind_(kind){notice_="Stopped; tap Play";setAppInfo().name=Name();static auto icon=assets::get_image("icon_dance.bin");setAppInfo().icon=&icon;}
 void onOpen()override{if(kind_==Kind::Synth)projects_=false;{LvglLockGuard lock;root_=lv_obj_create(lv_screen_active());lv_obj_set_size(root_,320,240);lv_obj_set_style_pad_all(root_,0,0);lv_obj_remove_flag(root_,LV_OBJ_FLAG_SCROLLABLE);}action_=0;closing_=false;if(owned_&&AudioTools().PollStopped())owned_=false;Page();}
 void onRunning()override{
  if(owned_&&!job_running_&&!pending_storage_){const auto state=AudioTools().Snapshot();if(!state.running&&!state.starting&&!state.stopping&&AudioTools().PollStopped())owned_=false;}
  const int action=action_.exchange(0);
  if(action==99){closing_=true;pending_storage_=false;Stop();}
  if(action==2)Stop();
  if(job_running_&&AudioTools().PollStopped()){owned_=false;job_running_=false;if(storage_){notice_=storage_->notice;if(storage_->success&&job_!=StorageJob::Save){project_=storage_->project;bank_=storage_->bank;}storage_.reset();Page();}}
  if(closing_){if(!owned_||AudioTools().PollStopped()){owned_=false;close();return;}Render();return;}
  if(pending_storage_&&!job_running_){if(!owned_||AudioTools().PollStopped()){owned_=false;pending_storage_=false;storage_=std::make_shared<StorageResult>();auto result=storage_;const auto job=job_;const auto project=project_;const unsigned slot=slot_;if(AudioTools().StartStorage([job,slot,project,result]{return Store(job,slot,project,result);})){owned_=true;job_running_=true;}else{notice_="Audio busy; SD operation not started";storage_.reset();}}Render();return;}
  if(job_running_){Render();return;}
  if(action==90){Panic();projects_=!projects_;Page();}
  else if(action==1){if(!owned_||AudioTools().PollStopped()){owned_=AudioTools().StartInstruments([]{return Board::GetInstance().GetAudioCodec();},project_,bank_);notice_=owned_?"Play requested":"Play unavailable; stop other audio / load SD bank";}if(owned_){Command held;held.kind=CommandKind::Sustain;held.b=sustain_;Send(held);}if(owned_&&kind_==Kind::Groovebox){Command c;c.kind=CommandKind::SequenceStart;Send(c);}}
  else if(action==10||action==11){Panic();base_note_=synth_view::Octave(base_note_,action==11?1:-1);Page();}
  else if(action==12){project_.preset.waveform=project_.preset.waveform==Waveform::Triangle?Waveform::Square:Waveform::Triangle;Changed();}
  else if(action==13){sustain_=!sustain_;Command c;c.kind=CommandKind::Sustain;c.b=sustain_;Send(c);Page();}
  else if(action==14||action==15){project_.preset.gain=synth_view::Level(project_.preset.gain,action==15?1:-1);Changed();}
  else if(action==20||action==21){project_.bpm=uint16_t(std::clamp(int(project_.bpm)+(action==21?5:-5),20,300));Changed();}
  else if(action>=100&&action<116){project_.steps[action-100].pad_mask^=uint8_t(1u<<pad_);Changed();}
  else if(action>=200&&action<204){pad_=unsigned(action-200);Command c;c.kind=CommandKind::Pad;c.a=pad_;c.b=100;Send(c);Page();}
  else if(action==70||action==71){slot_=unsigned(std::clamp(int(slot_)+(action==71?1:-1),1,8));Page();}
  else if(action>=72&&action<=74){job_=action==72?StorageJob::Save:action==73?StorageJob::Load:StorageJob::Bank;pending_storage_=true;Stop();}
  else if(action==75){Stop();bank_.reset();project_.pad_ids={0,1,2,3};notice_="Built-in drums selected; Stop, then Play";Page();}
  else if(action==76){pad_=(pad_+1)%4;Page();}
  else if(action==77){Stop();project_.pad_ids[pad_]=uint8_t((project_.pad_ids[pad_]+1)%8);bank_.reset();notice_="Sample ID changed; load SD bank";Page();}
  Render();
 }
 void onClose()override{Stop();LvglLockGuard lock;if(root_)lv_obj_delete(root_);root_=status_=nullptr;callbacks_.clear();action_=0;}
};
std::unique_ptr<mooncake::AppAbility> MakeApp(Kind kind){return std::make_unique<InstrumentApp>(kind);}
}
