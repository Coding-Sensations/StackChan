import Foundation

public struct CameraBinding: Equatable {
    public let conversationID: String
    public let turnID: String
    public let generation: UInt64
    public let epoch: UInt64
    public init(conversationID: String, turnID: String, generation: UInt64, epoch: UInt64) throws {
        func valid(_ value: String) -> Bool {
            !value.isEmpty && value.utf8.count <= 128 && value.utf8.allSatisfy {
                (48...57).contains($0) || (65...90).contains($0) || (97...122).contains($0) || [45,46,58,95].contains($0)
            }
        }
        guard valid(conversationID), valid(turnID), generation > 0, epoch > 0 else { throw VisionFailure.malformed }
        self.conversationID = conversationID; self.turnID = turnID
        self.generation = generation; self.epoch = epoch
    }
}
public enum VisionFailure: Error { case unavailableOwner, malformed, stale, replay, clock }
public struct VisionPoint {
    public let x: Double, y: Double, confidence: Double
    public init(x: Double, y: Double, confidence: Double) { self.x=x; self.y=y; self.confidence=confidence }
    var valid: Bool { x.isFinite && y.isFinite && confidence.isFinite && (0...1).contains(x) && (0...1).contains(y) && (0...1).contains(confidence) }
}
public struct VisionRect {
    public let x: Double, y: Double, width: Double, height: Double, confidence: Double
    public init(x: Double, y: Double, width: Double, height: Double, confidence: Double) {
        self.x=x; self.y=y; self.width=width; self.height=height; self.confidence=confidence
    }
    var valid: Bool { [x,y,width,height,confidence].allSatisfy { $0.isFinite } && x>=0 && y>=0 && width>0 && height>0 && x+width<=1.000001 && y+height<=1.000001 && (0...1).contains(confidence) }
    var area: Double { width * height }
}
public struct VisionObservations {
    // Detector converts Vision's bottom-left coordinates into display top-left.
    public let hands: [[String: VisionPoint]]
    public let faces: [VisionRect]
    public init(hands: [[String: VisionPoint]], faces: [VisionRect]) { self.hands=hands; self.faces=faces }
}
public enum GestureIntent: String { case unknown, requestCheckedLocalStop, requestDisplayAdvance }
public enum ApparentSize: String { case unknown, far, near }
public enum RelativeMovement: String { case unknown, stable, approaching, retreating }
public struct VisionResult {
    public let binding: CameraBinding
    public let sequence: UInt64
    public let capturedAt: Double
    public let gesture: GestureIntent
    public let apparentSize: ApparentSize
    public let movement: RelativeMovement
    public let normalizedArea: Double?
    public let areaSource: String?
    // Intents have no authority to Listen, arm a mic, dispatch network, or acknowledge Stop.
}
public struct VisionFrame {
    public let binding: CameraBinding
    public let sequence: UInt64
    public let capturedAt: Double
    public let width: Int, height: Int
    public let rgb: Data
    public let quarterTurnsClockwise: Int
    public let mirrored: Bool
    public init(binding: CameraBinding, sequence: UInt64, capturedAt: Double, width: Int, height: Int, rgb: Data, quarterTurnsClockwise: Int, mirrored: Bool) {
        self.binding=binding; self.sequence=sequence; self.capturedAt=capturedAt
        self.width=width; self.height=height; self.rgb=rgb
        self.quarterTurnsClockwise=quarterTurnsClockwise; self.mirrored=mirrored
    }
}
public protocol LocalLandmarkDetector { func detect(_ frame: VisionFrame) throws -> VisionObservations }

