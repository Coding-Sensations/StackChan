#pragma once
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <cstring>
namespace companion::directional {
inline constexpr const char* feed="local.mrkillbob.StackChan.hal.feed.v2";
inline constexpr const char* returned="local.mrkillbob.StackChan.hal.return.v2";
inline constexpr const char* bundle="local.mrkillbob.StackChan.DirectionalAudio";
using Reader=decltype(&AudioObjectGetPropertyData);
template<class T> bool get(AudioObjectID id,UInt32 selector,T& out,Reader read,UInt32 scope=kAudioObjectPropertyScopeGlobal){
    AudioObjectPropertyAddress address{selector,scope,kAudioObjectPropertyElementMain};UInt32 size=sizeof(out);
    return read(id,&address,0,nullptr,&size,&out)==noErr && size==sizeof(out);
}
inline bool string(AudioObjectID id,UInt32 selector,const char* expected,Reader read){
    CFStringRef value=nullptr;
    const bool ok=get(id,selector,value,read);bool match=false;
    if(ok && value && CFGetTypeID(value)==CFStringGetTypeID()){char text[256]{};match=CFStringGetCString(value,text,sizeof(text),kCFStringEncodingUTF8) && std::strcmp(text,expected)==0;}
    if(value)CFRelease(value);return match;
}
inline AudioObjectID translate(AudioObjectID object,UInt32 selector,const char* text,Reader read){
    CFStringRef qualifier=CFStringCreateWithCString(nullptr,text,kCFStringEncodingUTF8);
    if(!qualifier)return kAudioObjectUnknown;
    AudioObjectPropertyAddress a{selector,kAudioObjectPropertyScopeGlobal,kAudioObjectPropertyElementMain};
    AudioDeviceID id=kAudioObjectUnknown;UInt32 size=sizeof(id);
    const auto status=read(object,&a,sizeof(qualifier),&qualifier,&size,&id);CFRelease(qualifier);
    if(status || size!=sizeof(id) || id==kAudioObjectUnknown)return kAudioObjectUnknown;
    return id;
}
inline AudioDeviceID resolve(const char* uid,Reader read=&AudioObjectGetPropertyData){
    // Resolve both mappings afresh: HAL IDs can change after device migration.
    if(!uid || (std::strcmp(uid,feed) && std::strcmp(uid,returned)))return kAudioObjectUnknown;
    const auto id=translate(kAudioObjectSystemObject,kAudioHardwarePropertyTranslateUIDToDevice,uid,read);
    if(id==kAudioObjectUnknown)return kAudioObjectUnknown;
    const auto plugin=translate(kAudioObjectSystemObject,kAudioHardwarePropertyTranslateBundleIDToPlugIn,bundle,read);
    UInt32 pluginClass=0,deviceClass=0;
    // HAL global device Owner is the system object. Establish provenance through
    // the registered plugin's own UID translation, including hidden endpoints.
    // Plugin DeviceList is visibility-filtered and cannot prove hidden ownership.
    if(plugin==kAudioObjectUnknown ||
       !get(plugin,kAudioObjectPropertyClass,pluginClass,read) || pluginClass!=kAudioPlugInClassID ||
       !string(plugin,kAudioPlugInPropertyBundleID,bundle,read) ||
       translate(plugin,kAudioPlugInPropertyTranslateUIDToDevice,uid,read)!=id ||
       !get(id,kAudioObjectPropertyClass,deviceClass,read) || deviceClass!=kAudioDeviceClassID)return kAudioObjectUnknown;
    const bool input=std::strcmp(uid,returned)==0;
    UInt32 hidden=0,transport=0;
    if(!string(id,kAudioDevicePropertyDeviceUID,uid,read) ||
       !get(id,kAudioDevicePropertyIsHidden,hidden,read) || hidden!=1 ||
       !get(id,kAudioDevicePropertyTransportType,transport,read) || transport!=kAudioDeviceTransportTypeVirtual)return kAudioObjectUnknown;
    AudioStreamID stream=kAudioObjectUnknown;
    if(!get(id,kAudioDevicePropertyStreams,stream,read,input?kAudioObjectPropertyScopeInput:kAudioObjectPropertyScopeOutput) || stream==kAudioObjectUnknown)return kAudioObjectUnknown;
    AudioObjectPropertyAddress opposite{kAudioDevicePropertyStreams,input?kAudioObjectPropertyScopeOutput:kAudioObjectPropertyScopeInput,kAudioObjectPropertyElementMain};
    UInt32 empty=sizeof(AudioStreamID);AudioStreamID unexpected=0;
    if(read(id,&opposite,0,nullptr,&empty,&unexpected)!=noErr || empty!=0)return kAudioObjectUnknown;
    AudioStreamBasicDescription physical{};UInt32 direction=2;
    if(!get(stream,kAudioStreamPropertyDirection,direction,read) || direction!=UInt32(input) ||
       !get(stream,kAudioStreamPropertyPhysicalFormat,physical,read) || physical.mSampleRate!=48000 || physical.mChannelsPerFrame!=2 ||
       physical.mFormatID!=kAudioFormatLinearPCM || physical.mFormatFlags!=kAudioFormatFlagsNativeFloatPacked ||
       physical.mBytesPerFrame!=8 || physical.mBytesPerPacket!=8 || physical.mBitsPerChannel!=32 || physical.mFramesPerPacket!=1)return kAudioObjectUnknown;
    return id;
}
}
