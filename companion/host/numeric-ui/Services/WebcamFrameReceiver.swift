import Foundation
import CoreMedia
import CryptoKit

struct WebcamPurposeBinding: Equatable {
    let device: String, conversation: String, turn: String
    let generation: UInt64, epoch: UInt64
    var valid: Bool {
        generation > 0 && epoch > 0 && [(device,64),(conversation,128),(turn,128)].allSatisfy { value, cap in
            !value.isEmpty && value.utf8.count <= cap && value.utf8.allSatisfy { (48...57).contains($0) || (65...90).contains($0) || (97...122).contains($0) || [45,46,58,95].contains($0) }
        }
    }
    var cmioBinding: String {
        let tuple="\(device.utf8.count):\(device)\(conversation.utf8.count):\(conversation)\(turn.utf8.count):\(turn)"
        return "wcv1." + SHA256.hash(data:Data(tuple.utf8)).map { String(format:"%02x",$0) }.joined()
    }
}
struct WebcamMappedCapture { let hostTime: Double, uncertainty: Double }
struct WebcamStopReceipt {
    let binding:WebcamPurposeBinding
    let hardwareClosed:Bool // legacy observation only; insufficient for physical Off
    var qualifiedPhysical:QualifiedPhysicalCameraStop? = nil
}

// Implement only by a bridge to the existing verified session owner, never from
// a worker JSON boolean, comparison number, audio grant or reconnect status.
// The inspected owner cannot issue this purpose or prove producer clock/Stop.
protocol AuthenticatedWebcamOwner: AnyObject {
    var transportClockAndStopQualified: Bool { get }
    func issueFromExplicitUIIntent() throws -> WebcamPurposeBinding
    func isCurrent(_ binding: WebcamPurposeBinding) -> Bool
    func mapProducerCapture(_ nanoseconds: UInt64, binding: WebcamPurposeBinding) throws -> WebcamMappedCapture
    func requestCheckedStop(_ binding: WebcamPurposeBinding) -> WebcamStopReceipt?
}
enum WebcamReceiveFailure: Error { case unavailable, malformed, stale, replay }

