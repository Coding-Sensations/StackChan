import Foundation

/// The one deliberate view-open action may wait for the already separate camera
/// authorization/clock/extension gates. It can be consumed once only. This type
/// never creates those gates, a permission, a device session or a CMIO endpoint.
protocol WebcamOpenIntentPipeline: AnyObject {
    /// These aggregate existing permission, adapter, and owner-custody state;
    /// this lifecycle never creates or broadens any of those authorities.
    var readyToStart: Bool { get }
    var hasActiveBinding: Bool { get }
    var hasPendingStop: Bool { get }
    func explicitWebcamOn() -> Bool
    func revoke() -> Bool
}

/// Maps the existing pipeline's authenticated owner, binding and retained-stop
/// state into the open-intent boundary. Constructing this adapter has no start,
/// permission, extension or device side effect.
final class CompanionWebcamOpenIntentAdapter: WebcamOpenIntentPipeline {
    private let pipeline: CompanionWebcamPipeline

    init(pipeline: CompanionWebcamPipeline) { self.pipeline = pipeline }

    var readyToStart: Bool { pipeline.ready }
    var hasActiveBinding: Bool { pipeline.binding != nil }
    var hasPendingStop: Bool { pipeline.stopPending }
    func explicitWebcamOn() -> Bool { pipeline.explicitWebcamOn() }
    func revoke() -> Bool { pipeline.revoke() }
}

struct WebcamOpenViewToken: Equatable {
    fileprivate let generation: UInt64
}

final class WebcamOpenIntentLifecycle {
    private let pipeline: WebcamOpenIntentPipeline
    private var generation: UInt64 = 0
    private var openToken: WebcamOpenViewToken?
    private var intentPending = false
    private var consumed = false

    init(pipeline: WebcamOpenIntentPipeline) { self.pipeline = pipeline }

    /// A repeated open while this exact view is active cannot mint another intent.
    func openView() -> WebcamOpenViewToken? {
        precondition(Thread.isMainThread)
        guard openToken == nil, generation < UInt64.max else { return nil }
        generation += 1
        let token = WebcamOpenViewToken(generation: generation)
        openToken = token
        intentPending = true
        consumed = false
        _ = reconcile(token)
        return token
    }

    /// Call after existing owner/permission/extension state changes. Stale view
    /// callbacks are ignored. Pending intent is consumed only when all pipeline
    /// gates are already ready and prior retained stop custody is clear.
    @discardableResult func ownerStateChanged(for token: WebcamOpenViewToken) -> Bool {
        precondition(Thread.isMainThread)
        return reconcile(token)
    }

    private func reconcile(_ token: WebcamOpenViewToken) -> Bool {
        guard openToken == token, intentPending, !consumed,
              !pipeline.hasPendingStop, !pipeline.hasActiveBinding,
              pipeline.readyToStart else { return false }
        // Consume before crossing into the existing owner; failure is terminal
        // for this open. A later callback cannot turn failure into a retry/start.
        intentPending = false
        consumed = true
        return pipeline.explicitWebcamOn()
    }

    /// Synchronously invalidates delayed open callbacks before requesting stop.
    @discardableResult func closeView(_ token: WebcamOpenViewToken) -> Bool {
        precondition(Thread.isMainThread)
        guard openToken == token else { return false }
        openToken = nil
        intentPending = false
        consumed = true
        return pipeline.revoke()
    }
}
