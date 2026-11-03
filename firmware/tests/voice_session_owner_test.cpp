#include "muse_codec_fixture.hpp"
#include "voice_admission_fixture.hpp"
#include <companion/muse_voice_session_owner.hpp>
#include <companion/muse_voice_core_codec.hpp>
#include "voice_pcm_fixture.hpp"
using Session=VoiceSessionOwner<Admission>;
#include "voice_native_authority_fixture.hpp"
struct Composed {
 AdmissionOwner original;CoreS3AudioCodec core=Make();CoreVoiceCodec<CoreS3AudioCodec> codec{core};Matching native;std::unique_ptr<Session> session;
 Composed(){Reset();session=Session::Create(original.protocol,original.admission,codec,native,original.clock);assert(session&&!original.admission);}
 ~Composed(){disable_result=0;original.h.lookup_join=original.h.socket_join=true;assert(session->Close());}
 void Live(){assert(session->Open(original.Scope()));original.port->incoming.push_back(original.server.Response(7,200));assert(session->Tick());assert(!session->BorrowReview());original.port->incoming.push_back(original.server.Body(7,session_event));assert(session->Tick());}
 Session::ReviewBorrow Reviewed(bool directions=true){auto ui=session->BorrowReview();assert(ui&&ui.Get()->Open());auto&m=ui.Get()->Review();while(!m.Reviewed())assert(m.Acknowledge(m.Revision(),m.Page()));assert(m.OptIn(m.Revision(),directions,directions)&&m.Finish(m.Revision()));native.expected=m.Receipt();return ui;}
};
int main(){
 Reset();auto core=Make();CoreVoiceCodec<CoreS3AudioCodec> codec(core);AdmissionOwner original;voice_codec_prep::NativeUnavailablePermit native;
 auto session=Session::Create(original.protocol,original.admission,codec,native,original.clock);assert(session&&!original.admission&&!original.h.loads&&!session->BorrowReview());
 assert(session->Open(original.Scope()));original.port->incoming.push_back(original.server.Response(7,200));assert(session->Tick());original.port->incoming.push_back(original.server.Body(7,session_event));assert(session->Tick());
 auto ui=session->BorrowReview();assert(ui&&ui.Get()&&ui.Get()->Open());auto&m=ui.Get()->Review();while(!m.Reviewed())assert(m.Acknowledge(m.Revision(),m.Page()));assert(m.OptIn(m.Revision(),true,true)&&m.Finish(m.Revision()));assert(!session->PrepareDirections()&&CurrentAudioOwner()==AudioOwner::None);
 auto* original_review=ui.Get();assert(!session->Close()&&session->TransportJoined()&&!session->CleanupComplete()&&!ui.Get());assert(!original_review->Review().Active());ui.Reset();assert(session->Close()&&session->CleanupComplete());
 {
  Reset();auto another=Make();CoreVoiceCodec<CoreS3AudioCodec> second_codec(another);AdmissionOwner original;voice_codec_prep::NativeUnavailablePermit unavailable;auto session=Session::Create(original.protocol,original.admission,second_codec,unavailable,original.clock);auto scope=original.Scope();scope.voice_current={};assert(!session->Open(scope)&&!original.clock.samples&&!original.h.loads&&session->Close());
 }
 {
  Composed c;c.Live();auto ui=c.Reviewed();assert(c.session->PrepareDirections()&&c.session->DirectionsBound());const int writes_before=c.original.h.sends;
  disable_result=-1;assert(!c.session->Close()&&!c.session->TransportJoined()&&c.original.h.socket_closes==0&&CurrentAudioOwner()==AudioOwner::Muse);assert(!c.session->Tick()&&!c.session->Capture()&&c.original.h.sends==writes_before);
  disable_result=0;assert(!c.session->Close()&&c.session->TransportJoined()&&CurrentAudioOwner()==AudioOwner::None&&c.original.h.socket_closes==1&&c.original.h.socket_destroyed==0);ui.Reset();assert(c.session->Close()&&c.original.h.socket_destroyed==1);
 }
 {
  Composed c;c.Live();auto ui=c.Reviewed();assert(c.session->PrepareDirections());c.original.h.socket_join=false;
  assert(!c.session->Close()&&!c.session->TransportJoined()&&CurrentAudioOwner()==AudioOwner::None&&c.original.h.socket_destroyed==0);const auto sends=c.original.h.sends;assert(!c.session->Tick()&&c.original.h.sends==sends);
  c.original.h.socket_join=true;assert(!c.session->Close()&&c.session->TransportJoined()&&c.original.h.socket_destroyed==0);ui.Reset();assert(c.session->Close()&&c.original.h.socket_destroyed==1);
 }
 {
  Composed c;c.Live();std::array<Session::ReviewBorrow,Session::ReviewBorrowLimit> ui;for(auto&ref:ui){ref=c.session->BorrowReview();assert(ref);}assert(!c.session->BorrowReview()&&c.session->ReviewBorrows()==8);auto moved=std::move(ui[0]);assert(moved&&!ui[0]&&c.session->ReviewBorrows()==8);ui[1]=std::move(moved);assert(!moved&&c.session->ReviewBorrows()==7);assert(!c.session->Close()&&!ui[1].Get());for(auto&ref:ui)ref.Reset();assert(c.session->Close()&&c.original.h.socket_destroyed==1);
 }
 for(int phase=0;phase<3;++phase)for(bool ending:{false,true}){
  Composed c;auto stop=[&]{assert(!(ending?c.session->End():c.session->Close()));};if(phase==0)c.original.h.find_hook=stop;if(phase==1)c.original.h.connect_hook=stop;if(phase==2)c.original.h.send_hook=stop;
  assert(!c.session->Open(c.original.Scope()));const auto sends=c.original.h.sends;assert(!c.session->Open(c.original.Scope())&&!c.session->Tick()&&c.original.h.sends==sends);assert(c.session->Close()&&!c.session->EndConfirmed());
 }
 for(bool ending:{false,true}){
  Composed c;assert(c.session->Open(c.original.Scope()));c.original.h.send_hook=[&]{assert(!(ending?c.session->End():c.session->Close()));};assert(!c.session->Tick());const auto sends=c.original.h.sends;assert(!c.session->Tick()&&c.original.h.sends==sends);assert(c.session->Close());
 }
 for(bool ending:{false,true}){
  Composed c;c.Live();auto ui=c.Reviewed();c.original.port->receive_hook=[&]{assert(!(ending?c.session->End():c.session->Close()));};assert(!c.session->Tick()&&c.session->CloseRequested()&&!ui.Get());assert(!c.session->Close());ui.Reset();assert(c.session->Close());
 }
 {
  Composed c;assert(c.session->Open(c.original.Scope()));c.original.h.fail_send=c.original.h.sends+1;assert(!c.session->Tick());const auto sends=c.original.h.sends;assert(!c.session->Tick()&&c.original.h.sends==sends&&c.original.h.connects==1);assert(c.session->Close()&&!c.session->EndConfirmed());
 }
 {
  Composed c;c.Live();auto ui=c.Reviewed();assert(c.session->PrepareDirections());assert(!c.session->Capture()&&!c.session->Play()&&!c.session->CloseRequested()&&c.session->DirectionsBound()&&c.session->Tick());c.original.clock.okay=false;assert(!c.session->Close()&&c.session->TransportJoined()&&CurrentAudioOwner()==AudioOwner::None);ui.Reset();assert(c.session->Close());
 }
 {
  Composed c;c.Live();auto ui=c.Reviewed();assert(c.session->PrepareDirections());c.original.h.voice=false;const auto sends=c.original.h.sends;assert(!c.session->Tick()&&c.session->CloseRequested()&&c.original.h.sends==sends&&CurrentAudioOwner()==AudioOwner::None);assert(!c.session->Close());ui.Reset();assert(c.session->Close());
 }
 {
  Composed c;c.Live();auto ui=c.Reviewed();assert(c.session->PrepareDirections());assert(c.session->End()&&c.session->EndRequested()&&!c.session->DirectionsBound()&&!c.session->EndConfirmed()&&CurrentAudioOwner()==AudioOwner::None);assert(!c.session->Capture()&&!c.session->Play());
  c.original.port->incoming.push_back(c.original.server.Body(7,"{\"type\":\"response.done\"}\n",true));assert(c.session->Tick()&&!c.session->EndConfirmed());c.original.port->incoming.push_back(c.original.server.Response(9,200,"{\"ok\":true}",true));assert(!c.session->Tick()&&c.session->EndConfirmed());assert(!c.session->Close());ui.Reset();assert(c.session->Close());
 }
 {
  Composed c;c.Live();auto ui=c.Reviewed();assert(c.session->PrepareDirections());disable_result=-1;const auto sends=c.original.h.sends;assert(!c.session->End()&&!c.session->Tick()&&c.original.h.sends==sends&&c.original.h.socket_closes==0);disable_result=0;assert(c.session->End());assert(!c.session->Close());ui.Reset();assert(c.session->Close());
 }
 {
  Reset();auto core=Make();CoreVoiceCodec<CoreS3AudioCodec> codec(core);AdmissionOwner original;voice_codec_prep::NativeUnavailablePermit native;Protocol other_protocol;Clock other_clock;auto* candidate=original.admission.get();assert(!Session::Create(other_protocol,original.admission,codec,native,original.clock)&&original.admission.get()==candidate);assert(!Session::Create(original.protocol,original.admission,codec,native,other_clock)&&original.admission.get()==candidate&&!original.h.loads);assert(original.admission->Open(original.Scope()));assert(!Session::Create(original.protocol,original.admission,codec,native,original.clock)&&original.admission.get()==candidate);assert(original.admission->Close());
 }
 {
  Composed c;c.Live();auto ui=c.Reviewed();assert(c.session->PrepareDirections());assert(!c.session->Talking(5,true)&&c.session->CloseRequested()&&!reads&&!writes&&CurrentAudioOwner()==AudioOwner::None);assert(!c.session->Close());ui.Reset();assert(c.session->Close()); // actual native FlushOutput remains unavailable
 }
 {
  Composed c;c.Live();auto ui=c.Reviewed();assert(c.session->PrepareDirections());during=[&]{assert(!c.session->Close()&&c.original.h.socket_closes==0);};assert(!c.session->Close()&&c.session->TransportJoined()&&CurrentAudioOwner()==AudioOwner::None);ui.Reset();assert(c.session->Close());
 }
 {
  Composed c;c.Live();auto ui=c.Reviewed();c.original.port->close_hook=[&]{assert(!c.session->Close()&&c.original.h.socket_destroyed==0);};assert(!c.session->Close()&&c.session->TransportJoined());ui.Reset();assert(c.session->Close()&&c.original.h.socket_destroyed==1);
 }
 for(unsigned phase=0;phase<3;++phase)for(unsigned grant=0;grant<3;++grant)for(unsigned action=0;action<3;++action){
  Composed c;std::function<void()> trigger;auto scope=c.original.Scope();std::array<uint64_t,16> marker{};marker[7]=0x12345678;
  const auto callback=[&,marker]{auto f=std::move(trigger);trigger={};if(f)f();const volatile uint64_t* retained=marker.data();assert(retained[7]==0x12345678);return grant==0?c.original.h.credential:grant==1?c.original.h.network:c.original.h.voice;};
  if(grant==0)scope.credential_current=callback;else if(grant==1)scope.network_current=callback;else scope.voice_current=callback;
  assert(c.session->Open(scope));c.original.port->incoming.push_back(c.original.server.Response(7,200));assert(c.session->Tick());c.original.port->incoming.push_back(c.original.server.Body(7,session_event));assert(c.session->Tick());auto ui=c.session->BorrowReview();auto* view=ui.Get();assert(view);if(phase)assert(view->Open());
  if(phase==2){auto&m=view->Review();while(!m.Reviewed())assert(m.Acknowledge(m.Revision(),m.Page()));assert(m.OptIn(m.Revision(),true,true)&&m.Finish(m.Revision()));}
  bool called=false;trigger=[&]{called=true;assert(!(action==0?c.session->Close():action==1?c.session->End():c.session->Tick()));};
  if(phase==0)assert(!view->Open());else if(phase==1)assert(!view->Poll());else assert(!view->Review().Receipt().valid);
  assert(called&&!c.session->TransportJoined()&&c.original.h.socket_closes==0&&c.original.h.socket_destroyed==0&&!ui.Get());assert(!c.session->Close()&&c.session->TransportJoined());ui.Reset();assert(c.session->Close());
 }
 std::puts("PASS actual session/audio/UI retention; synthetic authority and ports only");
}