final class CompanionWebcamPipeline {
    static let frameBytes=307_200, messageBytes=4_112, headerBytes=56
    private let owner: AuthenticatedWebcamOwner?
    private let stopEvidence:CameraStopEvidenceConsumer?
    private let clock: () -> Double
    private let sink: (Data, UInt64, Double) -> Bool
    private let sinkRevoke: () -> Void
    private let sinkBegin:(WebcamPurposeBinding)->Bool
    private let extensionApproved: Bool
    private let allocate:(Int)->Data
    private var pendingStopBinding:WebcamPurposeBinding?
    private(set) var frameAllocations=0
    private(set) var binding: WebcamPurposeBinding?
    private var retired:(UInt64,UInt64)=(0,0)
    private var lastSequence:UInt64=0, assemblySequence:UInt64=0, producerCapture:UInt64=0
    private var assembly:Data?, latest:(Data,UInt64,Double)?
    private var nextOffset=0, captured=0.0, deadline=0.0, lastClock = -1.0
    private(set) var stopPending=false
    init(owner: AuthenticatedWebcamOwner?=nil, extensionApproved:Bool=false,
         clock:@escaping ()->Double={ProcessInfo.processInfo.systemUptime},
         sink:@escaping (Data,UInt64,Double)->Bool={_,_,_ in false}, sinkRevoke:@escaping ()->Void={}, allocate:@escaping (Int)->Data={Data(count:$0)}, sinkBegin:@escaping (WebcamPurposeBinding)->Bool={_ in true}, stopEvidence:CameraStopEvidenceConsumer?=nil) {
        self.owner=owner;self.stopEvidence=stopEvidence;self.extensionApproved=extensionApproved;self.clock=clock;self.sink=sink;self.sinkRevoke=sinkRevoke;self.allocate=allocate;self.sinkBegin=sinkBegin
    }
    var ready:Bool { owner?.transportClockAndStopQualified == true && extensionApproved && !stopPending }
    private func now() throws -> Double {
        let time=clock()
        guard time.isFinite,time>=0,time>=lastClock else { _=revoke();throw WebcamReceiveFailure.stale }
        lastClock=time;return time
    }
    func explicitWebcamOn() -> Bool {
        guard ready,binding==nil,let owner else { return false }
        do {
            let p=try owner.issueFromExplicitUIIntent();let time=try now()
            guard p.valid,p.cmioBinding.utf8.count<=128,(p.generation>retired.0 || (p.generation==retired.0 && p.epoch>retired.1)),owner.isCurrent(p) else { return false }
            binding=p;deadline=time+2;lastSequence=0
            guard sinkBegin(p) else { _=revoke();return false };return true
        } catch { return false }
    }
    @discardableResult func revoke() -> Bool {
        let p=binding;binding=nil;assembly=nil;latest=nil;nextOffset=0;assemblySequence=0;sinkRevoke()
        guard let p else { return !stopPending }
        if p.generation>retired.0 || (p.generation==retired.0 && p.epoch>retired.1) { retired=(p.generation,p.epoch) };stopPending=true;pendingStopBinding=p
        if let receipt=owner?.requestCheckedStop(p) { _=confirmCheckedStop(receipt) }
        return !stopPending
    }
    @discardableResult func confirmCheckedStop(_ receipt:WebcamStopReceipt)->Bool {
        guard pendingStopBinding==receipt.binding,let proof=receipt.qualifiedPhysical,let stopEvidence,
              stopEvidence.consumeForPendingBinding(receipt.binding,receipt:proof) else { return false }
        stopPending=false;pendingStopBinding=nil;return true
    }
    private func authorize(_ time:Double) throws -> WebcamPurposeBinding {
        guard let p=binding, time<deadline, owner?.isCurrent(p)==true else { _=revoke();throw WebcamReceiveFailure.unavailable };return p
    }
    private func integer(_ data:Data,_ offset:Int,_ count:Int)->UInt64 {
        var v:UInt64=0;for i in offset..<offset+count { v=(v<<8)|UInt64(data[i]) };return v
    }
    func ingestFragment(_ wire:Data) throws {
        precondition(Thread.isMainThread)
        do { try ingestCheckedFragment(wire) } catch { discardAssembly();throw error }
    }
    private func ingestCheckedFragment(_ wire:Data) throws {
        let time=try now(), p=try authorize(time)
        guard (Self.headerBytes+4...Self.messageBytes).contains(wire.count),Array(wire.prefix(4))==[87,67,86,49],wire[4]==1,wire[5]==1,
              integer(wire,6,2)==320,integer(wire,8,2)==240,integer(wire,42,4)==UInt64(Self.frameBytes) else { throw WebcamReceiveFailure.malformed }
        let gen=integer(wire,10,8),epoch=integer(wire,18,8),seq=integer(wire,26,8),capture=integer(wire,34,8)
        let offset=Int(integer(wire,46,4)),d=Int(integer(wire,50,2)),c=Int(integer(wire,52,2)),t=Int(integer(wire,54,2)),end=56+d+c+t
        guard gen>0,epoch>0,seq>lastSequence,capture>0,(1...64).contains(d),(1...128).contains(c),(1...128).contains(t),end<wire.count,offset<Self.frameBytes,wire.count-end<=Self.frameBytes-offset else { throw WebcamReceiveFailure.malformed }
        guard gen==p.generation,epoch==p.epoch,
              String(data:wire.subdata(in:56..<56+d),encoding:.ascii)==p.device,
              String(data:wire.subdata(in:56+d..<56+d+c),encoding:.ascii)==p.conversation,
              String(data:wire.subdata(in:56+d+c..<end),encoding:.ascii)==p.turn else { throw WebcamReceiveFailure.unavailable }
        guard let mapped=try owner?.mapProducerCapture(capture,binding:p),mapped.hostTime.isFinite,mapped.uncertainty.isFinite,mapped.uncertainty>=0,mapped.uncertainty<=0.025,mapped.hostTime<=time,time-mapped.hostTime+mapped.uncertainty<=0.25 else { throw WebcamReceiveFailure.stale }
        if offset==0 {
            guard assembly==nil || seq>assemblySequence else { throw WebcamReceiveFailure.replay }
            if assembly != nil { lastSequence=max(lastSequence,assemblySequence) }
            // The first frame-sized allocation follows all header/auth/time checks.
            frameAllocations+=1;assembly=allocate(Self.frameBytes)
            guard assembly?.count==Self.frameBytes else { discardAssembly();throw WebcamReceiveFailure.malformed }
            assemblySequence=seq;producerCapture=capture;captured=mapped.hostTime;nextOffset=0
        }
        guard assembly != nil,seq==assemblySequence,capture==producerCapture,mapped.hostTime==captured,offset==nextOffset else { discardAssembly();throw WebcamReceiveFailure.malformed }
        _=try authorize(try now());let n=wire.count-end
        assembly!.replaceSubrange(offset..<offset+n,with:wire[end...]);nextOffset+=n
        if nextOffset==Self.frameBytes { latest=(assembly!,seq,captured);assembly=nil;assemblySequence=0;nextOffset=0;lastSequence=seq;deadline=time+2 }
    }
    private func discardAssembly() { lastSequence=max(lastSequence,assemblySequence);assembly=nil;assemblySequence=0;nextOffset=0 }
    func submitLatestAfterControlAndAudio() -> Bool {
        do {
            let time=try now();_=try authorize(time);guard let frame=latest else { return false };latest=nil
            guard time-frame.2<=0.25 else { return false };return sink(frame.0,frame.1,frame.2)
        } catch { return false }
    }
    // Existing child stdout is lifecycle/generation-bound by PairingController.
    // No event grants owner, clock calibration, extension approval or readiness.
    func handleExistingWorkerEvent(_ line:Data) -> Bool {
        guard line.count<=8192,let object=try? JSONSerialization.jsonObject(with:line) as? [String:Any],let kind=object["type"] as? String,kind.hasPrefix("webcam_") else { return false }
        if kind=="webcam_unavailable" || kind=="webcam_off" { _=revoke();return true }
        guard kind=="webcam_fragment",object.count==2,let encoded=object["wire"] as? String,encoded.utf8.count<=5484,binding != nil,owner?.isCurrent(binding!)==true,
              let data=Data(base64Encoded:encoded),data.count<=Self.messageBytes else { return true }
        do { try ingestFragment(data);_=submitLatestAfterControlAndAudio() } catch { discardAssembly() }
        return true
    }
}

// Concrete CMIO delivery path in the existing app. The current controller passes
// owner:nil/extensionApproved:false; no sink is attached or API executed at boot.
extension CompanionWebcamPipeline {
    convenience init(owner:AuthenticatedWebcamOwner?, verifiedInstalledBridge:WebcamBridgeAdapter,
                     extensionApproved:Bool, clock:@escaping ()->Double={ProcessInfo.processInfo.systemUptime}) {
        self.init(owner:owner,extensionApproved:extensionApproved,clock:clock,
            sink:{ data,sequence,captured in
                verifiedInstalledBridge.submitFromCurrentAuthenticatedOwner(pixels:data,sequence:sequence,
                    trustedHostCaptureTime:CMTime(seconds:captured,preferredTimescale:1_000_000_000))
            },sinkRevoke:{ _=verifiedInstalledBridge.revokeLocalExtension() },
            sinkBegin:{ p in verifiedInstalledBridge.explicitWebcamOn(binding:p.cmioBinding,generation:p.generation,epoch:p.epoch) })
    }
}
