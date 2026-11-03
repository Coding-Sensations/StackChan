#pragma once
#include <atomic>
#include <cstdint>
#include <limits>
namespace runtime_lifecycle {
// Start is owner-thread only. Cancellation may arrive from the UI task. A timed
// out drain retains its owners and can be polled again; it never authorizes IO.
class StopSequence {
 std::atomic<bool> open_{false},stop_{true};
 std::atomic<uint64_t> generation_{0};
 bool revoked_=false,joined_=false,closed_=false;uint64_t since_=0;
public:
 bool Begin(){if(open_||generation_==std::numeric_limits<uint64_t>::max())return false;
  ++generation_;revoked_=joined_=closed_=false;since_=0;stop_=false;open_=true;return true;}
 uint64_t Generation()const{return generation_;}
 bool Accepts(uint64_t epoch)const{return open_&&!stop_&&epoch&&epoch==generation_;}
 void RequestStop(){if(open_)stop_=true;}
 bool Stopping()const{return open_&&stop_;}
 bool Open()const{return open_;}
 bool Pending(uint64_t now)const{return Stopping()&&revoked_&&now-since_>=5000;}
 template<class Revoke,class Join,class Close,class Release>
 bool Poll(uint64_t now,Revoke revoke,Join join,Close close,Release release){
  if(!open_)return true;
  if(!stop_)return false;
  if(!revoked_){revoke();revoked_=true;since_=now;}
  if(!joined_)joined_=join();
  if(!joined_)return false;
  if(!closed_)closed_=close();
  if(!closed_)return false;
  if(!release())return false;
  open_=false;return true;
 }
};
}
