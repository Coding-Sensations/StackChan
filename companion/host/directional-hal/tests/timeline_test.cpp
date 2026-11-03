#include "../timeline.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <thread>
#include <vector>
using stackchan::hal::Timeline;
int main() {
    Timeline t;
    float in[8]={.1f,.2f,.3f,.4f,.5f,.6f,.7f,.8f}, out[8];
    t.read(100,4,out); for(float x:out) assert(x==0);
    assert(!t.write(100,4,in));
    assert(t.start()); assert(t.write(100,4,in));
    t.read(100,4,out); for(int i=0;i<8;++i) assert(out[i]==in[i]);
    // A second reader receives the same frames; reads never consume a cursor.
    t.read(100,4,out); for(int i=0;i<8;++i) assert(out[i]==in[i]);
    assert(!t.write(100,4,in)); // duplicate/backwards write is rejected
    t.read(99,4,out); assert(out[0]==0 && out[2]==in[0]);
    t.stop(); t.read(100,4,out); for(float x:out) assert(x==0);
    assert(t.start()); t.read(100,4,out); for(float x:out) assert(x==0);
    // Restart can use the same sample time but cannot reveal an earlier epoch.
    assert(t.write(100,4,in));
    in[0]=std::numeric_limits<float>::infinity(); in[1]=std::nanf("");
    in[2]=2; in[3]=-2;
    assert(t.write(104,4,in)); t.read(104,4,out);
    assert(out[0]==0 && out[1]==0 && out[2]==1 && out[3]==-1);
    assert(!t.write(UINT64_MAX,4,in));
    assert(!t.write(200,Timeline::maxFrames+1,in));
    assert(!t.write(200,4,nullptr));
    std::vector<float> block(Timeline::maxFrames*2,.25f);
    for(uint64_t pos=1000;pos<1000+Timeline::capacity+Timeline::maxFrames;pos+=Timeline::maxFrames)
        assert(t.write(pos,Timeline::maxFrames,block.data()));
    t.read(100,4,out); for(float x:out) assert(x==0); // overwritten, never old ring data
    t.stop(); assert(t.start());
    // Concurrent non-consuming readers and producer, fixed bounded loops.
    std::atomic<bool> go{false};
    auto reader=[&] { float samples[32]; while(!go.load()) std::this_thread::yield();
        for(uint64_t i=0;i<3000;++i) { t.read(i*16,16,samples);
            for(float x:samples) assert(x==0 || x==.25f); } };
    std::thread a(reader),b(reader); go=true;
    for(uint64_t i=0;i<3000;++i) assert(t.write(i*16,16,block.data()));
    a.join();b.join();t.stop();
    t.read(2999*16,4,out);for(float x:out)assert(x==0);
}
