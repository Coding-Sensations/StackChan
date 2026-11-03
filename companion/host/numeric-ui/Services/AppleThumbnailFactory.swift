import Foundation
@available(macOS 11.0, *)
public extension CompanionThumbnailReceiver {
    convenience init(owner: AuthenticatedThumbnailOwner, clock: @escaping () -> Double) {
        self.init(owner:owner,detector:AppleVisionDetector(),clock:clock)
    }
}
