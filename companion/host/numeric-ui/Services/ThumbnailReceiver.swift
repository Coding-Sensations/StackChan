import Foundation

public enum ThumbnailFailure: Error { case unavailableCameraAuthority, missingTrustedClock, expiredClock, malformed, stale, replay, stopped }

/// Time bounds for an actual producer clock anchor, verified by the existing authenticated
/// camera session's synchronization protocol. Never construct this from frame arrival.
public struct ProducerClockCalibration {
    public let binding: CameraBinding
    public let producerAnchorMilliseconds: UInt64
    public let hostAnchorLower: Double, hostAnchorUpper: Double
    public let validUntil: Double
    public let maximumDriftPerSecond: Double
    public init(binding: CameraBinding, producerAnchorMilliseconds: UInt64,
                hostAnchorLower: Double, hostAnchorUpper: Double, validUntil: Double, maximumDriftPerSecond: Double) {
        self.binding=binding; self.producerAnchorMilliseconds=producerAnchorMilliseconds
        self.hostAnchorLower=hostAnchorLower; self.hostAnchorUpper=hostAnchorUpper; self.validUntil=validUntil
        self.maximumDriftPerSecond=maximumDriftPerSecond
    }
    func validate(binding expected: CameraBinding, now: Double) throws {
        guard binding==expected, [hostAnchorLower,hostAnchorUpper,validUntil,now,maximumDriftPerSecond].allSatisfy({$0.isFinite}),
              maximumDriftPerSecond>=0, maximumDriftPerSecond<=0.01, hostAnchorLower>=0, hostAnchorUpper>=hostAnchorLower, hostAnchorUpper-hostAnchorLower<=0.05,
              validUntil>=hostAnchorUpper, validUntil-hostAnchorLower<=2 else { throw ThumbnailFailure.missingTrustedClock }
        guard now>=hostAnchorUpper, now<validUntil else { throw ThumbnailFailure.expiredClock }
    }
    func map(_ capturedMilliseconds: UInt64, binding expected: CameraBinding, now: Double) throws -> Double {
        try validate(binding:expected,now:now)
        // Difference before conversion preserves full UInt64 counters without overflow/precision narrowing.
        let positive=capturedMilliseconds>=producerAnchorMilliseconds
        let difference=positive ? capturedMilliseconds-producerAnchorMilliseconds : producerAnchorMilliseconds-capturedMilliseconds
        guard difference<=2000 else { throw ThumbnailFailure.stale }
        let delta=(positive ? 1.0 : -1.0)*Double(difference)/1000
        let drift=abs(delta)*maximumDriftPerSecond
        let earliest=hostAnchorLower+delta-drift, latest=hostAnchorUpper+delta+drift
        guard earliest>=0, latest<=now, now-earliest<0.5 else { throw ThumbnailFailure.stale }
        return (earliest+latest)/2
    }
}

/// Supplied by existing authenticated camera owner; audio grant/paired UI is insufficient.
/// This protocol never creates a session, grants capability, or opens a socket/camera.
public protocol AuthenticatedThumbnailOwner: AnyObject {
    func currentBinding() -> CameraBinding?
    func verifiedCalibration(for binding: CameraBinding) -> ProducerClockCalibration?
    func isCurrentCameraOwner(_ binding: CameraBinding) -> Bool
    func requestCheckedLocalStop(_ binding: CameraBinding)
    func requestDisplayAdvance(_ binding: CameraBinding)
}

/// Current remembered worker has only audio admission/activity and no camera/clock protocol.
/// A concrete unavailable binding for the existing app; no fake ready/consent state.
public final class ExistingWorkerThumbnailUnavailable: AuthenticatedThumbnailOwner {
    public init() {}
    public func currentBinding() -> CameraBinding? { nil }
    public func verifiedCalibration(for binding: CameraBinding) -> ProducerClockCalibration? { nil }
    public func isCurrentCameraOwner(_ binding: CameraBinding) -> Bool { false }
    public func requestCheckedLocalStop(_ binding: CameraBinding) {}
    public func requestDisplayAdvance(_ binding: CameraBinding) {}
}

/// LV01 describes pixels only; trusted metadata is delivered separately by the producer owner.
/// Arrival time is never substituted for capture time. No upscaling/full-video conversion.
public struct CapturedThumbnail {
    public let binding: CameraBinding
    public let sequence: UInt64
    public let producerCapturedMilliseconds: UInt64
    public let quarterTurnsClockwise: Int
    public let mirrored: Bool
    public let lv01: Data
    public init(binding: CameraBinding, sequence: UInt64, producerCapturedMilliseconds: UInt64,
                quarterTurnsClockwise: Int, mirrored: Bool, lv01: Data) {
        self.binding=binding; self.sequence=sequence; self.producerCapturedMilliseconds=producerCapturedMilliseconds
        self.quarterTurnsClockwise=quarterTurnsClockwise; self.mirrored=mirrored; self.lv01=lv01
    }
    func frame(capturedAt: Double) throws -> VisionFrame {
        guard lv01.count>=11, lv01.count<=57608, sequence>0, (0...3).contains(quarterTurnsClockwise),
              Array(lv01.prefix(4))==[76,86,48,49] else { throw ThumbnailFailure.malformed }
        let bytes=[UInt8](lv01.prefix(8))
        let width=Int(bytes[4])*256+Int(bytes[5]), height=Int(bytes[6])*256+Int(bytes[7])
        guard width>=1, height>=1, width<=160, height<=120, lv01.count==8+width*height*3 else { throw ThumbnailFailure.malformed }
        return VisionFrame(binding:binding,sequence:sequence,capturedAt:capturedAt,width:width,height:height,
                           rgb:Data(lv01.dropFirst(8)),quarterTurnsClockwise:quarterTurnsClockwise,mirrored:mirrored)
    }
}

