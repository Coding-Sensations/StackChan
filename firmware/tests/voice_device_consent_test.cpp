#include "muse_codec_fixture.hpp"
#include "voice_admission_fixture.hpp"
#include <companion/muse_voice_session_owner.hpp>
#include <companion/muse_voice_core_codec.hpp>
#include "voice_pcm_fixture.hpp"
#include <companion/muse_voice_device_ui.hpp>
#include <companion/muse_direct_credential_custody.hpp>
#include <type_traits>
static_assert(!std::is_copy_constructible_v<companion::muse::CredentialLease> && !std::is_copy_assignable_v<companion::muse::CredentialLease> && !std::is_move_constructible_v<companion::muse::CredentialLease>);
struct Device {lv_indev_t* input=nullptr;bool foreground=true;std::string node="synthetic-device";bool Foreground()const{return foreground;}bool Node(std::string& out)const{out=node;return true;}lv_indev_t* Input()const{return input;}};
using Consent=DeviceVoiceConsent<Admission,Reader,Device>;
using Session=VoiceSessionOwner<Admission>;
using Ui=DeviceVoiceReviewUi<Session,Consent>;
static void Down(lv_obj_t* button,lv_indev_t& input){active_input=event_input=&input;input.active=button;input.state=LV_INDEV_STATE_PRESSED;Press(button);active_input=event_input=nullptr;}
static void Up(lv_obj_t* button,lv_indev_t& input){active_input=event_input=&input;input.active=button;input.state=LV_INDEV_STATE_RELEASED;ReleaseClick(button);active_input=event_input=nullptr;}
static void Tap(lv_obj_t*button,lv_indev_t&input){assert(button&&Visible(button));Down(button,input);Up(button,input);}
struct Rig {
 AdmissionOwner original;CoreS3AudioCodec core=Make();CoreVoiceCodec<CoreS3AudioCodec> codec{core};Device device;lv_indev_t pointer;Admission* admitted=original.admission.get();
 Consent consent{*admitted,original.reader,original.clock,device};std::unique_ptr<Session> session;std::unique_ptr<Ui> ui;
 Rig(){Reset();screen.w=320;screen.h=240;device.input=&pointer;session=Session::Create(original.protocol,original.admission,codec,consent,original.clock);assert(session);ui=std::make_unique<Ui>(*session,consent);}
 ~Rig(){assert(ui->Close());assert(session->Close());}
 void Live(){assert(session->Open(original.Scope()));original.port->incoming.push_back(original.server.Response(7,200));assert(session->Tick());original.port->incoming.push_back(original.server.Body(7,session_event));assert(session->Tick());}
 void Review(unsigned mask){assert(ui->Open(&screen));while(auto*next=Button("Next"))Tap(next,pointer);if(mask&1)Tap(Button("Mic: no"),pointer);if(mask&2)Tap(Button("Out: no"),pointer);Tap(Button("Review"),pointer);}
};
int main(){
#if !STACKCHAN_MUSE_RAM_ONLY
 {Rig r;r.Live();assert(!r.ui->Open(&screen)&&!r.consent.Bind()&&!r.consent.VoiceContext(7,0)&&!opens&&!reads&&!writes);}
 std::puts("PASS non-RAM device issuer refuses binding without audio IO");return 0;
#endif
 Reset();screen.w=320;screen.h=240;auto core=Make();CoreVoiceCodec<CoreS3AudioCodec> codec(core);AdmissionOwner original;Device device;lv_indev_t native;device.input=&native;Consent consent(*original.admission,original.reader,original.clock,device);auto session=Session::Create(original.protocol,original.admission,codec,consent,original.clock);Ui ui(*session,consent);
 assert(!ui.Open(&screen)&&!original.h.loads&&!original.h.connects);
 assert(session->Open(original.Scope()));original.port->incoming.push_back(original.server.Response(7,200));assert(session->Tick());original.port->incoming.push_back(original.server.Body(7,session_event));assert(session->Tick());
 assert(ui.Open(&screen));assert(!consent.VoiceContext(7,0)&&!ui.TakePrepareRequested());
 assert(ui.Close()&&session->Close()&&session->CleanupComplete()&&!LiveObjects());
 {
  Reset();auto second=Make();CoreVoiceCodec<CoreS3AudioCodec> second_codec(second);AdmissionOwner other;Device device;lv_indev_t native;device.input=&native;Consent unrelated(*other.admission,other.reader,other.clock,device);voice_codec_prep::NativeUnavailablePermit actual_authority;auto owner=Session::Create(other.protocol,other.admission,second_codec,actual_authority,other.clock);assert(owner->Open(other.Scope()));other.port->incoming.push_back(other.server.Response(7,200));assert(owner->Tick());other.port->incoming.push_back(other.server.Body(7,session_event));assert(owner->Tick());Ui wrong(*owner,unrelated);assert(!wrong.Open(&screen));assert(wrong.Close()&&owner->Close());
 }
 for(unsigned mask=0;mask<4;++mask){
  Rig r;r.Live();AdmittedVoiceContext context;assert(r.admitted->ReadAuthorizedContext(context)&&context.node=="synthetic-device"&&context.vm=="synthetic-vm"&&context.target.generation==7);r.Review(mask);
  assert(!r.consent.VoiceContext(7,0)&&!r.ui->TakePrepareRequested()&&!r.ui->TakeEndRequested()&&r.original.h.loads==1);
  if(!mask){assert(!Button("Allow")&&Text("No audio selected"));continue;}
  Click(Button("Allow"));assert(!r.consent.VoiceContext(7,0)&&!r.ui->TakePrepareRequested());
  Tap(Button("Allow"),r.pointer);assert(r.consent.VoiceContext(7,0)&&r.consent.Microphone(7,0)==bool(mask&1)&&r.consent.Output(7,0)==bool(mask&2)&&!r.consent.Hold(7,1,0));assert(r.ui->TakePrepareRequested()&&!r.ui->TakePrepareRequested());assert(r.session->PrepareDirections()&&r.session->DirectionsBound()&&!opens&&!reads&&!writes);
  auto borrow=r.session->BorrowReview();auto receipt=borrow.Get()->Review().Receipt();assert(!borrow.Get()->Review().NativePermit());assert(r.consent.MatchesReviewedContext(receipt.target,{bool(mask&1),bool(mask&2)},0));auto other=receipt.target;other.thread[0]='X';assert(!r.consent.MatchesReviewedContext(other,{bool(mask&1),bool(mask&2)},0));assert(!r.consent.MatchesReviewedContext(receipt.target,{!bool(mask&1),bool(mask&2)},0));borrow.Reset();
  Tap(Button("Return"),r.pointer);assert(r.consent.VoiceContext(7,0)&&r.ui->Show());Tap(Button("End session"),r.pointer);assert(r.ui->TakeEndRequested()&&!r.ui->TakeEndRequested()&&!r.consent.VoiceContext(7,0));assert(r.ui->Close()&&r.session->End());
 }
 for(unsigned fault=0;fault<6;++fault){
  Rig r;r.Live();r.Review(3);auto*allow=Button("Allow");lv_indev_t foreign;
  if(fault==0)Tap(allow,foreign);
  if(fault==1)Up(allow,r.pointer);
  if(fault==2){Down(allow,r.pointer);active_input=event_input=&r.pointer;lv_event_t event{allow,allow->callback_user,LV_EVENT_PRESS_LOST};allow->callback(&event);active_input=event_input=nullptr;Up(allow,r.pointer);}
  if(fault==3){Down(allow,r.pointer);r.device.input=&foreign;Up(allow,foreign);r.device.input=&r.pointer;}
  if(fault==4){Down(allow,r.pointer);assert(r.ui->Show());Up(allow,r.pointer);}
  if(fault==5){Down(allow,r.pointer);active_input=&r.pointer;event_input=&foreign;r.pointer.state=LV_INDEV_STATE_RELEASED;ReleaseClick(allow);active_input=event_input=nullptr;}
  assert(!r.consent.VoiceContext(7,0)&&!r.ui->TakePrepareRequested()&&!opens&&!reads&&!writes);
 }
 for(unsigned fault=0;fault<7;++fault){
  Rig r;r.Live();r.Review(3);Tap(Button("Allow"),r.pointer);assert(r.consent.VoiceContext(7,0));
  if(fault==0)r.original.h.credential=false;
  if(fault==1)r.original.port->verified=false;
  if(fault==2)r.device.foreground=false;
  if(fault==3)r.device.node="other-node";
  if(fault==4)r.original.clock.mono=10000000;
  if(fault==5)r.original.clock.wall=1760000010000LL;
  if(fault==6)r.original.clock.mono=-1;
  assert(!r.consent.VoiceContext(7,0)&&!r.ui->TakePrepareRequested()&&!r.ui->Poll()&&!LiveObjects());r.original.h.credential=true;r.original.port->verified=true;r.device.foreground=true;r.device.node="synthetic-device";r.original.clock.mono=0;r.original.clock.wall=1760000000000LL;assert(!r.consent.Bind()&&!r.consent.VoiceContext(7,0));
 }
 {
  Rig r;r.Live();r.Review(3);auto*allow=Button("Allow");Down(allow,r.pointer);r.original.clock.mono=10000001;Up(allow,r.pointer);assert(!r.consent.VoiceContext(7,0)&&!r.ui->TakePrepareRequested());
 }
 {
  Rig r;r.Live();assert(r.ui->Open(&screen));for(auto*button:{r.ui->Action(0),r.ui->Action(1),r.ui->Action(2),r.ui->Action(3)}){assert(button->x>=0&&button->y>=0&&button->x+button->w<=320&&button->y+button->h<=240);}
  r.original.clock.hook=[&]{assert(!r.ui->Close());};Tap(Button("Next"),r.pointer);assert(!r.consent.VoiceContext(7,0)&&r.ui->Close()&&r.session->Close());
 }
 {
  Rig r;r.Live();r.Review(3);Tap(Button("Allow"),r.pointer);assert(r.consent.VoiceContext(7,0));auto view=r.session->BorrowReview();view.Get()->Review().Cancel();assert(!r.consent.VoiceContext(7,0)&&!r.ui->TakePrepareRequested());view.Reset();
 }
 for(unsigned dimension=0;dimension<2;++dimension){
  Rig r;r.Live();r.Review(3);Tap(Button("Allow"),r.pointer);assert(r.consent.VoiceContext(7,0));
  const int begin=r.original.clock.samples;assert(r.consent.BoundCurrent());const int count=r.original.clock.samples-begin;assert(count>1);const int start=r.original.clock.samples;
  r.original.clock.sample_hook=[&](int index){const bool final=index==start+count;if(dimension==0)r.original.clock.mono=final?1000:2000;else r.original.clock.wall=1760000000000LL+(final?1:2);};
  assert(!r.consent.VoiceContext(7,0));r.original.clock.sample_hook={};r.original.clock.mono=2000;r.original.clock.wall=1760000000002LL;assert(!r.consent.Bind());
 }
 std::puts("PASS actual SDK/device consent/review UI source with inert input; no live native authority claim");
}
