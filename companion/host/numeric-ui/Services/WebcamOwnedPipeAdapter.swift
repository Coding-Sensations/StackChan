import Foundation

/// Source adapter for a future explicitly admitted exclusive video pipe reader.
/// Construction opens no descriptor, starts no reader and enables no extension.
/// The current process-generation owner supplies `current`; a transport boolean
/// or an untrusted event must never implement it. Existing pipeline authority,
/// capture clock and extension approval remain required for every frame.
/// Construct only after the intended purpose binding is active. An unbound
/// adapter stays inert; it cannot adopt a later binding on the shared pipeline.
final class WebcamOwnedPipeAdapter {
    private let decoder: WebcamWorkerLineDecoder

    init(pipeline: CompanionWebcamPipeline, current: @escaping () -> Bool,
         cleanupPending: @escaping () -> Void) {
        precondition(Thread.isMainThread)
        let originalBinding = pipeline.binding
        let ownsBinding = { originalBinding != nil && pipeline.binding == originalBinding }
        decoder = WebcamWorkerLineDecoder(current: {
                // The owner callback may synchronously change pipeline state.
                // Recheck the full original purpose after calling it as well.
                ownsBinding() && current() && ownsBinding()
            },
            consume: { pipeline.handleExistingWorkerEvent($0) },
            requestStop: {
                // Delayed reads, EOF and ownerLost retire this decoder only;
                // they must not revoke a replacement session's shared pipeline.
                guard ownsBinding() else { return }
                if !pipeline.revoke() { cleanupPending() }
            })
    }

    @discardableResult func acceptOwnedRead(_ bytes: Data) -> Bool { decoder.feed(bytes) }
    @discardableResult func ownedEOF() -> Bool { decoder.finish() }
    func ownerLost() { decoder.revoke() }
}
