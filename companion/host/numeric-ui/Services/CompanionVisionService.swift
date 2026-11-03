import Foundation

/// Existing-app source service. No task, daemon, camera, receiver, or transport created.
/// Caller schedules processLatest on its existing serialized local worker.
public final class CompanionVisionService {
    private let lock = NSRecursiveLock()
    private let policy: LocalVisionPolicy
    private let current: (CameraBinding) -> Bool
    private let clock: () -> Double
    private let intent: (VisionResult) -> Void
    private var binding: CameraBinding?
    // Policy owns the detector. Production convenience initializer supplies AppleVisionDetector.
    public init(detector: LocalLandmarkDetector,
                currentAuthenticatedCameraOwner: @escaping (CameraBinding) -> Bool,
                clock: @escaping () -> Double,
                requestIntent: @escaping (VisionResult) -> Void) {
        self.current=currentAuthenticatedCameraOwner; self.clock=clock; self.intent=requestIntent
        self.policy=LocalVisionPolicy(detector:detector,currentAuthenticatedCameraOwner:currentAuthenticatedCameraOwner,clock:clock)
    }
    private func clear() { binding=nil; policy.disable() }
    public func disable() { lock.lock(); defer { lock.unlock() }; clear() }
    public func optIn(_ owner: CameraBinding, explicitLocalOptIn: Bool) throws {
        lock.lock(); defer { lock.unlock() }; clear()
        try policy.enable(owner,localOptIn:explicitLocalOptIn); binding=owner
    }
    public func submitThumbnail(_ frame: VisionFrame) throws {
        lock.lock(); defer { lock.unlock() }
        do {
            guard binding==frame.binding, current(frame.binding) else { throw VisionFailure.unavailableOwner }
            try policy.push(frame)
        } catch { clear(); throw error }
    }
    public func processLatest() throws -> VisionResult? {
        lock.lock(); defer { lock.unlock() }
        do {
            guard let owner=binding, current(owner) else { throw VisionFailure.unavailableOwner }
            guard let result=try policy.processLatest(owner) else { return nil }
            let now=clock()
            guard now.isFinite, now>=result.capturedAt, now-result.capturedAt<0.5,
                  binding==owner, current(owner) else { throw VisionFailure.unavailableOwner }
            if result.gesture != .unknown {
                // Only a request: dispatcher must revalidate owner and check physical Stop completion.
                // No Listen, capture, network, or closure acknowledgement APIs exist here.
                intent(result)
                // A synchronous checked Stop may revoke authority; don't return a stale summary.
                if binding != owner || !current(owner) { clear(); return nil }
            }
            return result
        } catch { clear(); throw error }
    }
}
