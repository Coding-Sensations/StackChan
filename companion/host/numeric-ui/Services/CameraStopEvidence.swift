import Foundation

// Closure evidence consumer, not a camera-purpose issuer or permission verifier.
struct CameraStopContext {
    let session:AnyObject
    let binding:WebcamPurposeBinding
    let boot:String
    let authorityID:UInt64
    let scopeExpires:Double // capture lease metadata, not permission to abandon cleanup
}
struct CameraStopChallenge {
    let context:CameraStopContext
    let nonce:UUID // local challenge only; never a credential/camera permission
    let sequence:UInt64
    let expires:Double
}
struct CameraDriverStopReport {
    let binding:WebcamPurposeBinding
    let boot:String
    let authorityID:UInt64
    let challenge:UUID
    let stopSequence:UInt64
    let reportSerial:UInt64
    let outcome:String
    let required:UInt8,completed:UInt8
    let revoked:Bool,joined:Bool,wiped:Bool,custodyReleased:Bool,closeUncertain:Bool,everStarted:Bool
    var driverClosed:Bool {
        outcome=="DriverClosed" && everStarted && revoked && joined && wiped && custodyReleased && !closeUncertain && required<=15 && completed==required && reportSerial>0
    }
}
// Only an externally qualified local adapter returns this; no peer JSON promotion.
struct PhysicalCameraStopAttestation {
    let challenge:CameraStopChallenge
    let reportSerial:UInt64
    let qualificationReference:String
    let sensorCeased:Bool,dmaQuiescent:Bool,noNewFrames:Bool,startBarrierHeld:Bool
    func matches(_ p:CameraStopChallenge,_ r:CameraDriverStopReport)->Bool {
        challenge.context.session===p.context.session && challenge.context.binding==p.context.binding && challenge.context.boot==p.context.boot && challenge.context.authorityID==p.context.authorityID && challenge.nonce==p.nonce && challenge.sequence==p.sequence && challenge.expires==p.expires && reportSerial==r.reportSerial && reportSerial>0 && !qualificationReference.isEmpty && qualificationReference.utf8.count<=128 && qualificationReference.utf8.allSatisfy { ($0>=48 && $0<=57) || ($0>=65 && $0<=90) || ($0>=97 && $0<=122) || [45,95,46,58].contains($0) } && sensorCeased && dmaQuiescent && noNewFrames && startBarrierHeld
    }
}
fileprivate final class CameraStopSeal {}
final class QualifiedPhysicalCameraStop {
    fileprivate let seal:CameraStopSeal
    fileprivate let challenge:CameraStopChallenge
    fileprivate let report:CameraDriverStopReport
    var binding:WebcamPurposeBinding { challenge.context.binding }
    fileprivate init(seal:CameraStopSeal,challenge:CameraStopChallenge,report:CameraDriverStopReport) { self.seal=seal;self.challenge=challenge;self.report=report }
}
final class CameraStopEvidenceConsumer {
    private let current:(CameraStopContext)->Bool // existing retained owner callback
    private let driverOrigin:((CameraStopChallenge,CameraDriverStopReport)->Bool)?
    private let physical:((CameraStopChallenge,CameraDriverStopReport)->PhysicalCameraStopAttestation?)?
    private let noNewer:((CameraStopChallenge)->Bool)?
    private let clock:()->Double
    private let nonce:()->UUID
    private let seal=CameraStopSeal()
    private let lock=NSRecursiveLock()
    private var sequence:UInt64=0,driverSerial:UInt64=0
    private var lastClock = -1.0
    private var serialSession:AnyObject?,serialBoot:String?,serialAuthority:UInt64=0
    private var issued:QualifiedPhysicalCameraStop?
    private(set) var pending:CameraStopChallenge?
    private(set) var driverClosed=false
    var stopPending:Bool { pending != nil }
    init(current:@escaping (CameraStopContext)->Bool,
         driverOrigin:((CameraStopChallenge,CameraDriverStopReport)->Bool)?=nil,
         physicalQualification:((CameraStopChallenge,CameraDriverStopReport)->PhysicalCameraStopAttestation?)?=nil,
         noNewerStream:((CameraStopChallenge)->Bool)?=nil,
         clock:@escaping ()->Double={ProcessInfo.processInfo.systemUptime},nonce:@escaping ()->UUID={UUID()}) {
        self.current=current;self.driverOrigin=driverOrigin;self.physical=physicalQualification;self.noNewer=noNewerStream;self.clock=clock;self.nonce=nonce
    }
    private func now()->Double? {
        let t=clock();guard t.isFinite,t>=0,t>=lastClock else { return nil };lastClock=t;return t
    }
    func begin(_ context:CameraStopContext)->CameraStopChallenge? {
        lock.lock();defer { lock.unlock() }
        guard let t=now(),context.binding.valid,context.authorityID>0,!context.boot.isEmpty,context.boot.utf8.count<=64,
              context.scopeExpires.isFinite,current(context) else { return nil }
        if let p=pending {
            guard p.context.session===context.session,p.context.binding==context.binding,p.context.boot==context.boot,p.context.authorityID==context.authorityID,p.context.scopeExpires==context.scopeExpires else { return nil }
            if t<p.expires { return p }
            // Rotate only the expired challenge; exact cleanup custody remains held.
        }
        guard sequence<UInt64.max else { return nil };sequence+=1
        let p=CameraStopChallenge(context:context,nonce:nonce(),sequence:sequence,expires:t+2)
        if serialSession !== context.session || serialBoot != context.boot || serialAuthority != context.authorityID { driverSerial=0 }
        serialSession=context.session;serialBoot=context.boot;serialAuthority=context.authorityID
        pending=p;driverClosed=false;issued=nil;return p
    }
    private func matches(_ session:AnyObject,_ r:CameraDriverStopReport,_ t:Double)->Bool {
        guard let p=pending else { return false }
        return session===p.context.session && t<p.expires && current(p.context) && r.binding==p.context.binding && r.boot==p.context.boot && r.authorityID==p.context.authorityID && r.challenge==p.nonce && r.stopSequence==p.sequence
    }
    func observe(session:AnyObject,report:CameraDriverStopReport)->QualifiedPhysicalCameraStop? {
        lock.lock();defer { lock.unlock() }
        guard let t=now(),matches(session,report,t),report.reportSerial>driverSerial,report.driverClosed,let p=pending,
              driverOrigin?(p,report)==true,let checkedTime=now(),matches(session,report,checkedTime) else { return nil }
        driverSerial=report.reportSerial;driverClosed=true
        // Driver ioctls/closure do not prove sensor rail/reset or DMA/frame cessation.
        // External callbacks must qualify genuine hardware and retained start barrier;
        // no production adapters or peer-boolean promotions are supplied.
        guard let attestation=physical?(p,report),attestation.matches(p,report),noNewer?(p)==true,let finalTime=now(),matches(session,report,finalTime),noNewer?(p)==true else { return nil }
        let result=QualifiedPhysicalCameraStop(seal:seal,challenge:p,report:report);issued=result;return result
    }
    func consume(session:AnyObject,receipt:QualifiedPhysicalCameraStop)->Bool {
        lock.lock();defer { lock.unlock() }
        guard let t=now(),let p=pending,receipt===issued,receipt.seal===seal,receipt.challenge.nonce==p.nonce,
              matches(session,receipt.report,t),noNewer?(p)==true else { return false }
        pending=nil;issued=nil;driverClosed=false;return true
    }
    func consumeForPendingBinding(_ binding:WebcamPurposeBinding,receipt:QualifiedPhysicalCameraStop)->Bool {
        lock.lock();defer { lock.unlock() }
        guard let p=pending,p.context.binding==binding else { return false }
        return consume(session:p.context.session,receipt:receipt)
    }
}
