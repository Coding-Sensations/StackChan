#import "CompanionSinkBridge.h"
#import "WebcamGate.h"
static CFStringRef const PacketKey=CFSTR("org.stackchan.webcam.packet.v1");
@implementation CompanionSinkBridge {
 CMIODeviceID _device; CMIOStreamID _sink; CMSimpleQueueRef _queue;
 CVPixelBufferPoolRef _pool; WebcamGate *_gate; NSString *_binding; uint64_t _generation,_epoch;
}
- (instancetype)initWithDevice:(CMIODeviceID)device sink:(CMIOStreamID)sink {
 if ((self=[super init])) { _device=device; _sink=sink; _gate=[WebcamGate new];
 if (CVPixelBufferPoolCreate(kCFAllocatorDefault,NULL,(__bridge CFDictionaryRef)@{(__bridge NSString *)kCVPixelBufferWidthKey:@320,(__bridge NSString *)kCVPixelBufferHeightKey:@240,(__bridge NSString *)kCVPixelBufferPixelFormatTypeKey:@(kCVPixelFormatType_32BGRA),(__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey:@{}},&_pool)!=kCVReturnSuccess) return nil; }
 return self;
}
- (BOOL)enabled { return _gate.enabled; }
- (NSString *)indicator { return self.enabled ? @"Webcam On · host sending; client sharing unverified" : @"Webcam Off · device shutdown receipt required"; }
- (BOOL)enqueue:(NSData *)pixels packet:(NSDictionary *)packet time:(CMTime)time {
 if (!_queue || CMSimpleQueueGetCount(_queue)>=1) return NO;
 CVPixelBufferRef pb=NULL;
 if (CVPixelBufferPoolCreatePixelBufferWithAuxAttributes(kCFAllocatorDefault,_pool,(__bridge CFDictionaryRef)@{(__bridge NSString *)kCVPixelBufferPoolAllocationThresholdKey:@3},&pb)!=kCVReturnSuccess) return NO;
 CVPixelBufferLockBaseAddress(pb,0); size_t stride=CVPixelBufferGetBytesPerRow(pb); void *base=CVPixelBufferGetBaseAddress(pb);
 for(size_t y=0;y<240;y++) { if(pixels) memcpy((uint8_t *)base+y*stride,(uint8_t *)pixels.bytes+y*1280,1280); else memset((uint8_t *)base+y*stride,0,1280); }
 CVPixelBufferUnlockBaseAddress(pb,0);
 CMVideoFormatDescriptionRef fmt=NULL; CMSampleBufferRef sample=NULL;
 CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault,pb,&fmt);
 CMSampleTimingInfo timing={CMTimeMake(1,10),time,kCMTimeInvalid};
 OSStatus status=CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault,pb,fmt,&timing,&sample);
 if(status==noErr) { CMSetAttachment(sample,PacketKey,(__bridge CFTypeRef)packet,kCMAttachmentMode_ShouldPropagate); status=CMSimpleQueueEnqueue(_queue,sample); if(status!=noErr) CFRelease(sample); }
 // On success sink queue owns this +1 buffer; the framework consumes/releases it.
 if(fmt) CFRelease(fmt); CFRelease(pb); return status==noErr;
}
- (BOOL)explicitWebcamOn:(NSString *)binding generation:(uint64_t)generation epoch:(uint64_t)epoch {
 NSAssert(NSThread.isMainThread,@"Serialize UI/owner with CMIO bridge on main queue");
 double now=CMTimeGetSeconds(CMClockGetTime(CMClockGetHostTimeClock()));
 if(![_gate explicitWebcamOn:binding generation:generation epoch:epoch now:now]) return NO;
 _binding=[binding copy]; _generation=generation; _epoch=epoch;
 if(CMIOStreamCopyBufferQueue(_sink,NULL,NULL,&_queue)!=noErr || !_queue || CMIODeviceStartStream(_device,_sink)!=noErr) { [self revoke]; return NO; }
 if(![self enqueue:nil packet:@{@"kind":@"on",@"binding":binding,@"generation":@(generation),@"epoch":@(epoch)} time:CMClockGetTime(CMClockGetHostTimeClock())]) { [self revoke]; return NO; }
 return YES;
}
- (BOOL)submitAuthorizedBGRA:(NSData *)pixels sequence:(uint64_t)sequence capturedAtHostTime:(CMTime)time {
 NSAssert(NSThread.isMainThread,@"Serialize owner with CMIO bridge");
 double now=CMTimeGetSeconds(CMClockGetTime(CMClockGetHostTimeClock()));
 if(![_gate admit:pixels binding:_binding generation:_generation epoch:_epoch sequence:sequence captured:CMTimeGetSeconds(time) now:now]) return NO;
 NSData *latest=[_gate takeLatestAt:now];
 return latest && [self enqueue:latest packet:@{@"kind":@"frame",@"binding":_binding,@"generation":@(_generation),@"epoch":@(_epoch),@"sequence":@(sequence)} time:time];
}
- (OSStatus)explicitWebcamOff { return [self revoke]; }
- (OSStatus)revoke {
 [_gate explicitWebcamOff]; _binding=nil; _generation=_epoch=0;
 // Stop sink is authoritative for local extension revocation; no network ACK implied.
 OSStatus status=_queue ? CMIODeviceStopStream(_device,_sink) : noErr;
 if(_queue) { const void *item=NULL; while((item=CMSimpleQueueDequeue(_queue))) CFRelease(item); CFRelease(_queue); _queue=NULL; }
 return status;
}
- (void)dealloc { [self revoke]; if(_pool) CFRelease(_pool); }
@end
