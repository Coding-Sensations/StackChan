import Foundation

/// Borrowed owner tuple only; this type issues no authority or permission.
public struct ClockOwnerTuple: Equatable {
    public let deviceID, conversationID, turnID, ownerPurposeID: String
    public let turnGeneration, cameraGeneration, cameraEpoch: UInt64
    public init(deviceID:String,conversationID:String,turnID:String,turnGeneration:UInt64,cameraGeneration:UInt64,cameraEpoch:UInt64,ownerPurposeID:String) throws {
        func valid(_ s:String)->Bool { !s.isEmpty && s.utf8.count<=128 && s.utf8.allSatisfy { (48...57).contains($0) || (65...90).contains($0) || (97...122).contains($0) || [45,46,58,95].contains($0) } }
        guard [deviceID,conversationID,turnID,ownerPurposeID].allSatisfy(valid), turnGeneration>0,cameraGeneration>0,cameraEpoch>0 else { throw ClockMappingFailure.malformed }
        self.deviceID=deviceID; self.conversationID=conversationID; self.turnID=turnID; self.ownerPurposeID=ownerPurposeID
        self.turnGeneration=turnGeneration; self.cameraGeneration=cameraGeneration; self.cameraEpoch=cameraEpoch
    }
}
public struct ClockChallenge: Equatable { public let owner:ClockOwnerTuple; public let nonce:String }
public struct ClockResponse {
    public let challenge:ClockChallenge
    public let bootID,clockDomain:String
    public let producerReceiveMS,producerSendMS:UInt64
}
public struct ClockCaptureStamp {
    public let owner:ClockOwnerTuple
    public let bootID,clockDomain:String
    public let sequence,capturedMS:UInt64
}
public enum ClockMappingFailure:Error { case unavailable, malformed, replay, clock, expired, uncertainty, drift, provenance }
public struct QualifiedClockMapping {
    public let owner:ClockOwnerTuple
    public let bootID,clockDomain:String
    public let producerAnchorMS:UInt64
    public let hostLower,hostUpper,maximumDriftPerSecond,validUntil:Double
    func captureBounds(_ stamp:ClockCaptureStamp,now:Double)throws->(Double,Double) {
        guard stamp.owner==owner, stamp.bootID==bootID,stamp.clockDomain==clockDomain,stamp.sequence>0,
              now.isFinite, now>=hostUpper,now<validUntil else { throw ClockMappingFailure.expired }
        let positive=stamp.capturedMS>=producerAnchorMS
        let difference=positive ? stamp.capturedMS-producerAnchorMS : producerAnchorMS-stamp.capturedMS
        guard difference<=2000 else { throw ClockMappingFailure.expired }
        let delta=(positive ? 1.0 : -1.0)*Double(difference)/1000
        let drift=abs(delta)*maximumDriftPerSecond
        let earliest=hostLower+delta-drift-0.001, latest=hostUpper+delta+drift+0.001
        guard earliest>=0,latest<=now,now-earliest<0.5 else { throw ClockMappingFailure.expired }
        return (earliest,latest)
    }
}

