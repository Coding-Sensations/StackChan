#include "companion/camera_authority_ledger.hpp"
#include <cassert>
// Synthetic fixture friends populate the actual sealed software report types.
// They are never linked into firmware and establish no physical qualification.
class StackChanCamera {
public:
 static companion::desk::CameraDriverStopReceipt Receipt(companion::desk::CameraBinding b,uint64_t id) {
  companion::desk::CameraDriverStopReceipt r;r.binding_=b;r.authority_=id;r.serial_=1;
  r.outcome_=companion::desk::CameraDriverStopOutcome::NotStarted;
  r.revoked_=r.producer_quiesced_=r.raw_wiped_=true;return r;
 }
};
class M5StackCoreS3Board {
public:
 static companion::desk::BoardCameraStopReport Report(companion::desk::CameraBinding b,uint64_t id) {
  companion::desk::BoardCameraStopReport r;r.driver_=StackChanCamera::Receipt(b,id);
  r.requested_=b;r.exact_session_=r.custody_released_=true;return r;
 }
};
int main() {
 using namespace companion;
 CameraAuthorityContext c{"device","conversation","turn","purpose",std::string(32,'b'),1,10100,7,{1,1}},out;
 BoundedCameraSessionDispatcher absent;
 assert(!absent.InstallFromTrustedOwner(1,1,100,c));
 bool ui=true,device=true,ack=false;
 auto purpose=[&](uint64_t n,uint32_t e,const CameraAuthorityContext&){return ui&&n==1&&e==1;};
 auto optin=[&](uint64_t,uint32_t,const CameraAuthorityContext&){return device;};
 auto cleanup=[](uint64_t n,uint32_t e,const CameraAuthorityContext&){return n==2&&e==2;};
 BoundedCameraSessionDispatcher d(purpose,optin,cleanup,[&](const auto&,const auto&){return ack;});
 assert(d.InstallFromTrustedOwner(1,1,100,c));assert(d.Snapshot(1,1,100,out));
 auto wrong=c;wrong.turn_generation=2;assert(!d.Current(1,1,wrong,100));
 assert(!d.InstallFromTrustedOwner(1,1,100,c));
 assert(!d.RetainedSnapshot(1,1,out));assert(d.RetainedSnapshot(2,2,out));
 assert(!d.Snapshot(2,2,101,out)); // reconnect is cleanup only and revokes capture
 assert(d.RetainedStopCurrent(2,2,c));
 auto report=M5StackCoreS3Board::Report(c.binding,c.authority_id);
 assert(!report.physical_off());assert(!report.driver().physical_stop_qualified());
 CameraRestartBarrier barrier;CameraStopChallenge stop{c,std::string(32,'a'),1};
 assert(barrier.Begin(stop));assert(barrier.Pending());
 assert(!barrier.Completed(report,false,true));assert(!barrier.Completed(report,true,false));
 assert(barrier.Completed(report,true,true));assert(barrier.ReadyAck(stop,1));
 assert(!barrier.Acknowledge(stop,2));assert(barrier.Acknowledge(stop,1));assert(!barrier.Pending());
 assert(d.RevokedNoNewerStream(c,report));
 assert(!d.ReleaseAfterVerifiedAck(c,report));ack=true;
 assert(d.ReleaseAfterVerifiedAck(c,report));assert(!d.RetainedSnapshot(2,2,out));
 assert(!d.InstallFromTrustedOwner(1,1,102,c)); // generation and ID replay
 c.binding.generation=2;assert(d.InstallFromTrustedOwner(1,1,102,c)); // same board ID, fresh generation
 assert(!d.InstallFromTrustedOwner(1,1,102,c)); // exact tuple replay/replacement denied
 assert(!d.Snapshot(1,1,10100,out));assert(d.RetainedSnapshot(2,2,out));
 assert(!d.InstallFromTrustedOwner(1,1,10100,c));
 assert(!d.Snapshot(1,1,99,out)); // backward clock
 assert(!d.RevokedNoNewerStream(c,report)); // stale actual report
 BoundedCameraSessionDispatcher faults([](auto,auto,const auto&)->bool{throw 1;},optin,cleanup);
 assert(!faults.InstallFromTrustedOwner(1,1,100,c));
 device=false;BoundedCameraSessionDispatcher off(purpose,optin,cleanup);
 assert(!off.InstallFromTrustedOwner(1,1,100,c));
}
