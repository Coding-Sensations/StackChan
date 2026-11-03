import SwiftUI

/// One strip, three separate facts: worker-reported robot audio, the Mac bridge, and the call's
/// own mute (which Companion cannot read). Missing worker capture telemetry stays unknown.
struct MicrophoneStripView: View {
    let strip: MicrophoneStrip
    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            ForEach(strip.lines, id: \.self) { Text($0).font(.caption) }
            if strip.robot == .suppressedForPlayback {
                Text("The worker reports capture paused while StackChan plays sound.").font(.caption2).foregroundStyle(.secondary)
            }
        }
        .padding(8).frame(maxWidth: .infinity, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: 6).fill(Color.secondary.opacity(0.12)))
        .accessibilityElement(children: .combine)
    }
}
