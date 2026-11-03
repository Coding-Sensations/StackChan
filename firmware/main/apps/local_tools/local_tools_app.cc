#include <build_profiles/features.hpp>
#include "local_tools_app.h"
#include "local_audio.h"
#include "audio_view.hpp"
#include <apps/common/tool_visual.hpp>
#include <apps/common/common.h>
#include <assets/assets.h>
#include <board.h>
#include <esp_timer.h>
#include <lvgl.h>
#include <atomic>
#include <memory>
#include <vector>
#include <cstdio>
namespace local_tools_ui {
class AudioToolsApp final:public mooncake::AppAbility {
 using Tool=audio_view::Tool;
 struct Callback{AudioToolsApp* owner;int action;};
 std::vector<std::unique_ptr<Callback>> callbacks_;
 std::atomic<int> action_{0};
 Tool tool_=Tool::Home,next_=Tool::Home;
 bool owned_=false,pending_=false,close_pending_=false,reference_=false;
 unsigned note_=60,bpm_=120,string_=0;
 uint64_t render_at_=0,last_samples_=0,sample_wall_=0;
 bool sample_seen_=false;
 lv_obj_t *root_=nullptr,*value_=nullptr,*info_=nullptr,*status_=nullptr,*meter_=nullptr,*canvas_=nullptr,*freeze_=nullptr;
 std::unique_ptr<stackchan_tools::SpectrogramCache> spectrum_{new stackchan_tools::SpectrogramCache};
 std::vector<uint16_t> canvas_pixels_;
 uint64_t spectrum_sequence_=UINT64_MAX;
 unsigned inspect_column_=95,inspect_band_=32;
 std::string notice_="Select a tool; audio stays stopped";
 static constexpr int PlotW=276,PlotH=112;
 static bool Capture(Tool t){return t==Tool::Guitar||t==Tool::Piano||t==Tool::Key||t==Tool::Spectrogram||t==Tool::Meter;}
 AudioMode Mode()const{
  if((tool_==Tool::Guitar||tool_==Tool::Piano)&&reference_)return AudioMode::PitchPipe;
  switch(tool_){case Tool::PitchPipe:return AudioMode::PitchPipe;case Tool::Fork:return AudioMode::Fork;case Tool::Metro:return AudioMode::Metro;case Tool::Key:return AudioMode::Key;case Tool::Spectrogram:return AudioMode::Spectrogram;case Tool::Meter:return AudioMode::Meter;default:return AudioMode::Pitch;}
 }
 unsigned Target()const{return tool_==Tool::Guitar?audio_view::GuitarNotes[string_]:note_;}
 lv_obj_t* Text(const char* text,int x,int y,int w,bool muted=false){return stock_ui::Text(root_,text,x,y,w,muted);}
 lv_obj_t* Button(const char* text,int x,int y,int w,int id,int h=40){auto b=stock_ui::Button(root_,text,x,y,w,h);auto c=std::make_unique<Callback>();c->owner=this;c->action=id;lv_obj_add_event_cb(b,[](lv_event_t* e){auto c=static_cast<Callback*>(lv_event_get_user_data(e));c->owner->action_=c->action;},LV_EVENT_CLICKED,c.get());callbacks_.push_back(std::move(c));return b;}
 void Build(){
  LvglLockGuard lock;lv_obj_clean(root_);callbacks_.clear();value_=info_=status_=meter_=canvas_=freeze_=nullptr;canvas_pixels_.clear();stock_ui::Page(root_);
  Text(audio_view::Name(tool_),8,12,158);Button(tool_==Tool::Home?"Close":"Back",250,2,62,99);
  if(tool_==Tool::Home){Button("Stop",170,2,72,21);const Tool tools[]={Tool::PitchPipe,Tool::Fork,Tool::Guitar,Tool::Piano,Tool::Key,Tool::Spectrogram,Tool::Meter,Tool::Metro};for(unsigned i=0;i<8;++i){auto p=audio_view::Card(i);Button(audio_view::Name(tools[i]),p.x,p.y,p.w,10+int(tools[i]),p.h);}status_=stock_ui::Status(root_,"",220);return;}
  if(tool_==Tool::Spectrogram){
   Text("12k",2,52,32,true);Text("1k",2,96,32,true);Text("100",2,138,32,true);Text("23Hz",2,152,34,true);
   canvas_pixels_.assign(PlotW*PlotH,0);canvas_=lv_canvas_create(root_);lv_obj_set_pos(canvas_,36,52);lv_canvas_set_buffer(canvas_,canvas_pixels_.data(),PlotW,PlotH,LV_COLOR_FORMAT_RGB565);
   lv_obj_add_flag(canvas_,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(canvas_,[](lv_event_t* e){auto self=static_cast<AudioToolsApp*>(lv_event_get_user_data(e));auto indev=lv_indev_active();if(!indev)return;lv_point_t p;lv_indev_get_point(indev,&p);lv_area_t a;lv_obj_get_coords(self->canvas_,&a);unsigned x=unsigned(std::clamp(int(p.x-a.x1),0,PlotW-1)),y=unsigned(std::clamp(int(p.y-a.y1),0,PlotH-1));self->action_=2000+int(x*96/PlotW)*64+int(63-y*64/PlotH);},LV_EVENT_CLICKED,this);
   info_=Text("Waiting for microphone samples",8,168,304,true);lv_obj_set_style_text_font(info_,&lv_font_montserrat_14,0);lv_label_set_long_mode(info_,LV_LABEL_LONG_SCROLL_CIRCULAR);
   Button("Start",8,192,88,20);Button("Stop",104,192,88,21);freeze_=Button("Freeze",200,192,112,22);status_=stock_ui::Status(root_,"",32);lv_obj_set_width(status_,156);lv_obj_set_style_text_font(status_,&lv_font_montserrat_14,0);spectrum_sequence_=UINT64_MAX;return;
  }
  if(tool_==Tool::PitchPipe){
   value_=Text("--",8,48,304);lv_obj_set_style_text_font(value_,&lv_font_montserrat_20,0);
   Button("Oct -",8,78,64,40,32);Button("Oct +",248,78,64,41,32);info_=Text("Reference tone",80,82,160,true);lv_obj_set_style_text_font(info_,&lv_font_montserrat_14,0);
   for(unsigned i=0;i<12;++i){auto text=audio_view::Note(int(note_/12)*12+i);auto b=Button(text.c_str(),8+int(i%6)*52,112+int(i/6)*40,44,100+i,36);stock_ui::Active(b,i==note_%12);}
  }else {
   stock_ui::Card(root_,8,48,304,90);value_=Text("--",16,52,288);lv_obj_set_style_text_font(value_,&lv_font_montserrat_20,0);info_=Text("",16,80,288,true);lv_obj_set_style_text_font(info_,&lv_font_montserrat_14,0);lv_label_set_long_mode(info_,LV_LABEL_LONG_WRAP);
   if(tool_==Tool::Guitar){for(unsigned i=0;i<6;++i){auto text=audio_view::Note(audio_view::GuitarNotes[i]);auto b=Button(text.c_str(),8+int(i)*52,144,44,60+i,40);stock_ui::Active(b,i==string_);}Button(reference_?"Tone":"Mic",170,2,72,92);}
   else if(tool_==Tool::Piano){auto down=Button("Note -",8,144,148,40);auto up=Button("Note +",164,144,148,41);if(!reference_){lv_obj_add_state(down,LV_STATE_DISABLED);lv_obj_add_state(up,LV_STATE_DISABLED);}Button(reference_?"Tone":"Mic",170,2,72,92);}
   else if(tool_==Tool::Metro){Button("BPM -",8,144,148,40);Button("BPM +",164,144,148,41);}
   if(tool_==Tool::Guitar||tool_==Tool::Piano||tool_==Tool::Meter){meter_=lv_bar_create(root_);lv_obj_set_pos(meter_,16,120);lv_obj_set_size(meter_,288,10);lv_bar_set_range(meter_,0,100);lv_obj_set_style_bg_color(meter_,lv_color_hex(stock_ui::Panel),0);lv_obj_set_style_bg_color(meter_,lv_color_hex(stock_ui::Accent),LV_PART_INDICATOR);}
  }
  Button("Start",8,192,148,20);Button("Stop",164,192,148,21);status_=stock_ui::Status(root_,"",32);lv_obj_set_width(status_,156);lv_obj_set_style_text_font(status_,&lv_font_montserrat_14,0);
 }
 void Observe(const AudioSnapshot& s,uint64_t now){if(s.capture_samples!=last_samples_){last_samples_=s.capture_samples;sample_wall_=now;sample_seen_=true;}if(sample_seen_&&now<sample_wall_)sample_seen_=false;}
 bool Fresh(const AudioSnapshot& s,uint64_t now)const{return owned_&&s.running&&!s.stopping&&!s.starting&&!s.cleanup_failed&&s.fault==AudioFault::None&&sample_seen_&&s.capture_samples>0&&now>=sample_wall_&&now-sample_wall_<=500;}
 void PaintSpectrum(){
  if(!canvas_||spectrum_sequence_==spectrum_->sequence)return;
  for(int y=0;y<PlotH;++y){unsigned band=63-unsigned(y)*64/PlotH;for(int x=0;x<PlotW;++x){unsigned col=unsigned(x)*96/PlotW;auto f=col>=96-spectrum_->count?spectrum_->Chronological(col-(96-spectrum_->count)):nullptr;canvas_pixels_[y*PlotW+x]=f?audio_view::Heat(f->intensity[band]):0;}}
  lv_obj_invalidate(canvas_);spectrum_sequence_=spectrum_->sequence;
 }
 void Render(uint64_t now){
  auto s=AudioTools().Snapshot();Observe(s,now);bool active=owned_&&s.running&&!s.stopping&&!s.starting&&!s.cleanup_failed&&s.fault==AudioFault::None;bool fresh=Fresh(s,now);char text[220],body[160];
  if(!owned_&&(s.running||s.starting||s.stopping||s.cleanup_failed))notice_="Audio belongs to another tool or Mira";
  else if(s.cleanup_failed)notice_="Cleanup failed; Stop retries owned closure";
  else if(s.starting)notice_="Starting audio";else if(s.stopping)notice_="Stopping; waiting for codec closure";
  else if(s.fault!=AudioFault::None){const char* errors[]={"","Audio busy","Codec format unavailable","Audio task failed","Codec enable failed","Mic read failed","Speaker write failed","Storage failed"};notice_=unsigned(s.fault)<8?errors[unsigned(s.fault)]:"Audio fault";}
  else if(active&&Capture(tool_)&&!reference_&&!fresh)notice_="Microphone stalled / waiting; no live result";
  else if(active&&fresh&&notice_=="Microphone stalled / waiting; no live result")notice_="Microphone on / local analysis";
  if(tool_==Tool::Spectrogram){AudioTools().CopySpectrogramSnapshot(*spectrum_);LvglLockGuard lock;PaintSpectrum();auto f=inspect_column_>=96-spectrum_->count?spectrum_->Chronological(inspect_column_-(96-spectrum_->count)):nullptr;auto latest=spectrum_->count?spectrum_->Chronological(spectrum_->count-1):nullptr;double age=f&&latest?double(latest->end_sample-f->end_sample)/24000:0;std::snprintf(body,sizeof(body),"%.0f..%.0f Hz / %s / age %.2fs",spectrum_->FrequencyEdge(inspect_band_),spectrum_->FrequencyEdge(inspect_band_+1),f?"tap / freeze to inspect":"no sample",age);if(f)std::snprintf(body,sizeof(body),"%.0f..%.0f Hz / %.1f dBFS / age %.2fs",spectrum_->FrequencyEdge(inspect_band_),spectrum_->FrequencyEdge(inspect_band_+1),spectrum_->Dbfs(f->intensity[inspect_band_]),age);lv_label_set_text(info_,body);auto label=lv_obj_get_child(freeze_,0);lv_label_set_text(label,spectrum_->frozen?"Resume":"Freeze");lv_obj_center(label);lv_label_set_text(status_,!owned_&&(s.running||s.starting||s.stopping||s.cleanup_failed)?"Other audio owner / cache only":s.cleanup_failed?"Closure failed; mic state unknown / Stop retries":s.stopping?"Stopping; awaiting codec closure":s.starting?"Starting microphone":s.fault!=AudioFault::None?notice_.c_str():active?(spectrum_->frozen?"Frozen view; microphone still on":fresh?"Mic on / frequency over time":"Waiting / stalled microphone"):!owned_&&(s.running||s.starting||s.stopping)?"Other audio owner / retained cache":"Stopped / retained cache");return;}
  if(tool_==Tool::Home){LvglLockGuard lock;lv_label_set_text(status_,notice_.c_str());return;}
  text[0]=body[0]=0;
  if(tool_==Tool::Fork){std::snprintf(text,sizeof(text),"A4 / 440 Hz");std::snprintf(body,sizeof(body),"Reference tone / speaker %s",active?"on":s.cleanup_failed||s.stopping?"unknown / closing":"not started");}
  else if(tool_==Tool::PitchPipe){std::snprintf(text,sizeof(text),"%s / %.1f Hz",audio_view::Note(note_).c_str(),audio_view::Frequency(note_));std::snprintf(body,sizeof(body),"%s",active?"Speaker on":s.cleanup_failed||s.stopping?"State unknown":"Speaker stopped");}
  else if(tool_==Tool::Metro){std::snprintf(text,sizeof(text),"%u BPM",bpm_);std::snprintf(body,sizeof(body),"Sample-clock clicks / %s",active?"playing":s.cleanup_failed||s.stopping?"closing / unknown":"stopped");}
  else if(tool_==Tool::Meter){std::snprintf(text,sizeof(text),fresh&&std::isfinite(s.dbfs)?"%.1f dBFS":fresh?"Silence":"--",s.dbfs);std::snprintf(body,sizeof(body),"Digital RMS / no calibrated SPL");}
  else if(tool_==Tool::Key){std::snprintf(text,sizeof(text),"%s",fresh&&s.tonic>=0?audio_view::Signature(s.tonic,s.minor).c_str():"Insufficient note material");std::snprintf(body,sizeof(body),"%u notes / %u classes / margin %.2f\nHeuristic; not a unique key proof",s.key_observations,s.key_pitch_classes,s.margin);}
  else {unsigned target=Target();bool pitch=fresh&&s.pitch_valid&&std::isfinite(s.hz)&&s.hz>0;double cents=tool_==Tool::Guitar?audio_view::Cents(s.hz,target):s.cents;if(reference_){std::snprintf(text,sizeof(text),"%s / %.1f Hz",audio_view::Note(target).c_str(),audio_view::Frequency(target));std::snprintf(body,sizeof(body),"Reference speaker %s",active?"on":s.cleanup_failed||s.stopping?"unknown / closing":"not started");}else if(pitch){std::snprintf(text,sizeof(text),"%s / %+.1f cents",audio_view::Note(s.midi_note).c_str(),cents);std::snprintf(body,sizeof(body),"%.1f Hz / correlation %.2f\n%s",s.hz,s.confidence,tool_==Tool::Guitar?"Guitar / selected string":"C2..B5 only / no stretch tuning");}else{std::snprintf(text,sizeof(text),"-- / listen for one note");std::snprintf(body,sizeof(body),"%s",tool_==Tool::Guitar?"Target E2 A2 D3 G3 B3 E4":"C2..B5 only / no full-range piano");}}
  LvglLockGuard lock;lv_label_set_text(value_,text);lv_label_set_text(info_,body);lv_label_set_text(status_,notice_.c_str());if(meter_){double cents=tool_==Tool::Guitar?audio_view::Cents(s.hz,Target()):s.cents;unsigned amount=tool_==Tool::Meter?(fresh?audio_view::Bar(s.dbfs):0):fresh&&s.pitch_valid?unsigned(std::clamp(50.+cents,0.,100.)):50;lv_bar_set_value(meter_,amount,LV_ANIM_OFF);}
 }
 void Navigate(Tool next,bool close=false){next_=next;close_pending_=close;pending_=true;if(owned_)AudioTools().RequestStop();}
public:
 AudioToolsApp(){setAppInfo().name="AUDIO TOOLS";static auto icon=assets::get_image("icon_dance.bin");setAppInfo().icon=&icon;}
 void onOpen()override{{LvglLockGuard lock;root_=lv_obj_create(lv_screen_active());lv_obj_set_pos(root_,0,0);stock_ui::Page(root_);}if(owned_&&AudioTools().PollStopped())owned_=false;tool_=Tool::Home;pending_=close_pending_=reference_=false;Build();Render(uint64_t(esp_timer_get_time()/1000));}
 void onRunning()override{
  uint64_t now=uint64_t(esp_timer_get_time()/1000);int a=action_.exchange(0);
  if(a==99)Navigate(Tool::Home,tool_==Tool::Home);
  else if(a>=11&&a<=18)Navigate(Tool(a-10));
  else if(a==21&&owned_){AudioTools().RequestStop();notice_="Stopping";}
  if(pending_){if(owned_&&!AudioTools().PollStopped()){notice_="Stopping; Stop retries failed cleanup";Render(now);return;}owned_=false;pending_=false;if(close_pending_){close_pending_=false;close();return;}tool_=next_;reference_=false;notice_="Stopped; tap Start";sample_seen_=false;last_samples_=0;Build();}
  else if(a==20&&tool_!=Tool::Home){if(AudioTools().PollStopped()){AudioTools().SetSpectrogramFrozen(false);sample_seen_=false;last_samples_=0;owned_=AudioTools().Start([]{return Board::GetInstance().GetAudioCodec();},Mode(),bpm_,Target());notice_=owned_?(Capture(tool_)&&!reference_?"Microphone on / local analysis":"Speaker on / reference"):"Audio could not start";}else notice_="Stop its owning audio tool first";}
  else if(a==22&&tool_==Tool::Spectrogram&&owned_)AudioTools().SetSpectrogramFrozen(!spectrum_->frozen);
  else if(a>=2000&&a<8144&&tool_==Tool::Spectrogram){unsigned position=unsigned(a-2000);inspect_column_=position/64;inspect_band_=position%64;}
  else if(a==92&&(tool_==Tool::Guitar||tool_==Tool::Piano)){if(AudioTools().PollStopped()){reference_=!reference_;notice_="Stopped; choose Mic or Tone then Start";Build();}else notice_="Stop before switching microphone / speaker";}
  else if(a>=60&&a<66&&tool_==Tool::Guitar){string_=unsigned(a-60);if(owned_&&reference_)AudioTools().Note(Target(),64);Build();}
  else if(a>=100&&a<112&&tool_==Tool::PitchPipe){note_=note_/12*12+unsigned(a-100);if(owned_)AudioTools().Note(note_,64);Build();}
  else if(a==40||a==41){int delta=a==41?1:-1;if(tool_==Tool::Metro){if(AudioTools().PollStopped())bpm_=unsigned(std::clamp(int(bpm_)+delta*5,20,300));else notice_="Stop to change tempo";}else if(tool_==Tool::PitchPipe)note_=unsigned(std::clamp(int(note_/12)+delta,3,6))*12+note_%12;else if(tool_==Tool::Piano&&reference_)note_=unsigned(std::clamp(int(note_)+delta,36,83));if(owned_&&(tool_==Tool::PitchPipe||reference_))AudioTools().Note(Target(),64);Build();}
  if(owned_&&AudioTools().PollStopped()){owned_=false;notice_="Stopped";}
  if(a||now>=render_at_){Render(now);render_at_=now+100;}
 }
 void onClose()override{if(owned_){AudioTools().RequestStop();AudioTools().PollStopped();}LvglLockGuard lock;if(root_)lv_obj_delete(root_);root_=nullptr;callbacks_.clear();canvas_pixels_.clear();action_=0;}
};
std::unique_ptr<mooncake::AppAbility> MakeAudioToolsApp(){return std::make_unique<AudioToolsApp>();}
std::unique_ptr<mooncake::AppAbility> MakeApp(){
#if STACKCHAN_BUILD_GAMES
 return MakePetApp();
#else
 return {}; // Legacy pet alias cannot pull a parked games module into music.
#endif
}
}
