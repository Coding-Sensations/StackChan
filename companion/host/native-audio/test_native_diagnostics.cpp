// Real callbacks and rings with stack buffers; no device IO.
#include "coreaudio_bridge.cpp"
#include <cassert>
#ifndef COMPANION_NATIVE_DIAGNOSTICS
extern "C" uint64_t sc_diagnostic(void*,int){return 0;}
#endif
int main(){
 Bridge invalid;AudioBufferList empty{};
 assert(render(&invalid,nullptr,nullptr,0,4,&empty)==kAudio_ParamError);
 assert(sc_error(&invalid)==kAudio_ParamError); // Existing code lost this failure.
 assert(sc_diagnostic(&invalid,0)==1);
 Bridge b;int16_t pcm[960]{},out[512]{};AudioBufferList list{};
 list.mNumberBuffers=1;list.mBuffers[0]={1,sizeof(out),out};
 assert(sc_begin(&b,1)==0 && sc_arm(&b,1)==0);
 for(int i=0;i<8;++i)assert(sc_push(&b,1,pcm,960)==960);
 assert(sc_diagnostic(&b,5)==7680);
 assert(render(&b,nullptr,nullptr,0,512,&list)==0);
 assert(sc_diagnostic(&b,0)==1 && sc_diagnostic(&b,1)==512 && sc_diagnostic(&b,2)==512);
 assert(sc_push(&b,1,pcm,960)==512); // Preserve actual short result, not generic success.
 assert(sc_counter(&b,0,0)==448);
 assert(sc_diagnostic(&b,5)==7680);
 // Sustained synthetic producer/consumer drain at equal rates never loses PCM.
 Bridge balanced;assert(sc_begin(&balanced,1)==0 && sc_arm(&balanced,1)==0);
 for(int i=0;i<1000;++i){assert(sc_push(&balanced,1,pcm,512)==512);assert(render(&balanced,nullptr,nullptr,0,512,&list)==0);}
 assert(sc_diagnostic(&balanced,0)==1000 && sc_diagnostic(&balanced,2)==512000);
 assert(sc_counter(&balanced,0,0)==0 && sc_diagnostic(&balanced,5)==0);
}
