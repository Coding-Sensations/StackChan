import SwiftUI

// Add these types to the existing StackChanPairing target. No second app/entrypoint.
// Construct with existing owner's hooks only after signed extension + versioned
// authenticated channel + physical shutdown receipts are available.
final class WebcamControls: ObservableObject {
    @Published private(set) var on = false
    @Published private(set) var indicator = "Webcam unavailable · integration pending"
    @Published private(set) var ready = false
    @Published private(set) var stopPending = false
    let unavailableReason = "Full-frame authenticated transport, trusted capture clock, approved camera extension and checked device stop are pending."
    private let begin: () -> Bool
    // True only after the existing owner validates its exact pending tuple's
    // checked physical Stop receipt (or no capture session ever existed).
    // Peer status booleans, readiness and local CMIO stop are insufficient.
    private let revoke: () -> Bool
    private var prerequisitesReady = false
    init(begin: @escaping () -> Bool, revoke: @escaping () -> Bool) {
        self.begin = begin; self.revoke = revoke
    }
    func setReadiness(transport: Bool, extensionInstalled: Bool, checkedShutdown: Bool) {
        prerequisitesReady = transport && extensionInstalled && checkedShutdown
        if !prerequisitesReady { ownerLost(); return }
        ready = prerequisitesReady && !stopPending
        if stopPending { indicator = "Webcam Off · device stop pending" }
        else if !on { indicator = "Webcam Off" }
    }
    func explicitWebcamOn() {
        guard ready, !stopPending, !on, begin() else { return }
        on = true; indicator = "Webcam On · waiting for client status"
    }
    private func stopCapture(unavailable: Bool) {
        let heldCapture = on || stopPending
        on = false
        let checkedClosed = revoke()
        // Retain custody independently of `on`: repeated Off and owner loss
        // cannot erase unverified hardware state. Only qualified closure clears.
        if heldCapture { stopPending = !checkedClosed }
        ready = prerequisitesReady && !stopPending
        let prefix = unavailable ? "Webcam unavailable" : "Webcam Off"
        if stopPending { indicator = prefix + " · device stop pending" }
        else if heldCapture { indicator = prefix + " · device stopped" }
        else { indicator = unavailable ? prefix + " · integration pending" : prefix + " · no capture requested" }
    }
    func explicitWebcamOff() { stopCapture(unavailable: false) }
    func ownerLost() {
        // A new current owner must re-establish all prerequisites after closure.
        prerequisitesReady = false
        stopCapture(unavailable: true)
    }
    func sharingStatus(authorizedOwner: Bool, clients: Int, receivingFrames: Bool) {
        guard on else { return }
        guard authorizedOwner else { ownerLost(); return }
        indicator = clients > 0 && receivingFrames ? "Webcam On · sharing with camera client" : "Webcam On · waiting for frames"
    }
    // Reconnect, navigation, camera selection do not call explicitWebcamOn.
}

struct WebcamControlsView: View {
    @ObservedObject var controls: WebcamControls
    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Label(controls.indicator, systemImage: controls.on ? "camera.fill" : (controls.stopPending ? "exclamationmark.triangle.fill" : "camera.slash"))
                .foregroundStyle((controls.on || controls.stopPending) ? Color.orange : Color.secondary)
                .accessibilityLabel(controls.indicator)
            HStack {
                Button("Webcam On") { controls.explicitWebcamOn() }
                    .disabled(!controls.ready || controls.on)
                Button("Webcam Off") { controls.explicitWebcamOff() }
            }
            if !controls.ready { Text(controls.unavailableReason).font(.caption).fixedSize(horizontal: false, vertical: true) }
            Text("A selected camera app can record or send received video.")
                .font(.caption)
        }
    }
}
