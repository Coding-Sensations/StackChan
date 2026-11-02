#include <companion/muse_direct_audio.hpp>
#include <cassert>
#include <cstdio>
using namespace companion;
static muse::VoiceLease* target=nullptr;
static bool replace=false;
static bool Yes(){return true;}
static bool Original(){
 if(replace){replace=false;assert(target->Stop(true));assert(target->Reserve(9,Yes,Yes,Yes));}
 return true;
}
int main(){
 muse::VoiceLease lease;target=&lease;assert(lease.Reserve(7,Original,Yes,Yes));replace=true;
 // A callback replacing the old scope must not authorize it or revoke its successor.
 assert(!lease.Current());assert(lease.Current()&&CurrentAudioOwner()==AudioOwner::Muse);assert(lease.Stop(true));
 std::puts("PASS real VoiceLease callback replacement cannot authorize old/revoke new custody");
}