/// RAM only. All calls serialized; detector must be local and bounded. No producer or transport is created.
public final class LocalVisionPolicy {
    private let lock = NSRecursiveLock()
    private let currentCameraOwner: (CameraBinding) -> Bool
    private let clock: () -> Double
    private let detector: LocalLandmarkDetector
    private var binding: CameraBinding?, pending: VisionFrame?
    private var sequence: UInt64?, lastCapturedAt: Double?, lastNow: Double?, lastRun: Double?
    private var candidate = GestureIntent.unknown, candidateSince = 0.0, candidateCount = 0
    private var emitted = GestureIntent.unknown, lastEmission = -Double.infinity
    private var apparent = ApparentSize.unknown, priorArea: Double?, priorSource: String?
    public init(detector: LocalLandmarkDetector, currentAuthenticatedCameraOwner: @escaping (CameraBinding)->Bool, clock: @escaping ()->Double) {
        self.detector=detector; self.currentCameraOwner=currentAuthenticatedCameraOwner; self.clock=clock
    }
    private func clear() {
        binding=nil; pending=nil; sequence=nil; lastCapturedAt=nil; lastRun=nil
        candidate = .unknown; candidateSince=0; candidateCount=0; emitted = .unknown; lastEmission = -Double.infinity
        apparent = .unknown; priorArea=nil; priorSource=nil
    }
    public func disable() { lock.lock(); defer { lock.unlock() }; clear() }
    public func enable(_ b: CameraBinding, localOptIn: Bool) throws {
        lock.lock(); defer { lock.unlock() }; clear()
        guard localOptIn, currentCameraOwner(b) else { throw VisionFailure.unavailableOwner }
        _ = try now(); binding=b
    }
    private func now() throws -> Double {
        let n=clock()
        guard n.isFinite, n>=0, lastNow == nil || n>=lastNow! else { clear(); throw VisionFailure.clock }
        lastNow=n; return n
    }
    private func check(_ b: CameraBinding) throws -> Double {
        guard binding==b, currentCameraOwner(b) else { clear(); throw VisionFailure.unavailableOwner }
        return try now()
    }
    public func push(_ f: VisionFrame) throws {
        lock.lock(); defer { lock.unlock() }
        do {
            let n=try check(f.binding)
            guard f.width>0, f.height>0, f.width<=160, f.height<=120,
                  f.rgb.count==f.width*f.height*3, (0...3).contains(f.quarterTurnsClockwise),
                  f.capturedAt.isFinite, f.capturedAt>=0, f.capturedAt<=n, f.sequence>0 else { throw VisionFailure.malformed }
            guard n-f.capturedAt<0.5 else { throw VisionFailure.stale }
            guard sequence == nil || f.sequence>sequence!, lastCapturedAt == nil || f.capturedAt>=lastCapturedAt! else { throw VisionFailure.replay }
            sequence=f.sequence; lastCapturedAt=f.capturedAt; pending=f // Capacity one, replacement releases old pixels.
        } catch { clear(); throw error }
    }
    public func processLatest(_ b: CameraBinding) throws -> VisionResult? {
        lock.lock(); defer { lock.unlock() }
        do {
            let n=try check(b)
            guard let f=pending else { return nil }
            guard n-f.capturedAt<0.5 else { throw VisionFailure.stale }
            guard lastRun == nil || n-lastRun!>=0.2 else { return nil } // <=5 inference starts/sec
            if let previous=lastRun, n-previous>=0.5 {
                candidate = .unknown; candidateCount=0; emitted = .unknown
                apparent = .unknown; priorArea=nil; priorSource=nil
            }
            pending=nil; lastRun=n
            let obs=try detector.detect(f)
            let finish=try check(b)
            guard finish-f.capturedAt<0.5 else { throw VisionFailure.stale }
            guard obs.hands.count<=2, obs.faces.count<=8,
                  obs.hands.allSatisfy({ $0.count<=21 && $0.values.allSatisfy { $0.valid } }),
                  obs.faces.allSatisfy({ $0.valid }) else { throw VisionFailure.malformed }
            let proposed=classify(obs.hands)
            if proposed != candidate { candidate=proposed; candidateSince=finish; candidateCount=1 }
            else { candidateCount=min(3,candidateCount+1) }
            if proposed == .unknown { emitted = .unknown }
            var intent=GestureIntent.unknown
            if proposed != .unknown && candidateCount>=3 && finish-candidateSince>=0.4 && proposed != emitted && finish-lastEmission>=1 {
                intent=proposed; emitted=proposed; lastEmission=finish
            }
            let (area,source)=apparentArea(obs)
            var movement=RelativeMovement.unknown
            if let a=area {
                if priorSource != source { apparent = .unknown; priorArea=nil }
                if a>=0.16 { apparent = .near } else if a<=0.08 { apparent = .far }
                if let old=priorArea, old>0.005 {
                    let ratio=a/old
                    movement = ratio>=1.25 ? .approaching : (ratio<=0.8 ? .retreating : .stable)
                }
                priorArea=a; priorSource=source
            } else { apparent = .unknown; priorArea=nil; priorSource=nil }
            return VisionResult(binding:b, sequence:f.sequence, capturedAt:f.capturedAt, gesture:intent,
                                apparentSize:apparent, movement:movement, normalizedArea:area, areaSource:source)
        } catch { clear(); throw error }
    }
    private func classify(_ hands: [[String: VisionPoint]]) -> GestureIntent {
        // Conservative geometry on actual SDK landmarks, not RGB heuristics. Ambiguous/multiple hands -> Unknown.
        guard hands.count==1, let h=hands.first, let wrist=h["wrist"], wrist.confidence>=0.7 else { return .unknown }
        func distance(_ a: VisionPoint,_ b: VisionPoint)->Double { hypot(a.x-b.x,a.y-b.y) }
        let fingers=["index","middle","ring","little"]
        var extended=0, folded=0
        for finger in fingers {
            guard let tip=h[finger+"Tip"], let pip=h[finger+"PIP"], let mcp=h[finger+"MCP"],
                  min(tip.confidence,pip.confidence,mcp.confidence)>=0.7 else { return .unknown }
            let scale=distance(wrist,mcp)
            guard scale>=0.025 else { return .unknown }
            if distance(wrist,tip)>distance(wrist,pip)+0.3*scale { extended+=1 }
            if distance(wrist,tip)<distance(wrist,pip)-0.15*scale { folded+=1 }
        }
        if extended==4 { return .requestCheckedLocalStop }
        if folded==4 { return .requestDisplayAdvance }
        return .unknown
    }
    private func apparentArea(_ obs: VisionObservations)->(Double?,String?) {
        if let face=obs.faces.filter({ $0.confidence>=0.7 }).max(by: { $0.area<$1.area }) { return (face.area,"face") }
        guard obs.hands.count==1, let h=obs.hands.first, h.count>=17, h.values.allSatisfy({ $0.confidence>=0.7 }) else { return (nil,nil) }
        let xs=h.values.map { $0.x }, ys=h.values.map { $0.y }
        let area=(xs.max()!-xs.min()!)*(ys.max()!-ys.min()!)
        return area>0.005 ? (area,"hand") : (nil,nil)
    }
}
