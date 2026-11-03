import SwiftUI

/// Added to the existing app target; no application entry point, camera, transport, or timer.
final class GesturePerceptionControls: ObservableObject {
    @Published private(set) var on=false
    @Published private(set) var ready=false
    @Published private(set) var indicator="Local gestures unavailable · camera authority and trusted clock pending"
    @Published private(set) var proximity="Apparent size Unknown · relative movement Unknown"
    private let receiver: CompanionThumbnailReceiver
    init(receiver: CompanionThumbnailReceiver) { self.receiver=receiver }
    func refreshReadiness() {
        ready=receiver.readiness()
        let unavailableReason=receiver.reason
        if !ready { receiver.disable(); on=false; proximity="Apparent size Unknown · relative movement Unknown" }
        indicator=ready ? (on ? "Local gestures On" : "Local gestures Off") : unavailableReason
    }
    func explicitOn() {
        guard ready, !on else { return }
        do { try receiver.explicitOptIn(true); on=true; indicator=receiver.reason }
        catch { ownerLost(); indicator="Local gestures unavailable · current camera authority or trusted clock missing" }
    }
    func explicitOff() { receiver.disable(); on=false; indicator="Local gestures Off"; proximity="Apparent size Unknown · relative movement Unknown" }
    func ownerLost() { explicitOff(); ready=false; indicator="Local gestures unavailable · owner disconnected or revoked" }
    /// Existing serialized worker publishes only a fresh owner-checked result onto the UI thread.
    /// No inference is triggered by UI creation, refresh, navigation or reconnect.
    func present(_ result: VisionResult?, now: Double) {
        guard on, let result=result, now.isFinite, now>=result.capturedAt, now-result.capturedAt<0.5, receiver.acceptsPresentation(result) else {
            proximity="Apparent size Unknown · relative movement Unknown"; return
        }
        proximity="Apparent size \(result.apparentSize.rawValue) · relative movement \(result.movement.rawValue)"
    }
}
struct GesturePerceptionControlsView: View {
    @ObservedObject var controls: GesturePerceptionControls
    var body: some View {
        VStack(alignment:.leading,spacing:8) {
            Label(controls.indicator,systemImage:controls.on ? "hand.raised.fill" : "hand.raised.slash")
            HStack {
                Button("Local gestures On") { controls.explicitOn() }.disabled(!controls.ready || controls.on)
                Button("Local gestures Off") { controls.explicitOff() }
            }
            Text(controls.proximity).font(.caption)
            Text("Uses enabled camera thumbnails locally. Apparent size depends on pose and framing; it is not a distance measurement.").font(.caption).fixedSize(horizontal:false,vertical:true)
            Text("Gestures request display changes or checked local Stop. Physical Listen is still required for microphone use.").font(.caption).fixedSize(horizontal:false,vertical:true)
        }
    }
}
