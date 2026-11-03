#import <Foundation/Foundation.h>
// Attachments must preserve exact unsigned identity. Never narrow, coerce a
// float, accept Boolean consent as an identity, or interpret signed negatives.
static inline BOOL WCReadPositiveUInt64(id object, uint64_t *value) {
    if (![object isKindOfClass:NSNumber.class] || !value ||
        CFGetTypeID((__bridge CFTypeRef)object)!=CFNumberGetTypeID() ||
        CFNumberIsFloatType((__bridge CFNumberRef)object)) return NO;
    uint64_t parsed=[object unsignedLongLongValue];
    if (!parsed || ![object isEqualToNumber:@(parsed)]) return NO;
    *value=parsed; return YES;
}
