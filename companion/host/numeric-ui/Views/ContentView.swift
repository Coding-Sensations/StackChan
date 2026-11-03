import SwiftUI
/// Root: Mira home is always the launch surface. Pairing and the remembered-device controls
/// open only from its button. A live worker/session stays visible until it completes or is
/// cancelled, so hiding the pairing view never leaves pairing or audio running behind Home.
struct ContentView: View {
    @ObservedObject var controller: PairingController
    @ObservedObject var lifecycle: CompanionLifecycle
    var localStop: () -> Void = {}
    @State private var showPairing = false
    var body: some View {
        if !showPairing {
            MiraLandingView(lifecycle: lifecycle, controller: controller, repair: { showPairing = true }, localStop: localStop)
        } else {
            VStack(spacing: 0) {
                PairingContentView(controller: controller)
                if !controller.phase.busy || (controller.persistentMode && controller.phase == .paired && !controller.cleanupPending && controller.cleanupIssues.isEmpty) {
                    Button("Back to Mira") { showPairing = false }.buttonStyle(.link).font(.caption).padding(.bottom, 14)
                }
            }
        }
    }
}
struct PairingContentView: View {
    @ObservedObject var controller: PairingController
    var body: some View {
        VStack(alignment: .leading, spacing: 22) {
            HStack(spacing: 12) {
                Image(systemName: "desktopcomputer.and.arrow.down").font(.title)
                VStack(alignment: .leading, spacing: 3) {
                    Text("StackChan Companion").font(.title2.weight(.semibold))
                    Text(controller.persistentMode ? "Remembered pairing · Manual connection" : (controller.audioApproved ? controller.audioSubtitle : "USB + Wi-Fi · Temporary pairing check")).font(.caption).foregroundStyle(.secondary)
                }
            }
            RememberedDeviceView(controller: controller)
            DisclosureGroup("Temporary pairing diagnostic") {
            if controller.audioApproved && !controller.persistentMode {
                Text(controller.audioDisclosure)
                    .font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            }
            SessionFlowView(stage: controller.stage, readyNow: controller.readyNow)
            VStack(alignment: .leading, spacing: 12) {
                if !controller.persistentMode && controller.canAcknowledgeCall {
                    Button("My original call is open", action: controller.acknowledgeOriginalCall)
                }
                Text(controller.instruction).font(.body).fixedSize(horizontal: false, vertical: true)
                if let code = controller.code {
                    Text(code).font(.system(size: 48, weight: .semibold, design: .monospaced))
                        .frame(maxWidth: .infinity).padding(.vertical, 10).accessibilityLabel("Comparison number " + code.map(String.init).joined(separator: " "))
                } else if [.opening, .staging, .restoring].contains(controller.phase) {
                    ProgressView().controlSize(.small)
                }
                TimelineView(.periodic(from: .now, by: 1)) { _ in
                    if let remaining = controller.secondsRemaining() {
                        Text(controller.activity == .conversation ? "Conversation limit: \(remaining) seconds left" : "Pairing window: \(remaining) seconds left").font(.caption).foregroundStyle(.secondary)
                    }
                }
            }.frame(maxWidth: .infinity, minHeight: 145, alignment: .topLeading)
            if let recovery = controller.recovery {
                Text(recovery.guidance).font(.caption).foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
            }
            HStack {
                Button("Cancel", action: controller.cancel).disabled(!controller.canCancel).keyboardShortcut(.cancelAction)
                Spacer()
                Button(controller.primaryTitle, action: controller.primary).buttonStyle(.borderedProminent).disabled(!controller.canPrimary)
            }
            Text(controller.persistentMode ? "Enrollment saves wireless trust. Use StackChan prepares the listener; physical Listen on the robot remains the audio gate." : (controller.audioApproved ? "Match the number on both screens. The robot’s Pair & authorize mic/speaker button gives physical consent. Use its physical stop to end the session. Nothing is saved as permanent trust." : "Pairing does not grant audio. Audio starts only after separate physical Listen consent."))
                .font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
            }
            IntegrationReadinessView()
        }.padding(26).frame(width: 520)
    }
}
