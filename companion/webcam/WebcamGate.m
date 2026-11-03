#import "WebcamGate.h"
#import <math.h>
@implementation WebcamGate {
    NSString *_binding; uint64_t _generation, _epoch; uint64_t _sequence;
    uint64_t _retiredGeneration, _retiredEpoch; BOOL _seen; NSData *_latest; double _deadline, _captured, _lastClock;
    NSUInteger _clients;
}
- (instancetype)init { if ((self=[super init])) _lastClock=-1; return self; }
- (BOOL)enabled { return _binding != nil; }
- (BOOL)sharing { return self.enabled && _clients>0 && _latest!=nil; }
- (NSUInteger)clients { return _clients; }
- (NSString *)indicator { return self.enabled ? (self.sharing ? @"Webcam On · sharing" : @"Webcam On · waiting for frames") : @"Webcam Off"; }
- (BOOL)validClock:(double)now {
    if (!isfinite(now) || now<0 || now<_lastClock) { [self explicitWebcamOff]; return NO; }
    _lastClock=now; return YES;
}
- (BOOL)explicitWebcamOn:(NSString *)binding generation:(uint64_t)generation epoch:(uint64_t)epoch now:(double)now {
    // A separate owner validates this existing binding and intentional UI event.
    if (![self validClock:now] || self.enabled || !generation || !epoch || binding.length<1 || binding.length>128) return NO;
    if (generation<_retiredGeneration || (generation==_retiredGeneration && epoch<=_retiredEpoch)) return NO;
    _binding=[binding copy]; _generation=generation; _epoch=epoch; _deadline=now+2; _seen=NO; return YES;
}
- (void)explicitWebcamOff { if (_binding) { _retiredGeneration=_generation; _retiredEpoch=_epoch; } _binding=nil; _latest=nil; _deadline=0; _seen=NO; _generation=0; _epoch=0; }
- (void)setClientCount:(NSUInteger)count { _clients=MIN(count,32); if (!count) [self explicitWebcamOff]; }
- (BOOL)admit:(NSData *)bgra binding:(NSString *)binding generation:(uint64_t)generation epoch:(uint64_t)epoch sequence:(uint64_t)sequence captured:(double)captured now:(double)now {
    if (![self validClock:now]) return NO;
    if (now>=_deadline) [self explicitWebcamOff];
    if (!self.enabled || ![_binding isEqual:binding] || generation!=_generation || epoch!=_epoch || (!sequence || (_seen && sequence<=_sequence)) || !isfinite(captured) || captured>now || now-captured>0.25 || bgra.length!=320*240*4) return NO;
    // One immutable frame, replacing the previous frame; never a FIFO.
    _latest=[bgra copy]; _sequence=sequence; _seen=YES; _captured=captured; _deadline=now+2; return YES;
}
- (NSData *)takeLatestAt:(double)now {
    if (![self validClock:now]) return nil;
    if (now>=_deadline) [self explicitWebcamOff];
    if (!_latest || now-_captured>0.25) { _latest=nil; return nil; }
    NSData *value=_latest; _latest=nil; return value;
}
@end
