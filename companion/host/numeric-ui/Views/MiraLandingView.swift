import SwiftUI
import AppKit

/// Daily home. No diagnostics or listener. Pairing is a deliberate button here. The
/// character is the handoff-approved Mira frame (mira-idle-frame.png, SHA-256
/// 93432bf8c86216c9c164d026ab96568cfd966a760a55e12da165af5a35e4ce79), bundled unmodified.
struct MiraLandingView: View {
    @ObservedObject var lifecycle: CompanionLifecycle
    @ObservedObject var controller: PairingController
    private var session: MiraHomeSession {
        MiraHomeSession(phase: controller.phase, cleanupUnconfirmed: controller.cleanupPending || !controller.cleanupIssues.isEmpty || controller.stage == .restoring, persistentMode: controller.persistentMode)
    }
    var repair: () -> Void
    var localStop: () -> Void = {}
    private static let miraImage: NSImage? = Bundle.main.url(forResource: "mira-idle-frame", withExtension: "png").flatMap { NSImage(contentsOf: $0) }
    private var pairTitle: String {
        switch lifecycle.presence {
        case .needsPairing, .repairRequired, .trustUnknown: return "Pair with Mira…"
        default: return "Pair again…"
        }
    }
    var body: some View {
        VStack(spacing: 16) {
            if let image = Self.miraImage {
                Image(nsImage: image).resizable().scaledToFit().frame(maxHeight: 220).accessibilityLabel("Mira")
            }
            Text("Mira").font(.largeTitle.weight(.semibold))
            HStack(spacing: 8) {
                Circle().fill(lifecycle.presence.showsActiveCall ? Color.green : Color.secondary).frame(width: 10, height: 10)
                Text(status).font(.headline)
            }
            Text(detail).font(.callout).foregroundStyle(.secondary).multilineTextAlignment(.center).fixedSize(horizontal: false, vertical: true)
            if lifecycle.canOpen { Button("Connect to Mira") { lifecycle.open() }.buttonStyle(.borderedProminent) }
            if case .cleanupIncomplete = lifecycle.presence {
                Button("Retry audio worker release") { lifecycle.stopAudioSession() }.buttonStyle(.bordered)
            }
            if lifecycle.presence.isLive {
                MicrophoneStripView(strip: lifecycle.micStrip)
                if lifecycle.robotLinked {
                    Button("Stop audio session") { localStop(); lifecycle.stopAudioSession() }
                        .buttonStyle(.bordered)
                    Text("This stops the whole Companion audio worker; microphone-only mute is not available. A call in your call app may continue.")
                        .font(.caption2).foregroundStyle(.secondary).multilineTextAlignment(.center)
                    if !lifecycle.physicalConsent { Text("Waiting for the robot’s physical consent.").font(.caption).foregroundStyle(.secondary) }
                    if lifecycle.call == .notAttached {
                        Button("My Mira call is running on this Mac") { lifecycle.attestCall(running: true) }
                        Text("Start or join the call in your call app and pick StackChan Microphone/Speaker there. This only records your statement.").font(.caption2).foregroundStyle(.secondary).multilineTextAlignment(.center)
                    } else if lifecycle.call == .userAsserted {
                        Button("My call has ended") { lifecycle.attestCall(running: false) }.buttonStyle(.link).font(.caption)
                    }
                }
            }
            if session.pairedThisSession {
                Text("Paired for this session only. Nothing is remembered, and the call and speaker are not verified.").font(.caption).foregroundStyle(.secondary).multilineTextAlignment(.center).fixedSize(horizontal: false, vertical: true)
            }
            if session.showsControls {
                VStack(spacing: 10) {
                    Text(controller.instruction).font(.callout).fixedSize(horizontal: false, vertical: true)
                    TimelineView(.periodic(from: .now, by: 1)) { _ in
                        if let remaining = controller.secondsRemaining() {
                            Text("Session limit: \(remaining) seconds left").font(.caption)
                        }
                    }
                    if !lifecycle.robotLinked {
                        Button("Stop session") { localStop(); controller.cancel() }
                            .disabled(!controller.canCancel || lifecycle.presence == .closing)
                    }
                    Text("The robot’s physical stop also ends the session.").font(.caption).foregroundStyle(.secondary)
                }
            }
            if session.canOpenSessionControls {
                Button("Session and camera controls…", action: repair).buttonStyle(.bordered)
                Text("Opening controls keeps this connection active.").font(.caption2).foregroundStyle(.secondary)
            }
            Button(pairTitle, action: repair).buttonStyle(.link).font(.caption)
                .disabled(session.showsControls || lifecycle.presence.isLive || lifecycle.presence == .closing)
        }
        .padding(26).frame(minWidth: 420, maxWidth: 420, minHeight: 260)
        .onAppear { lifecycle.refreshTrust() }
        .onChange(of: controller.phase) { _, _ in lifecycle.refreshTrust() }
    }
    private var status: String {
        MiraPresentation.homeStatus(lifecycle.presence, session: session)
    }
    private var detail: String {
        MiraPresentation.homeDetail(lifecycle.presence, session: session, workerActivity: controller.activity,
                                    closeRecord: lifecycle.closeRecord, lastFailure: lifecycle.lastFailure)
    }
}
