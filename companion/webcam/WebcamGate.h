#import <Foundation/Foundation.h>
// Serialized by caller on the existing owner queue. No IO or capture.
@interface WebcamGate : NSObject
@property(nonatomic, readonly) BOOL enabled;
@property(nonatomic, readonly) BOOL sharing;
@property(nonatomic, readonly) NSString *indicator;
@property(nonatomic, readonly) NSUInteger clients;
- (BOOL)explicitWebcamOn:(NSString *)binding generation:(uint64_t)generation epoch:(uint64_t)epoch now:(double)now;
- (void)explicitWebcamOff;
- (void)setClientCount:(NSUInteger)count;
- (BOOL)admit:(NSData *)bgra binding:(NSString *)binding generation:(uint64_t)generation epoch:(uint64_t)epoch sequence:(uint64_t)sequence captured:(double)captured now:(double)now;
- (NSData *)takeLatestAt:(double)now;
@end
