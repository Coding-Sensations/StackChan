// Statically linked mock host: never loads a driver or calls CoreAudio device IO.
#include "../driver.cpp"
#include <cassert>
#include <iostream>
using namespace stackchan::hal;
unsigned notices=0;
OSStatus changed(AudioServerPlugInHostRef,AudioObjectID,UInt32 count,const AudioObjectPropertyAddress* a){
    assert(count==1 && a->mSelector==kAudioDevicePropertyDeviceIsRunning);++notices;return noErr;
}
AudioServerPlugInHostInterface mockHost{changed,nullptr,nullptr,nullptr,nullptr};
AudioObjectPropertyAddress address(UInt32 selector,UInt32 scope=kAudioObjectPropertyScopeGlobal){return {selector,scope,kAudioObjectPropertyElementMain};}
template<class T> T prop(AudioObjectID id,UInt32 selector,UInt32 scope=kAudioObjectPropertyScopeGlobal){
    T v{};UInt32 n=0,used=0;auto a=address(selector,scope);
    assert(interface.HasProperty(driver,id,0,&a));
    assert(interface.GetPropertyDataSize(driver,id,0,&a,0,nullptr,&n)==0 && n==sizeof(T));
    assert(interface.GetPropertyData(driver,id,0,&a,0,nullptr,n,&used,&v)==0 && used==n);return v;
}
void empty(AudioObjectID id,UInt32 selector,UInt32 scope=kAudioObjectPropertyScopeGlobal){
    UInt32 n=99,used=99;auto a=address(selector,scope);assert(interface.HasProperty(driver,id,0,&a));
    assert(interface.GetPropertyDataSize(driver,id,0,&a,0,nullptr,&n)==0 && n==0);
    UInt32 unused=0;assert(interface.GetPropertyData(driver,id,0,&a,0,nullptr,0,&used,&unused)==0 && used==0);
}
int main(){
    assert(!StackChanAudioFactory(nullptr,IUnknownUUID));
    assert(StackChanAudioFactory(nullptr,kAudioServerPlugInTypeUUID)==driver);
    assert(interface.Initialize(driver,&mockHost)==0);
    assert(interface.Initialize(driver,&mockHost)==0);
    for(const auto& e:endpoints){
        const auto correct=e.input?kAudioObjectPropertyScopeInput:kAudioObjectPropertyScopeOutput;
        const auto opposite=e.input?kAudioObjectPropertyScopeOutput:kAudioObjectPropertyScopeInput;
        assert(prop<UInt32>(e.id,kAudioObjectPropertyClass)==kAudioDeviceClassID);
        assert(prop<UInt32>(e.id,kAudioDevicePropertyIsHidden)==e.hidden);
        assert(prop<UInt32>(e.id,kAudioDevicePropertyStreams,correct)==e.stream);
        empty(e.id,kAudioDevicePropertyStreams,opposite);
        assert(prop<UInt32>(e.id,kAudioDevicePropertyDeviceCanBeDefaultDevice,correct)==!e.hidden);
        assert(prop<UInt32>(e.id,kAudioDevicePropertyDeviceCanBeDefaultDevice,opposite)==0);
        assert(prop<UInt32>(e.id,kAudioDevicePropertyDeviceCanBeDefaultSystemDevice,opposite)==0);
        assert(prop<UInt32>(e.stream,kAudioStreamPropertyDirection)==e.input);
        const auto f=prop<AudioStreamBasicDescription>(e.stream,kAudioStreamPropertyPhysicalFormat);
        assert(f.mChannelsPerFrame==2 && f.mSampleRate==48000 && f.mBytesPerFrame==8);
        assert(prop<UInt32>(e.id,kAudioDevicePropertyZeroTimeStampPeriod)>=10923);
        assert(prop<UInt32>(e.id,kAudioDevicePropertyClockDomain)==0x53434832);
        assert(prop<UInt32>(e.id,kAudioDevicePropertyLatency,correct)==(e.input?Timeline::latency:0));
        auto uid=prop<CFStringRef>(e.id,kAudioDevicePropertyDeviceUID);assert(CFEqual(uid,e.uid));CFRelease(uid);
        auto a=address(kAudioPlugInPropertyTranslateUIDToDevice);AudioObjectID found=0;UInt32 used=0;
        assert(interface.GetPropertyData(driver,1,0,&a,sizeof(e.uid),&e.uid,sizeof(found),&used,&found)==0 && found==e.id);
        assert(interface.GetPropertyData(driver,1,0,&a,0,nullptr,sizeof(found),&used,&found)!=0);
        Boolean yes=false,inPlace=false;
        AudioServerPlugInClientInfo temporary{0,0,true,nullptr};
        assert(interface.AddDeviceClient(driver,e.id,&temporary)==0);
        assert(interface.StartIO(driver,e.id,0)==0);assert(interface.StopIO(driver,e.id,0)==0);
        const auto op=e.input?kAudioServerPlugInIOOperationReadInput:kAudioServerPlugInIOOperationWriteMix;
        assert(interface.WillDoIOOperation(driver,e.id,0,op,&yes,&inPlace)==0 && yes && inPlace);
        assert(interface.WillDoIOOperation(driver,e.id,0,e.input?kAudioServerPlugInIOOperationWriteMix:kAudioServerPlugInIOOperationReadInput,&yes,&inPlace)==0 && !yes);
        assert(interface.RemoveDeviceClient(driver,e.id,&temporary)==0);
        temporary.mClientID=UINT32_MAX;
        assert(interface.AddDeviceClient(driver,e.id,&temporary)==0);assert(interface.StartIO(driver,e.id,UINT32_MAX)==0);
        assert(interface.StopIO(driver,e.id,UINT32_MAX)==0);assert(interface.RemoveDeviceClient(driver,e.id,&temporary)==0);
        auto bad=address(kAudioDevicePropertyStreams);bad.mElement=9;
        assert(!interface.HasProperty(driver,e.id,0,&bad));
        assert(interface.StartIO(driver,e.id,77)!=0); // must register first
    }
    // Bidirectional transport consists of two isolated one-way pairs.
    AudioServerPlugInClientInfo first{1,0,true,nullptr},second{2,0,true,nullptr};
    for(const auto& e:endpoints){assert(interface.AddDeviceClient(driver,e.id,&first)==0);assert(interface.StartIO(driver,e.id,1)==0);}
    assert(interface.AddDeviceClient(driver,2,&second)==0);assert(interface.StartIO(driver,2,2)==0);
    assert(interface.AddDeviceClient(driver,6,&second)==0);assert(interface.StartIO(driver,6,2)==0);
    AudioServerPlugInIOCycleInfo cycle{};cycle.mOutputTime.mFlags=kAudioTimeStampSampleTimeValid;
    cycle.mInputTime.mFlags=kAudioTimeStampSampleTimeValid;cycle.mOutputTime.mSampleTime=100;
    cycle.mInputTime.mSampleTime=100+Timeline::latency;
    float robot[8]={.1,.2,.3,.4,.5,.6,.7,.8},voice[8]={-.1,-.2,-.3,-.4,-.5,-.6,-.7,-.8},received[8];
    assert(interface.DoIOOperation(driver,6,7,1,kAudioServerPlugInIOOperationWriteMix,4,&cycle,robot,nullptr)==0);
    assert(interface.DoIOOperation(driver,4,5,1,kAudioServerPlugInIOOperationWriteMix,4,&cycle,voice,nullptr)==0);
    for(int reader=1;reader<=2;++reader){
        assert(interface.DoIOOperation(driver,2,3,reader,kAudioServerPlugInIOOperationReadInput,4,&cycle,received,nullptr)==0);
        for(int i=0;i<8;++i)assert(received[i]==robot[i]);
    }
    assert(interface.DoIOOperation(driver,8,9,1,kAudioServerPlugInIOOperationReadInput,4,&cycle,received,nullptr)==0);
    for(int i=0;i<8;++i)assert(received[i]==voice[i]);
    // Stopping one reader or writer must not retire its peers.
    assert(interface.StopIO(driver,2,2)==0);assert(interface.StopIO(driver,6,2)==0);
    assert(interface.DoIOOperation(driver,2,3,1,kAudioServerPlugInIOOperationReadInput,4,&cycle,received,nullptr)==0);
    for(int i=0;i<8;++i)assert(received[i]==robot[i]);
    // Deterministic peer-stop race: pause a slot between invalidation and
    // publication, stop its producer while a second producer remains active.
    assert(interface.StartIO(driver,6,2)==0);
    bool retired=false;states[2].callbacks.store(1);
    assert(!timelines[0].writeObserved(200,4,robot,0,states[2].clients[0].token.load(),[&]{
        if(!retired){assert(interface.StopIO(driver,6,1)==0);retired=true;}
    }));
    states[2].callbacks.store(0);cycle.mInputTime.mSampleTime=200+Timeline::latency;
    assert(interface.DoIOOperation(driver,2,3,1,kAudioServerPlugInIOOperationReadInput,4,&cycle,received,nullptr)==0);
    for(float x:received)assert(x==0); // late publication carries revoked lease
    cycle.mOutputTime.mSampleTime=204;cycle.mInputTime.mSampleTime=204+Timeline::latency;
    assert(interface.DoIOOperation(driver,6,7,2,kAudioServerPlugInIOOperationWriteMix,4,&cycle,robot,nullptr)==0);
    assert(interface.DoIOOperation(driver,2,3,1,kAudioServerPlugInIOOperationReadInput,4,&cycle,received,nullptr)==0);
    for(int i=0;i<8;++i)assert(received[i]==robot[i]); // peer lease still works
    assert(interface.StopIO(driver,6,2)==0);assert(interface.StartIO(driver,6,1)==0);
    // Simulate a known in-flight callback; logical stop succeeds and immediately
    // gates old audio. Restart waits for quiescence without any host Stop retry.
    states[2].callbacks.store(1);
    assert(interface.StopIO(driver,6,1)==0);assert(interface.StartIO(driver,6,2)!=0);
    assert(interface.DoIOOperation(driver,2,3,1,kAudioServerPlugInIOOperationReadInput,4,&cycle,received,nullptr)==0);
    for(float x:received)assert(x==0);
    states[2].callbacks.store(0);
    assert(interface.StartIO(driver,6,1)==0);
    assert(interface.DoIOOperation(driver,2,3,1,kAudioServerPlugInIOOperationReadInput,4,&cycle,received,nullptr)==0);
    for(float x:received)assert(x==0); // prior session not reused
    assert(interface.DoIOOperation(driver,2,3,99,kAudioServerPlugInIOOperationReadInput,4,&cycle,received,nullptr)!=0);
    assert(interface.DoIOOperation(driver,2,3,2,kAudioServerPlugInIOOperationReadInput,4,&cycle,received,nullptr)!=0); // stopped reader
    assert(interface.DoIOOperation(driver,6,7,2,kAudioServerPlugInIOOperationWriteMix,4,&cycle,robot,nullptr)!=0); // stopped writer
    // OwnedObjects filtering must honor requested classes, including base class.
    auto owned=address(kAudioObjectPropertyOwnedObjects);UInt32 ownedBytes=99;AudioClassID requested=kAudioStreamClassID;
    assert(interface.GetPropertyDataSize(driver,1,0,&owned,sizeof(requested),&requested,&ownedBytes)==0 && ownedBytes==0);
    requested=kAudioDeviceClassID;
    assert(interface.GetPropertyDataSize(driver,1,0,&owned,sizeof(requested),&requested,&ownedBytes)==0 && ownedBytes==4*sizeof(AudioObjectID));
    requested=kAudioObjectClassID;
    assert(interface.GetPropertyDataSize(driver,2,0,&owned,sizeof(requested),&requested,&ownedBytes)==0 && ownedBytes==sizeof(AudioObjectID));
    assert(interface.GetPropertyDataSize(driver,2,0,&owned,1,&requested,&ownedBytes)!=0);
    assert(prop<UInt32>(2,kAudioDevicePropertyDeviceCanBeDefaultSystemDevice,kAudioObjectPropertyScopeInput)==0);
    cycle.mInputTime.mSampleTime=std::nan("");
    assert(interface.DoIOOperation(driver,2,3,1,kAudioServerPlugInIOOperationReadInput,4,&cycle,received,nullptr)!=0);
    for(float x:received)assert(x==0);
    assert(interface.DoIOOperation(driver,2,3,1,kAudioServerPlugInIOOperationReadInput,Timeline::maxFrames+1,&cycle,received,nullptr)!=0);
    assert(interface.DoIOOperation(driver,2,7,1,kAudioServerPlugInIOOperationReadInput,4,&cycle,received,nullptr)!=0);
    Float64 previous=0;UInt64 previousTime=0;
    for(const auto& e:endpoints){Float64 sample;UInt64 time,seed;
        assert(interface.GetZeroTimeStamp(driver,e.id,1,&sample,&time,&seed)==0);
        assert(seed==1 && sample>=previous && time>=previousTime && std::fmod(sample,period)==0);
        previous=sample;previousTime=time;
    }
    for(const auto& e:endpoints){assert(interface.StopIO(driver,e.id,1)==0);assert(interface.RemoveDeviceClient(driver,e.id,&first)==0);}
    assert(interface.RemoveDeviceClient(driver,2,&second)==0);assert(interface.RemoveDeviceClient(driver,6,&second)==0);
    assert(notices>0);std::cout<<"HAL mock enumeration, timing, PCM and lifecycle checks passed\n";
}