/// Serial source receiver bound to existing owner. One pending decoded RGB thumbnail in policy.
public final class CompanionThumbnailReceiver {
    private let lock=NSRecursiveLock()
    private let owner: AuthenticatedThumbnailOwner
    private let clock: () -> Double
    private let detector: LocalLandmarkDetector
    private var service: CompanionVisionService?
    private var binding: CameraBinding?
    private var lastClock: Double?
    public private(set) var reason="Camera authority and trusted producer clock are unavailable in the existing worker."
    public init(owner: AuthenticatedThumbnailOwner, detector: LocalLandmarkDetector, clock: @escaping ()->Double) {
        self.owner=owner; self.detector=detector; self.clock=clock
    }
    private func now() throws -> Double {
        let value=clock()
        guard value.isFinite, value>=0, lastClock==nil || value>=lastClock! else { throw VisionFailure.clock }
        lastClock=value; return value
    }
    public func readiness() -> Bool {
        lock.lock(); defer {lock.unlock()}
        do {
            let n=try now()
            guard let b=owner.currentBinding(), owner.isCurrentCameraOwner(b) else {
                reason="Authenticated camera owner unavailable; audio pairing does not authorize perception."; return false
            }
            guard let calibration=owner.verifiedCalibration(for:b) else {
                reason="Trusted producer clock synchronization unavailable; arrival timestamps are refused."; return false
            }
            try calibration.validate(binding:b,now:n)
            reason="Local perception Off · explicit opt-in required"; return true
        } catch { reason="Trusted producer clock is invalid, expired, or lacks fresh capture bounds."; return false }
    }
    public func explicitOptIn(_ localOptIn: Bool) throws {
        lock.lock(); defer {lock.unlock()}; disable()
        guard localOptIn, readiness(), let b=owner.currentBinding(), owner.isCurrentCameraOwner(b) else { throw ThumbnailFailure.unavailableCameraAuthority }
        let source=owner, monotonic=clock
        let newService=CompanionVisionService(detector:detector,currentAuthenticatedCameraOwner:{ source.isCurrentCameraOwner($0) },clock:clock,requestIntent:{ result in
            guard source.isCurrentCameraOwner(result.binding), let calibration=source.verifiedCalibration(for:result.binding),
                  (try? calibration.validate(binding:result.binding,now:monotonic())) != nil else { return }
            switch result.gesture {
            case .requestCheckedLocalStop: source.requestCheckedLocalStop(result.binding)
            case .requestDisplayAdvance: source.requestDisplayAdvance(result.binding)
            case .unknown: break
            }
        })
        try newService.optIn(b,explicitLocalOptIn:true); binding=b; service=newService
        reason="Local perception On · waiting for trusted thumbnails"
    }
    public func disable() {
        lock.lock(); defer {lock.unlock()}; service?.disable(); service=nil; binding=nil
        reason="Local perception Off"
    }
    public func receive(_ packet: CapturedThumbnail) throws {
        lock.lock(); defer {lock.unlock()}
        do {
            let n=try now()
            guard binding==packet.binding, let service=service, owner.isCurrentCameraOwner(packet.binding) else { throw ThumbnailFailure.unavailableCameraAuthority }
            guard let calibration=owner.verifiedCalibration(for:packet.binding) else { throw ThumbnailFailure.missingTrustedClock }
            let mapped=try calibration.map(packet.producerCapturedMilliseconds,binding:packet.binding,now:n)
            try service.submitThumbnail(packet.frame(capturedAt:mapped))
        } catch { disable(); reason="Local perception stopped · frame authority, clock or format rejected"; throw error }
    }
    public func acceptsPresentation(_ result: VisionResult) -> Bool {
        lock.lock(); defer {lock.unlock()}
        do {
            let n=try now()
            guard binding==result.binding, owner.isCurrentCameraOwner(result.binding),
                  n>=result.capturedAt, n-result.capturedAt<0.5,
                  let calibration=owner.verifiedCalibration(for:result.binding) else { return false }
            try calibration.validate(binding:result.binding,now:n); return true
        } catch { return false }
    }
    public func processLatest() throws -> VisionResult? {
        lock.lock(); defer {lock.unlock()}
        do {
            guard let b=binding, let service=service, owner.isCurrentCameraOwner(b) else { throw ThumbnailFailure.stopped }
            let n=try now()
            guard let calibration=owner.verifiedCalibration(for:b) else { throw ThumbnailFailure.missingTrustedClock }
            // Calibration lease checked without requiring the old anchor itself to remain a fresh frame.
            try calibration.validate(binding:b,now:n)
            let result=try service.processLatest()
            if owner.isCurrentCameraOwner(b) {
                guard let finalCalibration=owner.verifiedCalibration(for:b) else { throw ThumbnailFailure.missingTrustedClock }
                try finalCalibration.validate(binding:b,now:try now())
            }
            if result==nil && !owner.isCurrentCameraOwner(b) { disable() }
            return result
        } catch { disable(); reason="Local perception stopped · current camera authority or clock lost"; throw error }
    }
}
