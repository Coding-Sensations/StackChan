import Foundation
import Combine

/// Production adapter: opens the wireless listener through the existing PairingController
/// ("Sync") and reports link up/lost/consent for the epoch it was opened with. It starts no
/// audio itself; physical Listen on the robot stays the only consent source.
final class PairingControllerRobotLink: RobotLinkOpening {
    private weak var controller: PairingController?
    private var cancellable: AnyCancellable?
    private var epoch: UInt64 = 0
    private var report: ((UInt64, RobotLinkEvent) -> Void)?
    private var wasLinked = false, wasConsented = false, localStop = false
    /// Set by the app to forward physical-consent grants and revocations to the lifecycle.
    var onConsentChanged: ((UInt64, Bool) -> Void)?

    func bind(_ controller: PairingController) { self.controller = controller }
    /// Called before this Mac's own Stop/Cancel so the resulting end is attributed to the user.
    func noteLocalStop() { localStop = true }

    func openRobotLink(epoch: UInt64, report: @escaping (UInt64, RobotLinkEvent) -> Void) {
        guard let controller = controller else { report(epoch, .lost(.protocolError)); return }
        self.epoch = epoch; self.report = report; wasLinked = false; wasConsented = false; localStop = false
        controller.refreshAudioRouteAvailability()
        controller.useStackChan()
        // A refused or failed start never produced a live worker, so nothing can report later.
        guard controller.phase.busy else {
            self.report = nil
            report(epoch, .lost(RobotLinkProjection.lossReason(phase: controller.phase, recovery: controller.recovery, localStop: false)))
            return
        }
        cancellable = controller.$phase.combineLatest(controller.$activity)
            .receive(on: DispatchQueue.main)
            .sink { [weak self, weak controller] phase, activity in
                guard let self = self, let controller = controller else { return }
                self.observe(phase: phase, activity: activity, recovery: controller.recovery)
            }
    }

    func observe(phase: PairingPhase, activity: PairingActivity?, recovery: PairingRecovery?) {
        guard let report = report else { return }
        let linked = RobotLinkProjection.linked(phase: phase, activity: activity)
        if linked && !wasLinked { wasLinked = true; report(epoch, .connected) }
        if let consented = RobotLinkProjection.consentTransition(
            phase: phase, activity: activity, previouslyConsented: wasConsented
        ) {
            wasConsented = consented
            onConsentChanged?(epoch, consented)
        }
        if !linked && (wasLinked || phase.terminal) {
            let reason = RobotLinkProjection.lossReason(phase: phase, recovery: recovery, localStop: localStop)
            wasLinked = false; wasConsented = false; self.report = nil; cancellable = nil
            report(epoch, .lost(reason))
        }
    }
}
