#include <companion/pair_connection_consent.hpp>
#include <cassert>
int main(){using namespace companion;std::string r(32,'a');PairConnectionConsent gate;
assert(!gate.Arm(r,100,true,false,0));assert(!gate.Arm(r,100,false,true,0));assert(!gate.Arm("bad",100,true,true,0));assert(!gate.Arm(r,0,true,true,0));
assert(gate.Arm(r,100,true,true,0));assert(!gate.RequestConnection(false));assert(gate.RequestConnection(true));assert(!gate.RequestConnection(true));assert(!gate.Take(10,false)&&gate.Pending());assert(gate.Take(11,true));assert(!gate.Take(12,true));
assert(gate.Arm(r,100,true,true,0));assert(!gate.RequestConnection(false));assert(gate.RequestConnection(true));assert(!gate.RequestConnection(true));assert(!gate.Take(100,true)&&!gate.Pending());
assert(gate.Arm(r,100,true,true,0));assert(!gate.RequestConnection(false));assert(gate.RequestConnection(true));assert(!gate.RequestConnection(true));gate.Cancel();assert(!gate.Take(10,true));
NumericPairingContext context;std::string d(32,'b'),h(32,'c');assert(context.Start(r,d,123,0));assert(context.Challenge(h,"123456",60000,0,true));assert(context.AudioRequested());assert(context.Confirm(1));assert(!context.Grant(r,d,h,90,2,false));assert(context.Grant(r,d,h,90,2,true));context.Cancel();assert(!context.AudioRequested()&&!context.Confirmed());
}
