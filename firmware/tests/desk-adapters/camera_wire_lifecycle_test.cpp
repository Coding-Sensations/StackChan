// Prepared source fixture only. Not compiled while build approval is blocked.
// These private friends construct synthetic sealed software receipts. No actual
// driver, camera, physical qualifier, GPIO, network or SDK runtime is exercised.
#include "companion/camera_authority_ledger.hpp"
#include <cassert>
class StackChanCamera {
public:
 static companion::desk::CameraDriverStopReceipt Receipt(companion::desk::CameraBinding b,uint64_t id,uint64_t serial) {
  companion::desk::CameraDriverStopReceipt r;r.binding_=b;r.authority_=id;r.serial_=serial;
  r.outcome_=companion::desk::CameraDriverStopOutcome::NotStarted;
  r.revoked_=r.producer_quiesced_=r.raw_wiped_=true;return r;
 }
};
class M5StackCoreS3Board {
public:
 static companion::desk::BoardCameraStopReport Report(companion::desk::CameraBinding b,uint64_t id,uint64_t serial) {
  companion::desk::BoardCameraStopReport r;r.driver_=StackChanCamera::Receipt(b,id,serial);
  r.requested_=b;r.exact_session_=r.custody_released_=true;return r;
 }
};
int main() {
 using namespace companion;
 CameraAuthorityContext c{"device","conversation","turn","purpose",std::string(32,'b'),1,10100,7,{1,1}},out;
 bool current=false;
 CameraRestartBarrier barrier;CameraStopChallenge challenge{c,std::string(32,'a'),1};
 auto route=[](uint64_t n,uint32_t e,const auto&){return n==1&&e==1;};
 auto capture=[&](auto,auto,const auto&){return current;};
 BoundedCameraSessionDispatcher ledger(capture,capture,route,[&](const auto& held,const auto& r){
   return held.binding==challenge.owner.binding&&barrier.ReadyAck(challenge,r.driver().report_serial());
 });
 // Reservation retained before host commit. No capture authority exists.
 assert(ledger.RetainReservationFromTrustedOwner(1,1,100,c));
 assert(!ledger.Snapshot(1,1,100,out));assert(ledger.RetainedSnapshot(1,1,out));
 auto report=M5StackCoreS3Board::Report(c.binding,c.authority_id,1);
 assert(!report.physical_off());assert(!ledger.ReleaseAfterVerifiedAck(c,report));
 assert(barrier.Begin(challenge));assert(!barrier.Completed(report,false,true));
 assert(barrier.Completed(report,true,ledger.RevokedNoNewerStream(c,report)));
 auto wrong=challenge;wrong.nonce=std::string(32,'c');assert(!barrier.ReadyAck(wrong,1));
 assert(!barrier.ReadyAck(challenge,2));
 assert(ledger.ReleaseAfterVerifiedAck(c,report));assert(barrier.Acknowledge(challenge,1));
 assert(!ledger.RetainedSnapshot(1,1,out));assert(!ledger.RetainReservationFromTrustedOwner(1,1,101,c));
 // Board authority ID can persist; a new generation and fresh purpose is needed.
 c.binding.generation=2;c.purpose_id="second-purpose";
 assert(ledger.RetainReservationFromTrustedOwner(1,1,102,c));current=true;
 auto shorter=c;shorter.expires_ms=9000;
 assert(ledger.InstallFromTrustedOwner(1,1,103,shorter));assert(ledger.Snapshot(1,1,103,out));
 ledger.RevokeFromTrustedOwner();assert(!ledger.InstallFromTrustedOwner(1,1,104,shorter));
 assert(ledger.RetainedSnapshot(1,1,out));assert(out.expires_ms==9000);
 assert(!ledger.ReleaseAfterVerifiedAck(shorter,report)); // old report never clears newer custody
 // Admission exhaustion or an older sampled clock cannot orphan an actual
 // newly acquired lease. Cleanup remains, capture activation stays denied.
 BoundedCameraSessionDispatcher bounded(capture,capture,route,[](const auto&,const auto&){return true;});
 auto cycle=c;current=true;
 for(unsigned i=1;i<=65;++i){
   cycle.binding.generation=i;
   assert(bounded.RetainReservationFromTrustedOwner(1,1,100,cycle));
   if(i==65)assert(!bounded.InstallFromTrustedOwner(1,1,100,cycle));
   assert(bounded.RetainedSnapshot(1,1,out));
   auto stopped=M5StackCoreS3Board::Report(cycle.binding,cycle.authority_id,i);
   bounded.RevokeFromTrustedOwner();assert(bounded.ReleaseAfterVerifiedAck(cycle,stopped));
 }
 cycle.binding.generation=66;
 assert(bounded.RetainReservationFromTrustedOwner(1,1,99,cycle));
 assert(!bounded.InstallFromTrustedOwner(1,1,100,cycle));assert(bounded.RetainedSnapshot(1,1,out));
 // Existing direct trusted-owner entrypoint retains its capture-callback gate.
 BoundedCameraSessionDispatcher direct(capture,capture,route);
 assert(direct.InstallFromTrustedOwner(1,1,100,c));assert(!direct.InstallFromTrustedOwner(1,1,101,c));
 current=false;BoundedCameraSessionDispatcher denied(capture,capture,route);
 assert(!denied.InstallFromTrustedOwner(1,1,100,c));assert(!denied.RetainedSnapshot(1,1,out));
}
