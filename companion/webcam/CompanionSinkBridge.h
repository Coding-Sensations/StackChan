#import <CoreMediaIO/CoreMediaIO.h>
// Link into existing companion only; not an application or transport server.
// Existing owner must validate device/session/turn authority before these APIs.
@interface CompanionSinkBridge : NSObject
@property(nonatomic, readonly) BOOL enabled;
@property(nonatomic, readonly) NSString *indicator;
- (instancetype)initWithDevice:(CMIODeviceID)device sink:(CMIOStreamID)sink;
- (BOOL)explicitWebcamOn:(NSString *)binding generation:(uint64_t)generation epoch:(uint64_t)epoch;
- (BOOL)submitAuthorizedBGRA:(NSData *)pixels sequence:(uint64_t)sequence capturedAtHostTime:(CMTime)time;
- (OSStatus)explicitWebcamOff;
// Disconnect, expiry, owner loss, app exit all call this; reconnect never calls On.
- (OSStatus)revoke;
@end
