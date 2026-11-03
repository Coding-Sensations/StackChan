#include "apps/runtime_lifecycle/stop_sequence.hpp"
#include <cassert>
#include <vector>
int main(){
 runtime_lifecycle::StopSequence s;
 for(unsigned run=0;run<50;++run){
  assert(s.Begin());auto epoch=s.Generation();assert(!s.Begin());
  unsigned revoked=0,released=0;bool tasks=false,hardware=false;
  s.RequestStop();assert(!s.Accepts(epoch));
  for(unsigned tick=0;tick<10;++tick){
   bool done=s.Poll(tick*100,[&]{++revoked;},[&]{return tasks;},[&]{return hardware;},[&]{++released;return true;});
   assert(!done&&released==0);
  }
  assert(revoked==1&&s.Pending(6000)); // expiry retains ownership; no reset/free
  tasks=true;assert(!s.Poll(6100,[]{},[&]{return tasks;},[&]{return hardware;},[&]{++released;return true;}));
  hardware=true;assert(s.Poll(6200,[]{},[&]{return tasks;},[&]{return hardware;},[&]{++released;return true;}));
  assert(released==1&&!s.Accepts(epoch));
 }
 assert(s.Begin());auto stale=s.Generation();s.RequestStop();
 assert(!s.Poll(0,[]{},[]{return true;},[]{return true;},[]{return false;}));
 assert(!s.Begin());assert(s.Poll(1,[]{},[]{return true;},[]{return true;},[]{return true;}));
 assert(s.Begin());assert(!s.Accepts(stale)); // stale producer cannot revive a new session
}
