#include "directional_devices.hpp"
#include <cassert>
#include <initializer_list>
using namespace companion::directional;
enum Fault {
 none,badOwner,badHidden,badDirection,badRate,oppositeStream,badUID,badSize,missing,
 missingPlugin,badPluginClass,badDeviceClass,pluginMismatch,pluginQueryError,
 bundleQueryError,globalQueryError,pluginSize,bundleSize,globalSize
} fault=none;
unsigned translations=0,bundleTranslations=0,pluginTranslations=0,ownerReads=0;
constexpr AudioObjectID pluginID=101;
OSStatus mockRead(AudioObjectID id,const AudioObjectPropertyAddress* a,UInt32 qn,const void* q,UInt32* size,void* data){
    auto copy=[&](auto value)->OSStatus {if(*size<sizeof(value))return kAudioHardwareBadPropertySizeError;*size=sizeof(value);std::memcpy(data,&value,*size);return noErr;};
    auto text=[&](CFStringRef s){CFRetain(s);return copy(s);};
    if(a->mSelector==kAudioHardwarePropertyTranslateUIDToDevice && id==kAudioObjectSystemObject){
        ++translations;assert(id==kAudioObjectSystemObject && qn==sizeof(CFStringRef));
        CFStringRef uid;std::memcpy(&uid,q,sizeof(uid));
        if(fault==globalQueryError)return kAudioHardwareUnspecifiedError;
        const auto status=copy(AudioObjectID(fault==missing?0:(CFEqual(uid,CFSTR("local.mrkillbob.StackChan.hal.feed.v2"))?6:8)));
        if(fault==globalSize)--*size;return status;
    }
    if(a->mSelector==kAudioHardwarePropertyTranslateBundleIDToPlugIn){
        ++bundleTranslations;assert(id==kAudioObjectSystemObject && qn==sizeof(CFStringRef));
        CFStringRef bundle;std::memcpy(&bundle,q,sizeof(bundle));
        assert(CFEqual(bundle,CFSTR("local.mrkillbob.StackChan.DirectionalAudio")));
        if(fault==bundleQueryError)return kAudioHardwareUnspecifiedError;
        const auto status=copy(AudioObjectID(fault==missingPlugin?0:pluginID));
        if(fault==bundleSize)--*size;return status;
    }
    if(a->mSelector==kAudioPlugInPropertyTranslateUIDToDevice && id==pluginID){
        ++pluginTranslations;assert(qn==sizeof(CFStringRef));
        CFStringRef uid;std::memcpy(&uid,q,sizeof(uid));
        if(fault==pluginQueryError)return kAudioHardwareUnspecifiedError;
        const auto status=copy(AudioObjectID(fault==pluginMismatch?999:(CFEqual(uid,CFSTR("local.mrkillbob.StackChan.hal.feed.v2"))?6:8)));
        if(fault==pluginSize)--*size;return status;
    }
    const bool input=id==8;const bool stream=id==7 || id==9;
    switch(a->mSelector){
        case kAudioDevicePropertyDeviceUID:return text(fault==badUID?CFSTR("wrong"):(input?CFSTR("local.mrkillbob.StackChan.hal.return.v2"):CFSTR("local.mrkillbob.StackChan.hal.feed.v2")));
        case kAudioDevicePropertyIsHidden:return copy(UInt32(fault==badHidden?0:1));
        case kAudioDevicePropertyTransportType:return copy(UInt32(kAudioDeviceTransportTypeVirtual));
        // Real public HAL normalizes device Owner to the system object.
        case kAudioObjectPropertyOwner:++ownerReads;return copy(UInt32(kAudioObjectSystemObject));
        case kAudioObjectPropertyClass:
            return copy(UInt32(id==kAudioObjectSystemObject?kAudioSystemObjectClassID:
                (id==pluginID?(fault==badPluginClass?kAudioDeviceClassID:kAudioPlugInClassID):
                 (fault==badDeviceClass?kAudioPlugInClassID:kAudioDeviceClassID))));
        case kAudioPlugInPropertyBundleID:
            assert(id==pluginID);return text(fault==badOwner?CFSTR("other.driver"):CFSTR("local.mrkillbob.StackChan.DirectionalAudio"));
        case kAudioPlugInPropertyDeviceList:
            assert(false && "Hidden devices are not verified through the visibility-filtered plugin DeviceList");return -1;
        case kAudioDevicePropertyStreams:
            if(a->mScope==(input?kAudioObjectPropertyScopeInput:kAudioObjectPropertyScopeOutput))return copy(UInt32(id+1));
            *size=fault==oppositeStream?sizeof(AudioStreamID):0;return noErr;
        case kAudioStreamPropertyDirection:assert(stream);return copy(UInt32(fault==badDirection?!(id==9):(id==9)));
        case kAudioStreamPropertyPhysicalFormat:{
            assert(stream);AudioStreamBasicDescription f{};f.mSampleRate=fault==badRate?44100:48000;
            f.mFormatID=kAudioFormatLinearPCM;f.mFormatFlags=kAudioFormatFlagsNativeFloatPacked;
            f.mBytesPerPacket=f.mBytesPerFrame=8;f.mFramesPerPacket=1;f.mChannelsPerFrame=2;f.mBitsPerChannel=32;
            const auto result=copy(f);if(fault==badSize)--*size;return result;
        }
        default:assert(false);return kAudioHardwareUnknownPropertyError;
    }
}
int main(){
    if(resolve(feed,mockRead)!=6)return 97; // finite regression signal, no crash dialog
    assert(resolve(returned,mockRead)==8);assert(translations==2);
    assert(bundleTranslations==2 && pluginTranslations==2 && ownerReads==0);
    assert(resolve("Pro Tools Audio Bridge 2-A_UID",mockRead)==0);assert(resolve(nullptr,mockRead)==0);assert(translations==2);
    for(auto f:{badOwner,badHidden,badDirection,badRate,oppositeStream,badUID,badSize,missing,
                missingPlugin,badPluginClass,badDeviceClass,pluginMismatch,pluginQueryError,
                bundleQueryError,globalQueryError,pluginSize,bundleSize,globalSize}){
        fault=f;assert(resolve(feed,mockRead)==0);assert(resolve(returned,mockRead)==0);
    }
}
