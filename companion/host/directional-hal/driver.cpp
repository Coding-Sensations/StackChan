// Original implementation of the Apple AudioServerPlugIn SDK interface.
// No third-party driver code, IO opening, network, file IPC, or PCM custom property.
#include "timeline.hpp"
#include <CoreAudio/AudioServerPlugIn.h>
#include <CoreFoundation/CFPlugInCOM.h>
#include <mach/mach_time.h>
#include <algorithm>
#include <array>
#include <mutex>

namespace stackchan::hal {
constexpr UInt32 period=16384; // SDK minimum is 10923, unrelated to IO block size.
const CFStringRef bundleID=CFSTR("local.mrkillbob.StackChan.DirectionalAudio");
struct Endpoint {
    AudioObjectID id,stream;
    CFStringRef uid,name;
    bool input,hidden;
    unsigned pair;
};
const std::array<Endpoint,4> endpoints{{
    {2,3,CFSTR("local.mrkillbob.StackChan.hal.microphone.v2"),CFSTR("StackChan Microphone"),true,false,0},
    {4,5,CFSTR("local.mrkillbob.StackChan.hal.speaker.v2"),CFSTR("StackChan Speaker"),false,false,1},
    {6,7,CFSTR("local.mrkillbob.StackChan.hal.feed.v2"),CFSTR("StackChan Internal Feed"),false,true,0},
    {8,9,CFSTR("local.mrkillbob.StackChan.hal.return.v2"),CFSTR("StackChan Internal Return"),true,true,1}
}};
struct Client {
    // Separate 64-bit vacancy sentinel supports every UInt32 client ID, including
    // the host's reserved zero ID. Tokens never reset on slot reuse.
    std::atomic<uint64_t> id{UINT64_MAX},token{0};
};
struct DeviceState {
    std::array<Client,128> clients{};
    std::atomic<UInt32> callbacks{0},running{0};
    std::atomic<bool> enabled{false};
    bool retiring=false; // control mutex only
};
std::array<DeviceState,4> states;
std::array<Timeline,2> timelines;
std::mutex control;
AudioServerPlugInHostRef host=nullptr;
uint64_t anchor=0;
double ticksPerFrame=0;
std::atomic<ULONG> references{1};
extern AudioServerPlugInDriverInterface interface;
AudioServerPlugInDriverInterface* interfacePointer=&interface;
AudioServerPlugInDriverRef driver=&interfacePointer;

int index(AudioObjectID id,bool stream=false){
    for(unsigned i=0;i<endpoints.size();++i)if((stream?endpoints[i].stream:endpoints[i].id)==id)return static_cast<int>(i);
    return -1;
}
bool valid(AudioServerPlugInDriverRef d){return d==driver;}
AudioStreamBasicDescription format(){
    AudioStreamBasicDescription f{};f.mSampleRate=48000;f.mFormatID=kAudioFormatLinearPCM;
    f.mFormatFlags=kAudioFormatFlagsNativeFloatPacked;f.mBytesPerPacket=8;f.mFramesPerPacket=1;
    f.mBytesPerFrame=8;f.mChannelsPerFrame=2;f.mBitsPerChannel=32;return f;
}
// One property dispatcher backs Has/Size/Get, avoiding disagreement in advertisement.
// No allocation or HAL calls; retained CFStrings follow CoreAudio ownership rules.
OSStatus property(AudioObjectID object,const AudioObjectPropertyAddress* a,UInt32 qsize,const void* qualifier,
                  UInt32 size,UInt32* used,void* data,bool sizeOnly){
    if(!a || !used)return kAudioHardwareIllegalOperationError;
    *used=0;
    const int di=index(object),si=index(object,true);
    if(object!=kAudioObjectPlugInObject && di<0 && si<0)return kAudioHardwareBadObjectError;
    if(a->mElement!=kAudioObjectPropertyElementMain)return kAudioHardwareUnknownPropertyError;
    const auto scope=a->mScope;
    if(scope!=kAudioObjectPropertyScopeGlobal && scope!=kAudioObjectPropertyScopeInput && scope!=kAudioObjectPropertyScopeOutput)
        return kAudioHardwareUnknownPropertyError;
    const auto sel=a->mSelector;
    const Endpoint* e=di>=0?&endpoints[di]:(si>=0?&endpoints[si]:nullptr);
    auto bytes=[&](const void* value,UInt32 n)->OSStatus{
        *used=n;if(sizeOnly)return noErr;
        if(size<n || (n && !data))return kAudioHardwareBadPropertySizeError;
        if(n)std::memcpy(data,value,n);return noErr;
    };
    auto value=[&](auto v){return bytes(&v,sizeof(v));};
    auto string=[&](CFStringRef s)->OSStatus{
        const auto result=value(s);if(!result && !sizeOnly)CFRetain(s);return result;
    };
    const bool global=scope==kAudioObjectPropertyScopeGlobal;
    const bool matching=e && (scope==(e->input?kAudioObjectPropertyScopeInput:kAudioObjectPropertyScopeOutput));
    if(sel==kAudioObjectPropertyBaseClass && global)return value(UInt32(kAudioObjectClassID));
    if(sel==kAudioObjectPropertyClass && global)return value(UInt32(di>=0?kAudioDeviceClassID:(si>=0?kAudioStreamClassID:kAudioPlugInClassID)));
    if(sel==kAudioObjectPropertyOwner && global)return value(AudioObjectID(si>=0?e->id:(di>=0?kAudioObjectPlugInObject:kAudioObjectUnknown)));
    if(sel==kAudioObjectPropertyName && global)return string(e?e->name:CFSTR("StackChan Directional Audio"));
    if(sel==kAudioObjectPropertyManufacturer && global)return string(CFSTR("StackChan community"));
    if(sel==kAudioObjectPropertyCustomPropertyInfoList && global)return bytes(nullptr,0);
    if(sel==kAudioObjectPropertyOwnedObjects){
        if(qsize%sizeof(AudioClassID) || qsize>64*sizeof(AudioClassID) || (qsize && !qualifier))return kAudioHardwareBadPropertySizeError;
        const AudioClassID type=di>=0?kAudioStreamClassID:kAudioDeviceClassID;
        bool matches=!qsize;
        for(UInt32 offset=0;offset<qsize;offset+=sizeof(AudioClassID)){
            AudioClassID requested;std::memcpy(&requested,static_cast<const char*>(qualifier)+offset,sizeof(requested));
            if(requested==type || requested==kAudioObjectClassID)matches=true;
        }
        if(matches && object==kAudioObjectPlugInObject && global){const AudioObjectID ids[]={2,4,6,8};return bytes(ids,sizeof(ids));}
        if(matches && di>=0 && (global || matching))return value(e->stream);
        return bytes(nullptr,0);
    }
    if(object==kAudioObjectPlugInObject && global){
        switch(sel){
            case kAudioPlugInPropertyBundleID:return string(bundleID);
            case kAudioPlugInPropertyResourceBundle:return string(CFSTR(""));
            case kAudioPlugInPropertyDeviceList:{const AudioObjectID ids[]={2,4,6,8};return bytes(ids,sizeof(ids));}
            case kAudioPlugInPropertyBoxList:case kAudioPlugInPropertyClockDeviceList:return bytes(nullptr,0);
            case kAudioPlugInPropertyTranslateUIDToDevice:{
                if(!qualifier || qsize!=sizeof(CFStringRef))return kAudioHardwareBadPropertySizeError;
                CFStringRef uid;std::memcpy(&uid,qualifier,sizeof(uid));
                if(!uid || CFGetTypeID(uid)!=CFStringGetTypeID())return kAudioHardwareIllegalOperationError;
                AudioObjectID id=kAudioObjectUnknown;
                for(const auto& endpoint:endpoints)if(CFEqual(uid,endpoint.uid)){id=endpoint.id;break;}
                return value(id);
            }
        }
    }
    if(di>=0){
        switch(sel){
            case kAudioDevicePropertyStreams:if(global || matching)return value(e->stream);return bytes(nullptr,0);
            case kAudioObjectPropertyControlList:return bytes(nullptr,0);
            case kAudioDevicePropertyDeviceUID:if(global)return string(e->uid);break;
            case kAudioDevicePropertyModelUID:if(global)return string(CFSTR("local.mrkillbob.StackChan.hal.model.v2"));break;
            case kAudioDevicePropertyTransportType:if(global)return value(UInt32(kAudioDeviceTransportTypeVirtual));break;
            case kAudioDevicePropertyRelatedDevices:if(global){const AudioObjectID ids[]={2,4,6,8};return bytes(ids,sizeof(ids));}break;
            case kAudioDevicePropertyClockDomain:if(global)return value(UInt32(0x53434832));break;
            case kAudioDevicePropertyDeviceIsAlive:if(global)return value(UInt32(1));break;
            case kAudioDevicePropertyDeviceIsRunning:if(global)return value(states[di].running.load()?UInt32(1):UInt32(0));break;
            case kAudioDevicePropertyIsHidden:if(global)return value(UInt32(e->hidden));break;
            case kAudioDevicePropertyDeviceCanBeDefaultDevice:
                if(!global)return value(UInt32(matching && !e->hidden));break;
            case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
                if(!global)return value(UInt32(matching && !e->hidden && !e->input));break;
            case kAudioDevicePropertyLatency:
                if(!global)return value(UInt32(matching && e->input?Timeline::latency:0));break;
            case kAudioDevicePropertySafetyOffset:if(!global)return value(UInt32(0));break;
            case kAudioDevicePropertyPreferredChannelsForStereo:
                if(matching){const UInt32 channels[]={1,2};return bytes(channels,sizeof(channels));}break;
            case kAudioDevicePropertyNominalSampleRate:if(global)return value(Float64(48000));break;
            case kAudioDevicePropertyAvailableNominalSampleRates:if(global)return value(AudioValueRange{48000,48000});break;
            case kAudioDevicePropertyZeroTimeStampPeriod:if(global)return value(period);break;
            case kAudioDevicePropertyClockAlgorithm:if(global)return value(UInt32(kAudioDeviceClockAlgorithmRaw));break;
            case kAudioDevicePropertyClockIsStable:if(global)return value(UInt32(1));break;
        }
    }
    if(si>=0 && global){
        switch(sel){
            case kAudioStreamPropertyIsActive:return value(UInt32(1));
            case kAudioStreamPropertyDirection:return value(UInt32(e->input));
            case kAudioStreamPropertyTerminalType:return value(UInt32(e->input?kAudioStreamTerminalTypeMicrophone:kAudioStreamTerminalTypeSpeaker));
            case kAudioStreamPropertyStartingChannel:return value(UInt32(1));
            case kAudioStreamPropertyLatency:return value(UInt32(0));
            case kAudioStreamPropertyVirtualFormat:case kAudioStreamPropertyPhysicalFormat:return value(format());
            case kAudioStreamPropertyAvailableVirtualFormats:case kAudioStreamPropertyAvailablePhysicalFormats:
                return value(AudioStreamRangedDescription{format(),{48000,48000}});
        }
    }
    return kAudioHardwareUnknownPropertyError;
}
HRESULT query(void* d,REFIID uuid,LPVOID* result){
    if(!result)return E_POINTER;*result=nullptr;if(d!=driver)return E_NOINTERFACE;
    CFUUIDRef requested=CFUUIDCreateFromUUIDBytes(nullptr,uuid);
    const bool match=CFEqual(requested,IUnknownUUID) || CFEqual(requested,kAudioServerPlugInDriverInterfaceUUID);
    CFRelease(requested);if(!match)return E_NOINTERFACE;
    references.fetch_add(1);*result=driver;return S_OK;
}
ULONG retain(void* d){return d==driver?references.fetch_add(1)+1:0;}
ULONG release(void* d){if(d!=driver)return 0;return references.fetch_sub(1)-1;} // immortal singleton
OSStatus initialize(AudioServerPlugInDriverRef d,AudioServerPlugInHostRef h){
    if(!valid(d) || !h)return kAudioHardwareIllegalOperationError;
    std::lock_guard<std::mutex> lock(control);
    if(host)return host==h?noErr:kAudioHardwareIllegalOperationError;
    mach_timebase_info_data_t timebase{};if(mach_timebase_info(&timebase)!=KERN_SUCCESS)return kAudioHardwareUnspecifiedError;
    ticksPerFrame=1e9/48000.0*timebase.denom/timebase.numer;anchor=mach_absolute_time();host=h;
    return noErr;
}
OSStatus create(AudioServerPlugInDriverRef,CFDictionaryRef,const AudioServerPlugInClientInfo*,AudioObjectID* out){
    if(out)*out=kAudioObjectUnknown;return kAudioHardwareUnsupportedOperationError;
}
OSStatus destroy(AudioServerPlugInDriverRef,AudioObjectID){return kAudioHardwareUnsupportedOperationError;}
OSStatus addClient(AudioServerPlugInDriverRef d,AudioObjectID id,const AudioServerPlugInClientInfo* info){
    const int i=index(id);if(!valid(d) || i<0)return kAudioHardwareBadDeviceError;
    if(!info || !host)return kAudioHardwareIllegalOperationError;
    std::lock_guard<std::mutex> lock(control);auto& s=states[i];
    for(auto& c:s.clients)if(c.id.load()==info->mClientID)return kAudioHardwareIllegalOperationError;
    for(auto& c:s.clients)if(c.id.load()==UINT64_MAX){c.id.store(info->mClientID);return noErr;}
    return kAudioHardwareIllegalOperationError;
}
Client* client(DeviceState& s,UInt32 id){for(auto& c:s.clients)if(c.id.load()==id)return &c;return nullptr;}
void notify(AudioObjectID id){
    if(host){AudioObjectPropertyAddress a{kAudioDevicePropertyDeviceIsRunning,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMain};
        host->PropertiesChanged(host,id,1,&a);}
}
OSStatus stopLocked(int i,Client& c){
    auto& s=states[i];if(!(c.token.load()&1))return kAudioHardwareIllegalOperationError;
    if(!endpoints[i].input)timelines[endpoints[i].pair].authorize(static_cast<uint32_t>(&c-s.clients.data()),0);
    if(s.running.load()==1){
        s.retiring=true;s.enabled.store(false);
        if(!endpoints[i].input)timelines[endpoints[i].pair].stop();
        // Storage is immortal. Retire logically now; a subsequent Start can
        // reopen only after existing callbacks have drained. No host retry,
        // worker thread, waiting, RT lock or callback-time notification needed.
        s.retiring=s.callbacks.load()!=0;
    }
    c.token.fetch_add(1);s.running.fetch_sub(1);return noErr;
}
OSStatus removeClient(AudioServerPlugInDriverRef d,AudioObjectID id,const AudioServerPlugInClientInfo* info){
    const int i=index(id);if(!valid(d) || i<0)return kAudioHardwareBadDeviceError;
    if(!info)return kAudioHardwareIllegalOperationError;
    {std::lock_guard<std::mutex> lock(control);auto* c=client(states[i],info->mClientID);
        if(!c)return kAudioHardwareIllegalOperationError;
        if(c->token.load()&1){const auto status=stopLocked(i,*c);if(status)return status;}c->id.store(UINT64_MAX);}
    notify(id);return noErr;
}
OSStatus change(AudioServerPlugInDriverRef,AudioObjectID,UInt64,void*){return kAudioHardwareUnsupportedOperationError;}
Boolean has(AudioServerPlugInDriverRef d,AudioObjectID id,pid_t,const AudioObjectPropertyAddress* a){
    if(!valid(d))return false;
    // UID translation existence is independent of an absent size-query qualifier.
    if(id==1 && a && a->mSelector==kAudioPlugInPropertyTranslateUIDToDevice && a->mScope==kAudioObjectPropertyScopeGlobal && a->mElement==0)return true;
    UInt32 size;return property(id,a,0,nullptr,0,&size,nullptr,true)==noErr;
}
OSStatus settable(AudioServerPlugInDriverRef d,AudioObjectID id,pid_t pid,const AudioObjectPropertyAddress* a,Boolean* out){
    if(!out)return kAudioHardwareIllegalOperationError;*out=false;
    return has(d,id,pid,a)?noErr:kAudioHardwareUnknownPropertyError;
}
OSStatus getSize(AudioServerPlugInDriverRef d,AudioObjectID id,pid_t,const AudioObjectPropertyAddress* a,UInt32 n,const void* q,UInt32* out){
    if(!valid(d))return kAudioHardwareBadObjectError;
    return property(id,a,n,q,0,out,nullptr,true);
}
OSStatus get(AudioServerPlugInDriverRef d,AudioObjectID id,pid_t,const AudioObjectPropertyAddress* a,UInt32 n,const void* q,UInt32 size,UInt32* used,void* out){
    if(!valid(d))return kAudioHardwareBadObjectError;return property(id,a,n,q,size,used,out,false);
}
OSStatus set(AudioServerPlugInDriverRef,AudioObjectID,pid_t,const AudioObjectPropertyAddress*,UInt32,const void*,UInt32,const void*){
    return kAudioHardwareUnsupportedOperationError; // fixed topology, formats and rate
}
OSStatus start(AudioServerPlugInDriverRef d,AudioObjectID id,UInt32 cid){
    const int i=index(id);if(!valid(d) || i<0)return kAudioHardwareBadDeviceError;
    {std::lock_guard<std::mutex> lock(control);auto& s=states[i];auto* c=client(s,cid);
        if(s.retiring && !s.callbacks.load())s.retiring=false;
        if(!c || (c->token.load()&1) || c->token.load()>=UINT64_MAX-1 || s.retiring || (!s.running.load() && s.callbacks.load()))return kAudioHardwareIllegalOperationError;
        if(!s.running.load() && !endpoints[i].input && !timelines[endpoints[i].pair].start())return kAudioHardwareIllegalOperationError;
        const auto token=c->token.fetch_add(1)+1;
        if(!endpoints[i].input)timelines[endpoints[i].pair].authorize(static_cast<uint32_t>(c-s.clients.data()),token);
        s.running.fetch_add(1);s.enabled.store(true);}
    notify(id);return noErr;
}
OSStatus stop(AudioServerPlugInDriverRef d,AudioObjectID id,UInt32 cid){
    const int i=index(id);if(!valid(d) || i<0)return kAudioHardwareBadDeviceError;
    {std::lock_guard<std::mutex> lock(control);auto* c=client(states[i],cid);
        if(!c)return kAudioHardwareIllegalOperationError;const auto status=stopLocked(i,*c);if(status)return status;}
    notify(id);return noErr;
}
OSStatus zero(AudioServerPlugInDriverRef d,AudioObjectID id,UInt32,Float64* sample,UInt64* time,UInt64* seed){
    if(!valid(d) || index(id)<0)return kAudioHardwareBadDeviceError;
    if(!sample || !time || !seed || !host || ticksPerFrame<=0)return kAudioHardwareIllegalOperationError;
    const auto now=mach_absolute_time();const double frames=double(now-anchor)/ticksPerFrame;
    const auto cycles=std::floor(frames/period);*sample=cycles*period;
    *time=anchor+static_cast<uint64_t>(*sample*ticksPerFrame);*seed=1;return noErr;
}
OSStatus will(AudioServerPlugInDriverRef d,AudioObjectID id,UInt32 cid,UInt32 op,Boolean* yes,Boolean* inPlace){
    const int i=index(id);if(!valid(d) || i<0)return kAudioHardwareBadDeviceError;
    if(!yes || !inPlace)return kAudioHardwareIllegalOperationError;
    if(!client(states[i],cid))return kAudioHardwareIllegalOperationError;
    *yes=op==(endpoints[i].input?kAudioServerPlugInIOOperationReadInput:kAudioServerPlugInIOOperationWriteMix);
    *inPlace=true;return noErr;
}
OSStatus phase(AudioServerPlugInDriverRef d,AudioObjectID id,UInt32 cid,UInt32,UInt32 frames,const AudioServerPlugInIOCycleInfo*){
    const int i=index(id);return valid(d) && i>=0 && client(states[i],cid) && frames<=Timeline::maxFrames?noErr:kAudioHardwareIllegalOperationError;
}
OSStatus io(AudioServerPlugInDriverRef d,AudioObjectID id,AudioObjectID stream,UInt32 cid,UInt32 op,UInt32 frames,
            const AudioServerPlugInIOCycleInfo* cycle,void* buffer,void*){
    const int i=index(id);if(!valid(d) || i<0)return kAudioHardwareBadDeviceError;
    const auto& e=endpoints[i];auto& state=states[i];
    if(stream!=e.stream || !cycle || !buffer || frames>Timeline::maxFrames)return kAudioHardwareIllegalOperationError;
    if(op!=(e.input?kAudioServerPlugInIOOperationReadInput:kAudioServerPlugInIOOperationWriteMix))return kAudioHardwareUnsupportedOperationError;
    struct Reference {DeviceState& s;explicit Reference(DeviceState& v):s(v){s.callbacks.fetch_add(1);}~Reference(){s.callbacks.fetch_sub(1);}} reference(state);
    auto* pcm=static_cast<float*>(buffer);
    if(e.input)std::memset(pcm,0,frames*2*sizeof(float));
    auto* owner=client(state,cid);const auto token=owner?owner->token.load():0;
    if(!owner || !(token&1) || owner->id.load()!=cid)return kAudioHardwareIllegalOperationError;
    if(!state.enabled.load())return noErr;
    const auto& stamp=e.input?cycle->mInputTime:cycle->mOutputTime;
    const double sample=stamp.mSampleTime;
    // Exact integer timestamps only, below the exact-double integer bound.
    if(!(stamp.mFlags&kAudioTimeStampSampleTimeValid) || !std::isfinite(sample) || sample<0 || sample>9007199254740991.0 || sample!=std::floor(sample))
        return kAudioHardwareIllegalOperationError;
    const auto frame=static_cast<uint64_t>(sample);auto& timeline=timelines[e.pair];
    if(e.input){if(frame>=Timeline::latency)timeline.read(frame-Timeline::latency,frames,pcm);}
    else if(!timeline.write(frame,frames,pcm,static_cast<uint32_t>(owner-state.clients.data()),token))return kAudioHardwareIllegalOperationError;
    if(e.input && (!state.enabled.load() || owner->id.load()!=cid || owner->token.load()!=token))std::memset(pcm,0,frames*2*sizeof(float));
    return noErr;
}
AudioServerPlugInDriverInterface interface={nullptr,query,retain,release,initialize,create,destroy,addClient,removeClient,
    change,change,has,settable,getSize,get,set,start,stop,zero,will,phase,io,phase};
}
extern "C" __attribute__((visibility("default"))) void* StackChanAudioFactory(CFAllocatorRef,CFUUIDRef type){
    return type && CFEqual(type,kAudioServerPlugInTypeUUID)?stackchan::hal::driver:nullptr;
}
