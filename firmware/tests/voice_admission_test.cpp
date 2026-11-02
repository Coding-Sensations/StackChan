#include "voice_admission_fixture.hpp"
int main(){
 {
  AdmissionOwner o;assert(o.admission->Open(o.Scope()));assert(o.h.loads==1&&o.h.finds==1&&o.h.connects==1&&o.h.sends==2);o.port->incoming.push_back(o.server.Response(7,200));assert(o.admission->Tick());o.port->incoming.push_back(o.server.Body(7,session_event));assert(o.admission->Tick());auto* review=o.admission->Review();assert(review&&review->Open());assert(!review->Review().NativePermit());assert(o.admission->Close()&&o.admission->CleanupComplete());assert(!review->Open()&&!review->Review().Active());assert(!o.admission->Open(o.Scope()));
 }
 for(int error=0;error<6;++error){AdmissionOwner o;auto scope=o.Scope();if(error==0)o.h.credential=false;if(error==1)o.h.network=false;if(error==2)o.h.voice=false;if(error==3)scope.call="invalid call";if(error==4)scope.budget.generation=8;if(error==5)scope.credential_current={};assert(!o.admission->Open(scope)&&!o.h.loads&&!o.h.connects);assert(o.admission->Close());}
 for(int error=0;error<5;++error){AdmissionOwner o;if(error==0)o.h.wrong_node=true;if(error==1)o.h.wrong_vm=true;if(error==2)o.h.load_hook=[&]{o.h.credential=false;};if(error==3)o.h.find_hook=[&]{o.h.network=false;};if(error==4)o.h.lookup_join=false;assert(!o.admission->Open(o.Scope())&&!o.h.connects);if(error==4)assert(!o.admission->Close()&&!o.admission->CleanupComplete());o.h.lookup_join=true;assert(o.admission->Close());}
 for(int error=0;error<4;++error){AdmissionOwner o;if(error<2)o.h.fail_send=error+1;if(error==2)o.h.connect_hook=[&]{o.h.voice=false;};if(error==3)o.h.send_hook=[&]{o.h.network=false;};assert(!o.admission->Open(o.Scope()));const int sends=o.h.sends;assert(!o.admission->Open(o.Scope())&&o.h.sends==sends);o.h.socket_join=false;assert(!o.admission->Close()&&!o.admission->CleanupComplete());o.h.socket_join=true;assert(o.admission->Close());}
 for(int bad=0;bad<4;++bad){AdmissionOwner o;auto scope=o.Scope();if(bad==0)scope.call[2]=' ';if(bad==1)scope.call[2]='"';if(bad==2)scope.call[2]='\0';if(bad==3)scope.node="synthetic.device";assert(!o.admission->Open(scope)&&!o.h.loads&&!o.h.finds&&!o.h.connects&&!o.h.sends);assert(o.admission->Close());}
 {
  AdmissionOwner o;o.clock.sample_hook=[&](int n){if(n==2)o.h.credential=false;};assert(!o.admission->Open(o.Scope())&&!o.h.loads&&!o.h.connects);assert(o.admission->Close());
 }
 for(int phase=0;phase<3;++phase){AdmissionOwner o;auto close=[&]{assert(!o.admission->Close());};if(phase==0)o.h.find_hook=close;if(phase==1)o.h.connect_hook=close;if(phase==2)o.h.send_hook=close;assert(!o.admission->Open(o.Scope()));assert(o.admission->Close());}
 {
  AdmissionOwner o;auto scope=o.Scope();assert(o.admission->Open(scope));scope.node="other-device";scope.vm="other-vm";scope.budget.generation=8;o.h.voice=false;const int sends=o.h.sends;assert(!o.admission->Tick()&&o.h.sends==sends);o.h.socket_join=false;assert(!o.admission->Close());o.h.socket_join=true;assert(o.admission->Close());
 }
 std::puts("PASS actual native admission/SDK handshake/worker composition; synthetic credentials, crypto and ports only");
}
