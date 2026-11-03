import Foundation
import Vision
import CoreGraphics
import ImageIO

/// Uses Apple OS-provided local hand-pose and face models. No camera, file, or network APIs.
/// Calling detect performs inference; this candidate is compiled only, never invoked here.
@available(macOS 11.0, *)
public final class AppleVisionDetector: LocalLandmarkDetector {
    public init() {}
    public func detect(_ frame: VisionFrame) throws -> VisionObservations {
        guard frame.width>0, frame.height>0, frame.width<=160, frame.height<=120,
              frame.rgb.count==frame.width*frame.height*3, (0...3).contains(frame.quarterTurnsClockwise),
              let provider=CGDataProvider(data:frame.rgb as CFData),
              let image=CGImage(width:frame.width,height:frame.height,bitsPerComponent:8,bitsPerPixel:24,
                                bytesPerRow:frame.width*3,space:CGColorSpaceCreateDeviceRGB(),
                                bitmapInfo:CGBitmapInfo(rawValue:0),provider:provider,decode:nil,
                                shouldInterpolate:false,intent:.defaultIntent) else { throw VisionFailure.malformed }
        let upright: [CGImagePropertyOrientation] = [.up,.right,.down,.left]
        let reflected: [CGImagePropertyOrientation] = [.upMirrored,.rightMirrored,.downMirrored,.leftMirrored]
        let orientation=(frame.mirrored ? reflected : upright)[frame.quarterTurnsClockwise]
        let handRequest=VNDetectHumanHandPoseRequest(); handRequest.maximumHandCount=2
        let faceRequest=VNDetectFaceRectanglesRequest()
        let handler=VNImageRequestHandler(cgImage:image,orientation:orientation,options:[:])
        try handler.perform([handRequest,faceRequest])
        let joints: [(String,VNHumanHandPoseObservation.JointName)] = [
            ("wrist",.wrist), ("thumbCMC",.thumbCMC), ("thumbMP",.thumbMP), ("thumbIP",.thumbIP), ("thumbTip",.thumbTip),
            ("indexMCP",.indexMCP), ("indexPIP",.indexPIP), ("indexDIP",.indexDIP), ("indexTip",.indexTip),
            ("middleMCP",.middleMCP), ("middlePIP",.middlePIP), ("middleDIP",.middleDIP), ("middleTip",.middleTip),
            ("ringMCP",.ringMCP), ("ringPIP",.ringPIP), ("ringDIP",.ringDIP), ("ringTip",.ringTip),
            ("littleMCP",.littleMCP), ("littlePIP",.littlePIP), ("littleDIP",.littleDIP), ("littleTip",.littleTip)]
        let hands=try (handRequest.results ?? []).prefix(2).map { observation -> [String:VisionPoint] in
            var points=[String:VisionPoint]()
            for (name,joint) in joints {
                let p=try observation.recognizedPoint(joint)
                points[name]=VisionPoint(x:Double(p.location.x),y:1-Double(p.location.y),confidence:Double(p.confidence))
            }
            return points
        }
        let faces=(faceRequest.results ?? []).prefix(8).map { observation -> VisionRect in
            let r=observation.boundingBox
            return VisionRect(x:Double(r.minX),y:1-Double(r.maxY),width:Double(r.width),height:Double(r.height),confidence:Double(observation.confidence))
        }
        return VisionObservations(hands:hands,faces:faces)
    }
}

@available(macOS 11.0, *)
public extension CompanionVisionService {
    /// Constructs the source service without performing inference or loading model requests.
    /// No model/camera execution until the existing app explicitly opts in, submits a frame,
    /// and invokes processLatest on its worker. Construction itself does not grant authority.
    convenience init(currentAuthenticatedCameraOwner: @escaping (CameraBinding) -> Bool,
                     clock: @escaping () -> Double,
                     requestIntent: @escaping (VisionResult) -> Void) {
        self.init(detector:AppleVisionDetector(),currentAuthenticatedCameraOwner:currentAuthenticatedCameraOwner,
                  clock:clock,requestIntent:requestIntent)
    }
}