/// Existing authenticated dispatcher and qualified driver receipts are injected.
/// All defaults deny. No issuer, new grant verifier, credential, socket, camera, or capture.
public final class ProducerClockHandshake {
    private let lock=NSRecursiveLock()
    private let current:(ClockOwnerTuple)->Bool
    private let clockReceipt:(ClockResponse)->Bool
    private let captureReceipt:(ClockCaptureStamp)->Bool
    private let clock:()->Double
    private let nonce:()->String
    private var owner:ClockOwnerTuple?,boot:String?,used=Set<String>()
    private var pending:(ClockChallenge,Double)?
    private var anchor:(UInt64,UInt64,Double,Double)?
    private var mapping:QualifiedClockMapping?
    private var lastNow:Double?,captureSequence:UInt64?,captureMS:UInt64?
    public init(currentOwner:@escaping(ClockOwnerTuple)->Bool={_ in false},
                acceptedClockReceipt:@escaping(ClockResponse)->Bool={_ in false},
                acceptedCaptureReceipt:@escaping(ClockCaptureStamp)->Bool={_ in false},
                clock:@escaping()->Double={ProcessInfo.processInfo.systemUptime},
                nonce:@escaping()->String={UUID().uuidString.replacingOccurrences(of:"-",with:"").lowercased()}) {
        current=currentOwner;clockReceipt=acceptedClockReceipt;captureReceipt=acceptedCaptureReceipt;self.clock=clock;self.nonce=nonce
    }
    private func clear() { pending=nil;anchor=nil;mapping=nil;captureSequence=nil;captureMS=nil }
    public func invalidate() { lock.lock();defer {lock.unlock()};clear() }
    private func now()throws->Double {
        let n=clock();guard n.isFinite,n>=0,n<=4294967296,lastNow==nil || n>=lastNow! else { clear();throw ClockMappingFailure.clock }
        lastNow=n;return n
    }
    private func requireCurrent(_ o:ClockOwnerTuple)throws {
        guard current(o) else { clear();throw ClockMappingFailure.unavailable }
    }
    public func begin(_ o:ClockOwnerTuple)throws->ClockChallenge {
        lock.lock();defer {lock.unlock()}
        do {
            try requireCurrent(o);let n=try now()
            if owner != o { clear();owner=o;boot=nil;used.removeAll() }
            guard pending==nil,used.count<64 else { throw ClockMappingFailure.unavailable }
            let value=nonce()
            guard value.utf8.count==32,value.utf8.allSatisfy({(48...57).contains($0) || (97...102).contains($0)}),!used.contains(value) else { throw ClockMappingFailure.replay }
            let c=ClockChallenge(owner:o,nonce:value);used.insert(value);pending=(c,n);return c
        } catch { clear();throw error }
    }
    public func complete(_ r:ClockResponse)throws->QualifiedClockMapping? {
        lock.lock();defer {lock.unlock()}
        do {
            let n=try now();guard let (challenge,h0)=pending else { throw ClockMappingFailure.replay };pending=nil
            try requireCurrent(challenge.owner)
            guard r.challenge==challenge,clockReceipt(r) else { throw ClockMappingFailure.provenance }
            guard !r.bootID.isEmpty,r.bootID.utf8.count<=128,r.bootID.utf8.allSatisfy({(48...57).contains($0) || (65...90).contains($0) || (97...122).contains($0) || [45,46,58,95].contains($0)}),
                  r.clockDomain=="esp-monotonic-ms-v1",r.producerSendMS>=r.producerReceiveMS,n-h0<=0.1 else { throw ClockMappingFailure.malformed }
            guard boot==nil || boot==r.bootID else { throw ClockMappingFailure.clock };boot=r.bootID
            let processing=r.producerSendMS-r.producerReceiveMS
            guard processing<=100 else { throw ClockMappingFailure.malformed }
            let lower=h0+(Double(processing)/1000)/1.01-0.001,upper=n+0.001
            guard lower>=0,upper>=lower,upper-lower<=0.004 else { throw ClockMappingFailure.uncertainty }
            if let previous=anchor {
                guard r.producerReceiveMS>=previous.1,r.producerSendMS>previous.1 else { throw ClockMappingFailure.clock }
                let elapsedMS=r.producerSendMS-previous.1
                guard (500...2000).contains(elapsedMS) else { throw ClockMappingFailure.drift }
                let elapsed=Double(elapsedMS)/1000
                let measured=max(abs(1-(lower-previous.3)/elapsed),abs(1-(upper-previous.2)/elapsed))
                guard measured<=0.01 else { throw ClockMappingFailure.drift }
                let result=QualifiedClockMapping(owner:challenge.owner,bootID:r.bootID,clockDomain:r.clockDomain,
                    producerAnchorMS:r.producerSendMS,hostLower:lower,hostUpper:upper,maximumDriftPerSecond:0.01,validUntil:lower+2)
                try requireCurrent(challenge.owner)
                anchor=(r.producerReceiveMS,r.producerSendMS,lower,upper);mapping=result;return result
            }
            anchor=(r.producerReceiveMS,r.producerSendMS,lower,upper);mapping=nil;return nil
        } catch { clear();throw error }
    }
    public func mapCapture(_ stamp:ClockCaptureStamp)throws->(Double,Double) {
        lock.lock();defer {lock.unlock()}
        do {
            let n=try now();guard let m=mapping else { throw ClockMappingFailure.unavailable }
            try requireCurrent(m.owner)
            guard captureReceipt(stamp) else { throw ClockMappingFailure.provenance }
            guard captureSequence==nil || stamp.sequence>captureSequence!,captureMS==nil || stamp.capturedMS>=captureMS! else { throw ClockMappingFailure.replay }
            let bounds=try m.captureBounds(stamp,now:n);try requireCurrent(stamp.owner)
            captureSequence=stamp.sequence;captureMS=stamp.capturedMS;return bounds
        } catch { clear();throw error }
    }
}
