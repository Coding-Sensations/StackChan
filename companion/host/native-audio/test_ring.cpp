#include "pcm_ring.hpp"
#include <cassert>
#include <thread>
#include <vector>
int main() {
    PcmRing<8> ring; int16_t a[]{1,2,3,4,5,6},out[8]{};
    assert(ring.push(1,a,6)==6); assert(ring.pull(1,out,3)==3);
    assert(out[0]==1 && out[2]==3);
    assert(ring.push(2,a,6)==5); assert(ring.dropped==1);
    assert(ring.pull(2,out,8)==5); assert(out[0]==1 && out[4]==5 && out[5]==0);
    assert(ring.stale==3 && ring.underrun==3);
    // In-flight old-epoch samples never cross an interruption boundary.
    assert(ring.push(2,a,6)==6); assert(ring.pull(3,out,8)==0);
    for(auto sample:out) assert(sample==0);
    PcmRing<64> concurrent;
    std::atomic<bool> done{false}; std::atomic<unsigned> count{0};
    std::thread producer([&]{ for(unsigned i=0;i<100000;++i) { int16_t value=static_cast<int16_t>(i%30000); while(concurrent.push(7,&value,1)!=1) std::this_thread::yield(); } done=true; });
    std::thread consumer([&]{ unsigned expected=0; while(!done || expected<100000) { int16_t value=0; if(concurrent.pull(7,&value,1)) { assert(value==static_cast<int16_t>(expected%30000)); ++expected; } else std::this_thread::yield(); } count=expected; });
    producer.join(); consumer.join(); assert(count==100000);
}
