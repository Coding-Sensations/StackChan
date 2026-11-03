#include "companion/station_authority.hpp"
#include "companion/checked_station_stop.hpp"
#include "companion/saved_wifi_owner.hpp"
#include <cassert>
#include <thread>
using namespace companion::network;
struct Driver {
 int wifi=1,ip=1,netif=1;bool stopped=false,idle=false;int fail=0;
 bool UnregisterWifi(){if(fail==1)return false;wifi=0;return true;}
 bool UnregisterIp(){assert(!wifi);if(fail==2)return false;ip=0;return true;}
 bool StopDriver(){assert(!wifi&&!ip);if(fail==3)return false;stopped=true;return true;}
 bool DrainEvents(){return fail!=6;}
 bool DestroyNetif(){assert(stopped);if(fail==4)return false;netif=0;return true;}
 bool VerifyIdle(){assert(!netif);if(fail==5)return false;idle=true;return true;}
};
int main(){
 StationAuthority a;
 for(int repeat=0;repeat<100;++repeat){
  assert(a.Acquire(Owner::Saved));auto epoch=a.Generation();assert(a.Accepts(Owner::Saved,epoch));
  auto child=a.BorrowSaved();assert(child);assert(a.Borrowers()==1);
  a.RequestStop(Owner::Saved);assert(!a.Accepts(Owner::Saved,epoch));assert(!a.Acquire(Owner::Vendor));assert(!a.BorrowSaved());
  assert(!a.FinishStop(Owner::Saved,true)); // live HTTP/TLS child retains exclusive custody
  std::thread consumer([b=std::move(child)]()mutable{b.Reset();});consumer.join();assert(a.Borrowers()==0);
  Driver d;for(int step=1;step<=6;++step){d.fail=step;assert(!StopStationChecked(d));assert(!a.FinishStop(Owner::Saved,false));assert(!a.Acquire(Owner::Vendor));}
  d.fail=0;assert(StopStationChecked(d));assert(a.FinishStop(Owner::Saved,d.idle));
  assert(a.Acquire(Owner::Vendor));assert(!a.Acquire(Owner::Saved));assert(!a.Accepts(Owner::Saved,epoch));
  a.RequestStop(Owner::Vendor);assert(!a.FinishStop(Owner::Vendor,false));assert(!a.Acquire(Owner::Saved));assert(a.FinishStop(Owner::Vendor,true));
  assert(!a.FinishStop(Owner::Saved,true)); // stale saved-owner receipt cannot release a later session
 }
 companion::SavedWifiOwner saved;saved.Begin(true);saved.Touched();saved.InitializationFailed();saved.StopRequested();assert(saved.Begin(true)==companion::SavedWifiAction::Refused);assert(!saved.Stopped(false));assert(saved.Stopped(true));assert(saved.Begin(true)==companion::SavedWifiAction::Initialize);
}
