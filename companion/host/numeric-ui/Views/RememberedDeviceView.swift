import SwiftUI
// Explicit wrapper alias avoids SDK27 State macro subprocesses in source-only builds.
private typealias CompanionViewState<Value> = SwiftUI.State<Value>
struct RememberedDeviceView: View {
    @ObservedObject var controller: PairingController
    @CompanionViewState private var deviceFeedback = ""
    var body: some View {
        GroupBox("Sync · Saved Mac pairing") {
            VStack(alignment: .leading, spacing: 8) {
                Text("Saved wireless trust is reused. The existing StackChan aliases appear in both Input and Output; dedicated input-only and output-only devices are not available yet. Sync still prepares the existing internal audio route. Physical Listen remains required.").font(.caption).fixedSize(horizontal: false, vertical: true)
                HStack(spacing: 8) {
                    Image(systemName: controller.audioRouteEndpointsAvailable ? "checkmark.circle.fill" : "exclamationmark.circle")
                        .foregroundStyle(controller.audioRouteEndpointsAvailable ? .green : .secondary)
                    Text(controller.audioRouteEndpointsAvailable ? "Both internal bridge buses are available" : "CoreAudio Bridge 2-A/2-B not found")
                        .font(.caption)
                }
                Button("Check audio device directions") {
                    do {
                        _ = try StackChanAudioDevices.ensure()
                        deviceFeedback = "Input-only StackChan Microphone and output-only StackChan Speaker verified. Sync and physical Listen are still required."
                    } catch {
                        deviceFeedback = "Directional StackChan devices are unavailable: \(error). Existing Sync transport is unchanged; duplex aliases are not a direction fix."
                    }
                    controller.refreshAudioRouteAvailability()
                }.disabled(controller.activity != nil || controller.cleanupPending)
                if !deviceFeedback.isEmpty {
                    Text(deviceFeedback).font(.caption).fixedSize(horizontal: false, vertical: true)
                }
                if controller.persistentMode, let code = controller.code {
                    Text(code).font(.system(size: 36, weight: .semibold, design: .monospaced))
                        .accessibilityLabel("Comparison number " + code.map(String.init).joined(separator: " "))
                }
                if controller.persistentMode, let recovery = controller.recovery {
                    Text(recovery.guidance).font(.caption).fixedSize(horizontal: false, vertical: true)
                }
                HStack {
                    Button("Enroll", action: controller.enrollRemembered).disabled(!controller.canEnrollRemembered)
                    Button("Confirm match", action: controller.confirmRemembered).disabled(!controller.persistentMode || controller.phase != .comparing || controller.confirmationSent)
                    Button("Sync", action: controller.useStackChan).disabled(!controller.canUseStackChan)
                    Button("Forget on Mac", action: controller.forgetRemembered).disabled(!controller.canForgetRemembered)
                }
                if controller.persistentMode && controller.activity != nil {
                    Text("Listener stays ready for reconnection. Disconnect closes the listener.").font(.caption).fixedSize(horizontal: false, vertical: true)
                }
                if controller.persistentMode {
                    Button(controller.activity == nil ? "Cancel" : "Disconnect", action: controller.cancel)
                        .disabled(!controller.canCancel)
                }
                GroupBox("Companion camera") {
                    VStack(alignment: .leading, spacing: 6) {
                        Text(controller.cameraPermissionFeedback).font(.caption).fixedSize(horizontal: false, vertical: true)
                        HStack {
                            Button("Request webcam permission") { controller.explicitCameraPermission(.webcam) }
                                .disabled(controller.activity == nil || controller.cleanupPending)
                            Button("Request gesture permission") { controller.explicitCameraPermission(.gestures) }
                                .disabled(controller.activity == nil || controller.cleanupPending)
                            Button("Camera permission Off") { controller.revokeCameraPermission(requestID: UUID().uuidString) }
                        }
                        Text("Confirm this purpose on StackChan. Capture stays unavailable until the device is qualified.").font(.caption)
                        WebcamControlsView(controls: controller.webcam)
                        GesturePerceptionControlsView(controls: controller.gestures)
                    }.padding(.vertical, 4)
                }
                Text(controller.rememberedStatus).font(.caption).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                if controller.phase == .paired && controller.activity == nil { Text("No microphone capture starts during enrollment. Sync is a separate action; physical Listen remains required.").font(.caption) }
            }.padding(.top, 8)
        }.onAppear { controller.refreshAudioRouteAvailability() }
    }
}
