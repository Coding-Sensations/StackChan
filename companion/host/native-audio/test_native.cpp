// Callback tests with stack-owned buffers; no device IO or AudioUnitRender.
#include "coreaudio_bridge.cpp"
#include <cassert>
int main() {
    Bridge b; int16_t input[]{21,22,23},output[4]{99,99,99,99};
    AudioBufferList list{}; list.mNumberBuffers=1; list.mBuffers[0]={1,sizeof(output),output};
    assert(sc_begin(&b,1)==0); assert(sc_arm(&b,1)==0);
    assert(sc_push(&b,1,input,3)==3);
    assert(render(&b,nullptr,nullptr,0,4,&list)==0);
    assert(output[0]==21 && output[2]==23 && output[3]==0);
    assert(sc_push(&b,1,input,3)==3); assert(sc_playing(&b,1,1)==0);
    assert(render(&b,nullptr,nullptr,0,4,&list)==0);
    for(auto sample:output) assert(sample==0);
    assert(sc_playing(&b,1,0)==0);
    assert(render(&b,nullptr,nullptr,0,4,&list)==0);
    for(auto sample:output) assert(sample==0);
    b.voice_to_robot.push(1,input,3);
    assert(sc_mute(&b,1)==0); assert(sc_arm(&b,1)!=0);
    assert(sc_pull(&b,1,output,4)==-1);
    for(auto sample:output) assert(sample==0);
    assert(sc_begin(&b,2)==0); assert(sc_arm(&b,2)==0);
    assert(sc_pull(&b,2,output,4)==0);
    assert(sc_push(&b,1,input,3)==-1);
    b.callback_error.store(kAudio_ParamError); assert(sc_mute(&b,2)==0);
    assert(sc_begin(&b,3)==0); assert(sc_arm(&b,3)!=0);
    assert(sc_counter(&b,0,2)>0); assert(sc_counter(&b,1,1)==3);
}
