#import <CoreMediaIO/CoreMediaIO.h>
#import <Security/Security.h>
#import "WebcamGate.h"
#import "WebcamUInt64.h"
// Embedded target only. Never run this executable during source validation.
static CFStringRef const PacketKey=CFSTR("org.stackchan.webcam.packet.v1");
static double Now(void) { return CMTimeGetSeconds(CMClockGetTime(CMClockGetHostTimeClock())); }
@class WebcamDevice;
@interface WebcamStream : NSObject <CMIOExtensionStreamSource>
@property CMIOExtensionStream *stream;
@property NSArray<CMIOExtensionStreamFormat *> *formats;
@property BOOL sink;
@property BOOL running;
@property BOOL consuming;
@property CMIOExtensionClient *writer;
@property(weak) WebcamDevice *owner;
@end
@interface WebcamDevice : NSObject <CMIOExtensionDeviceSource>
@property CMIOExtensionDevice *device;
@property WebcamStream *output;
@property WebcamStream *input;
@property WebcamGate *gate;
@property dispatch_source_t timer;
@property NSUInteger readers;
- (void)tick;
@end
static BOOL TrustedWriter(CMIOExtensionClient *client) {
    // Packaging must supply the existing companion's actual bundle ID + Team ID.
    // Reject absent/unresolved configuration. No credentials/profiles are read.
    NSDictionary *info=NSBundle.mainBundle.infoDictionary;
    NSString *team=info[@"CompanionTeamID"], *bundle=info[@"CompanionBundleID"];
    NSCharacterSet *safe=[NSCharacterSet characterSetWithCharactersInString:@"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.-"];
    if (!team.length || !bundle.length || [team containsString:@"REPLACE"] || [bundle containsString:@"REPLACE"] || [team rangeOfCharacterFromSet:safe.invertedSet].location!=NSNotFound || [bundle rangeOfCharacterFromSet:safe.invertedSet].location!=NSNotFound) return NO;
    NSString *text=[NSString stringWithFormat:@"anchor apple generic and identifier \"%@\" and certificate leaf[subject.OU] = \"%@\"",bundle,team];
    SecRequirementRef requirement=NULL; SecCodeRef code=NULL;
    OSStatus s=SecRequirementCreateWithString((__bridge CFStringRef)text,0,&requirement);
    if (s==errSecSuccess) s=SecCodeCopyGuestWithAttributes(NULL,(__bridge CFDictionaryRef)@{(__bridge NSString *)kSecGuestAttributePid:@(client.pid)},0,&code);
    if (s==errSecSuccess) s=SecCodeCheckValidity(code,kSecCSStrictValidate,requirement);
    if (code) CFRelease(code); if (requirement) CFRelease(requirement); return s==errSecSuccess;
}
@implementation WebcamStream
- (NSSet *)availableProperties { return [NSSet setWithObjects:CMIOExtensionPropertyStreamActiveFormatIndex,CMIOExtensionPropertyStreamFrameDuration,nil]; }
- (CMIOExtensionStreamProperties *)streamPropertiesForProperties:(NSSet *)properties error:(NSError **)error {
    CMIOExtensionStreamProperties *p=[CMIOExtensionStreamProperties streamPropertiesWithDictionary:@{}]; p.activeFormatIndex=0; p.frameDuration=CFBridgingRelease(CMTimeCopyAsDictionary(CMTimeMake(1,10),kCFAllocatorDefault)); return p;
}
- (BOOL)setStreamProperties:(CMIOExtensionStreamProperties *)p error:(NSError **)error { return p.activeFormatIndex==0 && CMTimeCompare(CMTimeMakeFromDictionary((__bridge CFDictionaryRef)p.frameDuration),CMTimeMake(1,10))==0; }
- (BOOL)authorizedToStartStreamForClient:(CMIOExtensionClient *)client {
    if (!self.sink) return YES;
    if (self.writer || !TrustedWriter(client)) return NO;
    self.writer=client; return YES;
}
- (BOOL)startStreamAndReturnError:(NSError **)error {
    if (self.sink) { if (!self.writer) return NO; self.running=YES; }
    else { if (self.owner.readers>=32) return NO; self.owner.readers++; [self.owner.gate setClientCount:self.owner.readers]; self.running=YES; }
    // Client selection does not call explicitWebcamOn or start physical capture.
    return YES;
}
- (BOOL)stopStreamAndReturnError:(NSError **)error {
    if (self.sink) { self.running=NO; self.writer=nil; [self.owner.gate explicitWebcamOff]; }
    else { if (self.owner.readers) self.owner.readers--; [self.owner.gate setClientCount:self.owner.readers]; self.running=self.owner.readers>0; }
    return YES;
}
@end
@implementation WebcamDevice { CVPixelBufferPoolRef _pool; }
- (instancetype)init {
    if ((self=[super init])) {
        _gate=[WebcamGate new];
        if (CVPixelBufferPoolCreate(kCFAllocatorDefault,NULL,(__bridge CFDictionaryRef)@{(__bridge NSString *)kCVPixelBufferWidthKey:@320,(__bridge NSString *)kCVPixelBufferHeightKey:@240,(__bridge NSString *)kCVPixelBufferPixelFormatTypeKey:@(kCVPixelFormatType_32BGRA),(__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey:@{}},&_pool)!=kCVReturnSuccess) return nil;
        _device=[[CMIOExtensionDevice alloc] initWithLocalizedName:@"StackChan Companion" deviceID:[[NSUUID alloc] initWithUUIDString:@"CF68191E-CE26-4F1D-B862-5196AC63D4C4"] legacyDeviceID:nil source:self];
        CMFormatDescriptionRef format=NULL;
        CMVideoFormatDescriptionCreate(kCFAllocatorDefault,kCVPixelFormatType_32BGRA,320,240,NULL,&format);
        CMIOExtensionStreamFormat *f=[[CMIOExtensionStreamFormat alloc] initWithFormatDescription:format maxFrameDuration:CMTimeMake(1,10) minFrameDuration:CMTimeMake(1,10) validFrameDurations:nil]; CFRelease(format);
        _output=[WebcamStream new]; _output.owner=self; _output.formats=@[f];
        _input=[WebcamStream new]; _input.owner=self; _input.sink=YES; _input.formats=@[f];
        _output.stream=[[CMIOExtensionStream alloc] initWithLocalizedName:@"Companion video" streamID:[[NSUUID alloc] initWithUUIDString:@"78F8047D-A95F-4992-9606-D93ABDE8F6AB"] direction:CMIOExtensionStreamDirectionSource clockType:CMIOExtensionStreamClockTypeHostTime source:_output];
        _input.stream=[[CMIOExtensionStream alloc] initWithLocalizedName:@"Companion authorized input" streamID:[[NSUUID alloc] initWithUUIDString:@"8EBA7F8C-EC1E-45A5-8D92-CA725AC43C4B"] direction:CMIOExtensionStreamDirectionSink clockType:CMIOExtensionStreamClockTypeHostTime source:_input];
        NSError *error=nil; if (![_device addStream:_output.stream error:&error] || ![_device addStream:_input.stream error:&error]) return nil;
        // All callbacks/ticks on the provider's main client queue.
        _timer=dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,0,0,dispatch_get_main_queue());
        dispatch_source_set_timer(_timer,dispatch_time(DISPATCH_TIME_NOW,100000000),100000000,1000000);
        __weak WebcamDevice *weak=self; dispatch_source_set_event_handler(_timer,^{ [weak tick]; }); dispatch_resume(_timer);
    } return self;
}
- (void)dealloc { if(_timer) dispatch_source_cancel(_timer); if(_pool) CFRelease(_pool); }
- (NSSet *)availableProperties { return [NSSet setWithObject:CMIOExtensionPropertyDeviceModel]; }
- (CMIOExtensionDeviceProperties *)devicePropertiesForProperties:(NSSet *)properties error:(NSError **)error { CMIOExtensionDeviceProperties *p=[CMIOExtensionDeviceProperties devicePropertiesWithDictionary:@{}]; p.model=@"StackChan Companion · explicit Webcam On required"; return p; }
- (BOOL)setDeviceProperties:(CMIOExtensionDeviceProperties *)p error:(NSError **)error { return NO; }
- (void)receive:(CMSampleBufferRef)sample from:(CMIOExtensionClient *)writer {
    if (!_input.running || ![_input.writer.clientID isEqual:writer.clientID] || !TrustedWriter(writer)) { [_gate explicitWebcamOff]; return; }
    NSDictionary *p=(__bridge NSDictionary *)CMGetAttachment(sample,PacketKey,NULL);
    if (![p isKindOfClass:NSDictionary.class] || p.count>8) return;
    NSString *kind=p[@"kind"], *binding=p[@"binding"];
    if ([kind isEqual:@"off"]) { [_gate explicitWebcamOff]; return; }
    // Only an already authenticated owner-issued session reaches this signed host sink.
    if (![binding isKindOfClass:NSString.class] || ![p[@"generation"] isKindOfClass:NSNumber.class] || ![p[@"epoch"] isKindOfClass:NSNumber.class]) return;
    uint64_t gen=0, epoch=0;
    if (!WCReadPositiveUInt64(p[@"generation"],&gen) || !WCReadPositiveUInt64(p[@"epoch"],&epoch)) return;
    if ([kind isEqual:@"on"]) { [_gate explicitWebcamOn:binding generation:gen epoch:epoch now:Now()]; return; }
    uint64_t sequence=0;
    if (![kind isEqual:@"frame"] || !WCReadPositiveUInt64(p[@"sequence"],&sequence)) return;
    CVPixelBufferRef pb=CMSampleBufferGetImageBuffer(sample);
    if (!pb || CVPixelBufferGetWidth(pb)!=320 || CVPixelBufferGetHeight(pb)!=240 || CVPixelBufferGetPixelFormatType(pb)!=kCVPixelFormatType_32BGRA || CVPixelBufferIsPlanar(pb)) return;
    if (CVPixelBufferLockBaseAddress(pb,kCVPixelBufferLock_ReadOnly)!=kCVReturnSuccess) return;
    size_t stride=CVPixelBufferGetBytesPerRow(pb); void *base=CVPixelBufferGetBaseAddress(pb);
    if (stride<1280 || stride>4096 || !base) { CVPixelBufferUnlockBaseAddress(pb,kCVPixelBufferLock_ReadOnly); return; }
    NSMutableData *bytes=[NSMutableData dataWithLength:307200];
    for (size_t y=0;y<240;y++) memcpy((uint8_t *)bytes.mutableBytes+y*1280,(uint8_t *)base+y*stride,1280);
    CVPixelBufferUnlockBaseAddress(pb,kCVPixelBufferLock_ReadOnly);
    [_gate admit:bytes binding:binding generation:gen epoch:epoch sequence:sequence captured:CMTimeGetSeconds(CMSampleBufferGetPresentationTimeStamp(sample)) now:Now()];
}
- (void)tick {
    if (_input.running && !_input.consuming && _input.writer) {
        _input.consuming=YES; CMIOExtensionClient *writer=_input.writer;
        __weak WebcamDevice *weak=self;
        [_input.stream consumeSampleBufferFromClient:writer completionHandler:^(CMSampleBufferRef sample,uint64_t seq,CMIOExtensionStreamDiscontinuityFlags discontinuity,BOOL more,NSError *error) {
            (void)discontinuity; (void)more;
            if (sample) CFRetain(sample);
            dispatch_async(dispatch_get_main_queue(),^{ WebcamDevice *strong=weak; if (!strong) { if(sample) CFRelease(sample); return; } strong.input.consuming=NO; if (sample) [strong.input.stream notifyScheduledOutputChanged:[CMIOExtensionScheduledOutput scheduledOutputWithSequenceNumber:seq hostTimeInNanoseconds:(uint64_t)(Now()*1e9)]]; if (error) [strong.gate explicitWebcamOff]; else if(sample) [strong receive:sample from:writer]; if(sample) CFRelease(sample); });
        }];
    }
    NSData *bytes=[_gate takeLatestAt:Now()];
    if (!_output.running) return;
    CVPixelBufferRef pb=NULL;
    if (CVPixelBufferPoolCreatePixelBufferWithAuxAttributes(kCFAllocatorDefault,_pool,(__bridge CFDictionaryRef)@{(__bridge NSString *)kCVPixelBufferPoolAllocationThresholdKey:@3},&pb)!=kCVReturnSuccess) return;
    CVPixelBufferLockBaseAddress(pb,0); size_t stride=CVPixelBufferGetBytesPerRow(pb); uint8_t *base=CVPixelBufferGetBaseAddress(pb);
    // Off/unavailable slate is solid dark gray; no last captured frame is reused.
    for (size_t y=0;y<240;y++) { if(bytes) memcpy(base+y*stride,(uint8_t *)bytes.bytes+y*1280,1280); else for(size_t x=0;x<320;x++) { uint8_t *v=base+y*stride+x*4; v[0]=v[1]=v[2]=32; v[3]=255; } }
    CVPixelBufferUnlockBaseAddress(pb,0);
    CMVideoFormatDescriptionRef fmt=NULL; CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault,pb,&fmt);
    CMSampleTimingInfo timing={CMTimeMake(1,10),CMClockGetTime(CMClockGetHostTimeClock()),kCMTimeInvalid}; CMSampleBufferRef out=NULL;
    if (CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault,pb,fmt,&timing,&out)==noErr) { [_output.stream sendSampleBuffer:out discontinuity:0 hostTimeInNanoseconds:(uint64_t)(Now()*1e9)]; CFRelease(out); }
    if(fmt) CFRelease(fmt); CFRelease(pb);
}
@end
@interface WebcamProvider : NSObject <CMIOExtensionProviderSource>
@property CMIOExtensionProvider *provider;
@property WebcamDevice *camera;
@end
@implementation WebcamProvider
- (instancetype)init { if ((self=[super init])) { _camera=[WebcamDevice new]; if (!_camera) return nil; _provider=[[CMIOExtensionProvider alloc] initWithSource:self clientQueue:dispatch_get_main_queue()]; NSError *e=nil; if (![_provider addDevice:_camera.device error:&e]) return nil; } return self; }
- (BOOL)connectClient:(CMIOExtensionClient *)client error:(NSError **)error { return YES; }
- (void)disconnectClient:(CMIOExtensionClient *)client { if ([_camera.input.writer.clientID isEqual:client.clientID]) { _camera.input.writer=nil; _camera.input.running=NO; [_camera.gate explicitWebcamOff]; } }
- (NSSet *)availableProperties { return [NSSet setWithObject:CMIOExtensionPropertyProviderManufacturer]; }
- (CMIOExtensionProviderProperties *)providerPropertiesForProperties:(NSSet *)p error:(NSError **)e { CMIOExtensionProviderProperties *v=[CMIOExtensionProviderProperties providerPropertiesWithDictionary:@{}]; v.manufacturer=@"StackChan Companion"; return v; }
- (BOOL)setProviderProperties:(CMIOExtensionProviderProperties *)p error:(NSError **)e { return NO; }
@end
int main(void) { @autoreleasepool { WebcamProvider *p=[WebcamProvider new]; if (!p) return 1; [CMIOExtensionProvider startServiceWithProvider:p.provider]; CFRunLoopRun(); } return 0; }
