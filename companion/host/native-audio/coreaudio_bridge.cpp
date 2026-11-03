#include "pcm_ring.hpp"
#include "checked_unit.hpp"
#include "directional_devices.hpp"
#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

#define COMPANION_NATIVE_DIAGNOSTICS 1
namespace {
constexpr UInt32 max_frames=2048;
struct Bridge {
    companion::CheckedUnit<AudioUnit> output, input;
    std::atomic<bool> closing{false};
    std::atomic<uint32_t> callbacks{0};
    PcmRing<> robot_to_voice, voice_to_robot;
    std::atomic<uint32_t> epoch{0};
    std::atomic<uint32_t> capture_generation{1};
    std::atomic<bool> armed{false}, playing{false};
    bool arm_allowed=false; // serialized control thread only
    std::atomic<OSStatus> callback_error{noErr};
    std::array<int16_t,max_frames> capture{};
    std::atomic<uint64_t> output_callbacks{0},output_frames{0},output_consumed{0},input_callbacks{0},input_frames{0};
    bool started=false;
    ~Bridge() = default; // Checked-close owns all OS teardown before destruction.

};
struct CallbackReference {
    Bridge& bridge;
    explicit CallbackReference(Bridge& b):bridge(b){bridge.callbacks.fetch_add(1,std::memory_order_acq_rel);}
    ~CallbackReference(){bridge.callbacks.fetch_sub(1,std::memory_order_acq_rel);}
};
struct UnitOps {
    int stop(AudioUnit u) noexcept{return AudioOutputUnitStop(u);}
    int uninitialize(AudioUnit u) noexcept{return AudioUnitUninitialize(u);}
    int dispose(AudioUnit u) noexcept{return AudioComponentInstanceDispose(u);}
};
int close_bridge(Bridge* b, uint32_t* retained) {
    if(!b){if(retained)*retained=0;return 0;}
    b->closing.store(true,std::memory_order_release);b->armed.store(false,std::memory_order_release);b->arm_allowed=false;
    UnitOps ops;
    int input_status=b->input.stop(ops);
    int output_status=b->output.stop(ops); // independent; never short-circuit
    // Do not uninitialize/dispose units while either callback still accesses them.
    const bool callbacks_pending=b->callbacks.load(std::memory_order_acquire)!=0;
    if(!callbacks_pending){
        if(!input_status)input_status=b->input.close(ops);
        if(!output_status)output_status=b->output.close(ops);
    }
    const uint32_t mask=(b->input.handle?1u:0u)|(b->output.handle?2u:0u)|(b->callbacks.load(std::memory_order_acquire)?4u:0u);
    if(retained)*retained=mask;
    if(input_status)return input_status;
    if(output_status)return output_status;
    return mask?-70001:0; // callbacks still in flight: retain Bridge, explicit retry
}
OSStatus render(void* ref, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32 n, AudioBufferList* io) {
    auto& b=*static_cast<Bridge*>(ref);
    CallbackReference callback_reference(b);
    b.output_callbacks.fetch_add(1,std::memory_order_relaxed);b.output_frames.fetch_add(n,std::memory_order_relaxed);
    if(!io || n>max_frames) { b.armed.store(false); b.callback_error.store(kAudio_ParamError); return kAudio_ParamError; }
    // Silence every buffer first; an interruption never exposes prior callback contents.
    for(UInt32 i=0;i<io->mNumberBuffers;++i) if(io->mBuffers[i].mData) std::memset(io->mBuffers[i].mData,0,io->mBuffers[i].mDataByteSize);
    if(io->mNumberBuffers!=1 || !io->mBuffers[0].mData || io->mBuffers[0].mDataByteSize<n*sizeof(int16_t)) {b.armed.store(false);b.callback_error.store(kAudio_ParamError);return kAudio_ParamError;}
    auto e=b.epoch.load(std::memory_order_acquire);
    if(!b.closing.load(std::memory_order_acquire) && b.armed.load(std::memory_order_acquire) && !b.playing.load()) {
        auto consumed=b.robot_to_voice.pull(b.capture_generation.load(),static_cast<int16_t*>(io->mBuffers[0].mData),n);
        b.output_consumed.fetch_add(consumed,std::memory_order_relaxed);
        if(!b.armed.load() || e!=b.epoch.load()) std::memset(io->mBuffers[0].mData,0,n*sizeof(int16_t));
    }
    return noErr;
}
OSStatus capture(void* ref, AudioUnitRenderActionFlags* flags, const AudioTimeStamp* time, UInt32, UInt32 n, AudioBufferList*) {
    auto& b=*static_cast<Bridge*>(ref);
    CallbackReference callback_reference(b);
    b.input_callbacks.fetch_add(1,std::memory_order_relaxed);
    if(n>max_frames) { b.armed.store(false); b.callback_error.store(kAudio_ParamError); return kAudio_ParamError; }
    AudioBufferList list{}; list.mNumberBuffers=1;
    list.mBuffers[0]={1,n*static_cast<UInt32>(sizeof(int16_t)),b.capture.data()};
    auto e=b.epoch.load(std::memory_order_acquire);
    auto armed=b.armed.load(std::memory_order_acquire);
    if(b.closing.load(std::memory_order_acquire))return noErr;
    auto status=AudioUnitRender(b.input.handle,flags,time,1,n,&list);
    if(status) { b.armed.store(false); b.callback_error.store(status); return status; }
    b.input_frames.fetch_add(n,std::memory_order_relaxed);
    if(armed && b.armed.load(std::memory_order_acquire) && e==b.epoch.load()) b.voice_to_robot.push(e,b.capture.data(),n);
    return noErr;
}
AudioDeviceID device(const char* uid) {
    AudioObjectPropertyAddress addr{kAudioHardwarePropertyDevices,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMain};
    UInt32 size=0;
    if(AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,&addr,0,nullptr,&size)) return kAudioObjectUnknown;
    std::vector<AudioDeviceID> ids(size/sizeof(AudioDeviceID));
    if(AudioObjectGetPropertyData(kAudioObjectSystemObject,&addr,0,nullptr,&size,ids.data())) return kAudioObjectUnknown;
    for(auto id:ids) {
        addr={kAudioDevicePropertyDeviceUID,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMain};
        CFStringRef value=nullptr; size=sizeof(value);
        if(AudioObjectGetPropertyData(id,&addr,0,nullptr,&size,&value) || !value) continue;
        char text[256]{}; auto ok=CFStringGetCString(value,text,sizeof(text),kCFStringEncodingUTF8); CFRelease(value);
        if(ok && std::strcmp(uid,text)==0) return id;
    }
    return kAudioObjectUnknown;
}
OSStatus configure(companion::CheckedUnit<AudioUnit>& state,AudioDeviceID id,bool input,Bridge& b) {
    auto& unit=state.handle;
    AudioComponentDescription desc{kAudioUnitType_Output,kAudioUnitSubType_HALOutput,kAudioUnitManufacturer_Apple,0,0};
    auto component=AudioComponentFindNext(nullptr,&desc);
    if(!component) return kAudio_ParamError;
    auto status=AudioComponentInstanceNew(component,&unit); if(status) return status;
    UInt32 on=1,off=0;
    status=AudioUnitSetProperty(unit,kAudioOutputUnitProperty_EnableIO,kAudioUnitScope_Input,1,input?&on:&off,sizeof(on)); if(status) return status;
    status=AudioUnitSetProperty(unit,kAudioOutputUnitProperty_EnableIO,kAudioUnitScope_Output,0,input?&off:&on,sizeof(on)); if(status) return status;
    status=AudioUnitSetProperty(unit,kAudioOutputUnitProperty_CurrentDevice,kAudioUnitScope_Global,0,&id,sizeof(id)); if(status) return status;
    AudioStreamBasicDescription physical{}; UInt32 size=sizeof(physical);
    status=AudioUnitGetProperty(unit,kAudioUnitProperty_StreamFormat,input?kAudioUnitScope_Input:kAudioUnitScope_Output,input?1:0,&physical,&size);
    if(status) return status;
    if(physical.mSampleRate!=48000 || physical.mChannelsPerFrame<1) return kAudioUnitErr_FormatNotSupported;
    AudioStreamBasicDescription mono{};
    mono.mSampleRate=48000; mono.mFormatID=kAudioFormatLinearPCM;
    mono.mFormatFlags=kAudioFormatFlagIsSignedInteger|kAudioFormatFlagIsPacked;
    mono.mBytesPerPacket=2; mono.mFramesPerPacket=1; mono.mBytesPerFrame=2; mono.mChannelsPerFrame=1; mono.mBitsPerChannel=16;
    status=AudioUnitSetProperty(unit,kAudioUnitProperty_StreamFormat,input?kAudioUnitScope_Output:kAudioUnitScope_Input,input?1:0,&mono,sizeof(mono)); if(status) return status;
    // Physical channel one only. All other output channels disabled.
    std::vector<SInt32> map(input?1:physical.mChannelsPerFrame,-1); map[0]=0;
    status=AudioUnitSetProperty(unit,kAudioOutputUnitProperty_ChannelMap,kAudioUnitScope_Output,input?1:0,map.data(),static_cast<UInt32>(map.size()*sizeof(SInt32))); if(status) return status;
    status=AudioUnitSetProperty(unit,kAudioUnitProperty_MaximumFramesPerSlice,kAudioUnitScope_Global,0,&max_frames,sizeof(max_frames)); if(status) return status;
    AURenderCallbackStruct callback{input?capture:render,&b};
    status=AudioUnitSetProperty(unit,input?kAudioOutputUnitProperty_SetInputCallback:kAudioUnitProperty_SetRenderCallback,input?kAudioUnitScope_Global:kAudioUnitScope_Input,0,&callback,sizeof(callback)); if(status) return status;
    state.needs_uninitialize=true; // an attempted initialization may need teardown even on failure
    return AudioUnitInitialize(unit);
}
int open_devices(AudioDeviceID out,AudioDeviceID in,void** result) {
    if(!result)return kAudio_ParamError;*result=nullptr;
    if(out==kAudioObjectUnknown || in==kAudioObjectUnknown || out==in)return kAudio_ParamError;
    auto* b=new Bridge();auto status=configure(b->output,out,false,*b);
    if(!status)status=configure(b->input,in,true,*b);
    if(status){uint32_t retained=0;if(close_bridge(b,&retained)==0)delete b;else *result=b;return status;}
    *result=b;return noErr;
}
}
extern "C" {
// No IO is opened at library load. This explicit call must only be made in an approved physical test.
int sc_open(const char* to_voice,const char* from_voice,void** result) {
    if(!result) return kAudio_ParamError; *result=nullptr;
    // Deliberately restrict prototype IO to the already inventoried virtual buses.
    if(!to_voice || !from_voice || std::strcmp(to_voice,"Pro Tools Audio Bridge 2-A_UID") || std::strcmp(from_voice,"Pro Tools Audio Bridge 2-B_UID")) return kAudio_ParamError;
    return open_devices(device(to_voice),device(from_voice),result);
}
// Additive, explicit opt-in. Never selected by sc_open, defaults, profile, or UID
// fallback. Opening still requires separate physical-test/runtime approval.
int sc_open_directional(void** result) {
    if(!result)return kAudio_ParamError;*result=nullptr;
    return open_devices(companion::directional::resolve(companion::directional::feed),
                        companion::directional::resolve(companion::directional::returned),result);
}
int sc_start(void* ptr) {
    if(!ptr) return kAudio_ParamError; auto& b=*static_cast<Bridge*>(ptr);
    if(b.started || b.closing.load()) return kAudio_ParamError;
    b.output.needs_stop=true;
    auto status=AudioOutputUnitStart(b.output.handle); if(status) return status;
    b.input.needs_stop=true;
    status=AudioOutputUnitStart(b.input.handle);
    if(status)return status; // checked-close retains all attempted start custody
    b.started=true; return noErr;
}
// Control methods and worker methods have one serialized caller. begin/interrupt require a NEW epoch.
int sc_begin(void* ptr,uint32_t epoch) {
    if(!ptr || !epoch) return -1; auto& b=*static_cast<Bridge*>(ptr);
    b.armed.store(false,std::memory_order_release);
    if(b.closing.load() || epoch<=b.epoch.load()) return -1;
    if(b.capture_generation.load()==UINT32_MAX) return -1;
    b.capture_generation.fetch_add(1); b.playing.store(false); b.epoch.store(epoch,std::memory_order_release); b.arm_allowed=true; return 0;
}
int sc_arm(void* ptr,uint32_t epoch) {
    if(!ptr) return -1; auto& b=*static_cast<Bridge*>(ptr);
    if(b.closing.load() || !epoch || epoch!=b.epoch.load() || b.callback_error.load() || !b.arm_allowed) return -1;
    b.armed.store(true,std::memory_order_release); return 0;
}
int sc_playing(void* ptr,uint32_t epoch,int playing) {
    if(!ptr || (playing!=0 && playing!=1)) return -1; auto& b=*static_cast<Bridge*>(ptr);
    if(b.closing.load() || !epoch || epoch!=b.epoch.load()) return -1;
    if(playing) {
        if(b.capture_generation.load()==UINT32_MAX) { b.armed.store(false); b.arm_allowed=false; return -1; }
        b.playing.store(true); b.capture_generation.fetch_add(1);
    }
    else b.playing.store(false);
    return 0;
}
int sc_push(void* ptr,uint32_t epoch,const int16_t* pcm,uint32_t frames) {
    if(!ptr || !pcm || !frames || frames>max_frames) return -1; auto& b=*static_cast<Bridge*>(ptr);
    if(b.closing.load() || !epoch || epoch!=b.epoch.load() || !b.armed.load() || b.playing.load()) return -1;
    auto generation=b.capture_generation.load();
    if(b.playing.load()) return -1;
    return static_cast<int>(b.robot_to_voice.push(generation,pcm,frames));
}
int sc_pull(void* ptr,uint32_t epoch,int16_t* pcm,uint32_t frames) {
    if(!ptr || !pcm || !frames || frames>max_frames) return -1; auto& b=*static_cast<Bridge*>(ptr);
    if(b.closing.load() || !epoch || epoch!=b.epoch.load() || !b.armed.load()) { std::fill(pcm,pcm+frames,0); return -1; }
    return static_cast<int>(b.voice_to_robot.pull(epoch,pcm,frames));
}
int sc_error(void* ptr) { return ptr?static_cast<Bridge*>(ptr)->callback_error.load():-1; }
int sc_mute(void* ptr,uint32_t epoch) {
    if(!ptr) return -1; auto& b=*static_cast<Bridge*>(ptr);
    if(b.closing.load() || !epoch || epoch!=b.epoch.load()) return -1;
    b.armed.store(false,std::memory_order_release); b.arm_allowed=false; return 0;
}
uint64_t sc_counter(void* ptr,int direction,int metric) {
    if(!ptr || (direction!=0 && direction!=1)) return UINT64_MAX;
    auto& b=*static_cast<Bridge*>(ptr); auto& ring=direction?b.voice_to_robot:b.robot_to_voice;
    if(metric==0) return ring.dropped.load();
    if(metric==1) return ring.stale.load();
    if(metric==2) return ring.underrun.load();
    return UINT64_MAX;
}
// Observational counters only. Queue lengths are bounded snapshots.
uint64_t sc_diagnostic(void* ptr,int metric) {
    if(!ptr)return UINT64_MAX;auto& b=*static_cast<Bridge*>(ptr);
    switch(metric){
      case 0:return b.output_callbacks.load();case 1:return b.output_frames.load();
      case 2:return b.output_consumed.load();case 3:return b.input_callbacks.load();
      case 4:return b.input_frames.load();case 5:return b.robot_to_voice.queued();
      case 6:return b.voice_to_robot.queued();default:return UINT64_MAX;
    }
}
// Additive ABI: clears caller handle only after both units disposed and callbacks exited.
int sc_close_checked(void** ptr,uint32_t* retained) {
    if(!ptr)return kAudio_ParamError;
    auto* b=static_cast<Bridge*>(*ptr);
    const int status=close_bridge(b,retained);
    if(!status){delete b;*ptr=nullptr;}
    return status;
}
int sc_close_unit_status(void* ptr,int direction,int phase) {
    if(!ptr || (direction!=0 && direction!=1) || phase<0 || phase>2)return kAudio_ParamError;
    auto& b=*static_cast<Bridge*>(ptr);const auto& u=direction?b.input:b.output;
    return phase==0?u.stop_status:(phase==1?u.uninitialize_status:u.dispose_status);
}
// Legacy symbol retained. Failure deliberately retains allocation; void ABI cannot
// expose custody. Updated callers MUST use sc_close_checked and retain their handle.
void sc_close(void* ptr) { void* owned=ptr;uint32_t retained=0;(void)sc_close_checked(&owned,&retained); }
}
