import Foundation

/// Production adapter. The existing worker owns the native capture/playback buses and
/// socket; its only supported stop is the atomic cancel + stdin close + terminate
/// escalation in PairingController.shutdown. That call starts immediately and completes
/// true only when the exact worker process exit is observed. No separate mute, capture
/// or playback confirmation exists, so none is claimed. No audio is touched here.
final class WorkerAudioResources: OwnedAudioResources {
    static let shared = WorkerAudioResources()
    private weak var controller: PairingController?
    func bind(_ controller: PairingController) { self.controller = controller }
    func shutdownAndConfirmRelease(_ done: @escaping (Bool) -> Void) {
        guard let controller = controller else { done(true); return }   // no worker was ever bound
        controller.shutdown(done)
    }
}
