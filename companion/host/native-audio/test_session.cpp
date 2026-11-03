#include "audio_session.hpp"
#include <cassert>
int main() {
    companion::AudioSession s;
    assert(!s.arm(0,true)); assert(s.begin(1,100));
    assert(!s.packet(1,0,10,false,101)); assert(!s.acknowledge(2));
    assert(s.acknowledge(1)); assert(!s.arm(1,false)); assert(s.arm(1,true));
    assert(s.packet(1,0,4096,false,101)); assert(!s.packet(2,1,10,false,101));
    assert(s.playing(1,true)); assert(!s.packet(1,1,10,false,102));
    assert(s.packet(1,0,10,true,102)); assert(s.interrupt(1));
    assert(!s.acknowledge(1)); assert(!s.arm(1,true)); assert(!s.packet(1,1,10,true,103));
    assert(!s.begin(1,104)); assert(s.begin(2,104));
    assert(s.acknowledge(2)); assert(s.arm(2,true));
    assert(!s.packet(2,1,10,true,105)); assert(!s.armed());
    assert(s.begin(3,200)); assert(s.acknowledge(3)); assert(s.arm(3,true));
    assert(s.heartbeat(3,2100)); assert(s.tick(4099)); assert(!s.tick(4100));
    assert(!s.heartbeat(3,4101)); assert(!s.arm(3,true));
    assert(!s.begin(4,UINT64_MAX));
}
