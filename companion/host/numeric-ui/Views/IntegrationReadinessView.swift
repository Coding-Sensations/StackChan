import SwiftUI
struct IntegrationReadinessView: View {
    var body: some View {
        DisclosureGroup("HomeLab, Apple Home and apps") {
            VStack(alignment: .leading, spacing: 8) {
                Text("Apple Home / HOOBS: offline room status and TV/LIFX review drafts are prepared. Live status, actions and automations require a verified HomeLab adapter.")
                Text("Siri: the existing HomeLab wrapper opens Companion. Companion can stop the whole audio session, but cannot mute only the microphone. Task and agent handoff remains unavailable; existing Codex, Claude and Hermes sessions must be selected explicitly.")
                Text("Robot source includes IR tools, NFC URL and touch 2048. App Center browses the vendor catalog; installation is disabled. Flipper app compatibility still needs an exact app and hardware review.")
            }.font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true).padding(.top, 8)
        }
    }
}
