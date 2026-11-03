import Foundation
import CoreMediaIO

// No construction from navigation or reconnect. Configure only from the existing
// owner's separately verified installed-extension stream and transport binding.
final class WebcamBridgeAdapter {
    private var sink: CompanionSinkBridge?
    func attachVerifiedInstalledExtension(device: CMIODeviceID, sinkStream: CMIOStreamID) {
        precondition(Thread.isMainThread)
        _ = sink?.revoke()
        sink = CompanionSinkBridge(device: device, sink: sinkStream)
    }
    func explicitWebcamOn(binding: String, generation: UInt64, epoch: UInt64) -> Bool {
        precondition(Thread.isMainThread)
        return sink?.explicitWebcam(on: binding, generation: generation, epoch: epoch) ?? false
    }
    func submitFromCurrentAuthenticatedOwner(pixels: Data, sequence: UInt64, trustedHostCaptureTime: CMTime) -> Bool {
        precondition(Thread.isMainThread)
        guard pixels.count == 307_200 else { return false }
        return sink?.submitAuthorizedBGRA(pixels, sequence: sequence, capturedAtHostTime: trustedHostCaptureTime) ?? false
    }
    func revokeLocalExtension() -> Bool {
        precondition(Thread.isMainThread)
        // This return value is local extension stop status, never a device receipt.
        return sink?.revoke() == noErr
    }
}
