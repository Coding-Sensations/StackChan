import Foundation

struct DurableRobotIdentity: Equatable {
    let deviceKeyDigest: String
    let hostKeyDigest: String
    let trustProfileDigest: String
    let enrollmentVersion = 1
    init?(deviceKeyDigest: String, hostKeyDigest: String, trustProfileDigest: String) {
        func valid(_ digest: String) -> Bool {
            digest.utf8.count == 64 && digest.utf8.allSatisfy { (48...57).contains($0) || (97...102).contains($0) }
        }
        guard valid(deviceKeyDigest), valid(hostKeyDigest), valid(trustProfileDigest) else { return nil }
        self.deviceKeyDigest=deviceKeyDigest; self.hostKeyDigest=hostKeyDigest; self.trustProfileDigest=trustProfileDigest
    }
}
struct RememberedRobot: Equatable {
    let identity: DurableRobotIdentity
    let secureItemReference: String
    init?(identity: DurableRobotIdentity, secureItemReference: String) {
        guard (1...128).contains(secureItemReference.utf8.count),
              secureItemReference.utf8.allSatisfy({ (33...126).contains($0) }) else { return nil }
        self.identity=identity; self.secureItemReference=secureItemReference
    }
}
enum RememberedDeviceError: Error { case unavailable }
protocol RememberedDeviceStorage {
    func readForReview() throws -> RememberedRobot?
    func remove(_ reference: String) throws
}
protocol DeviceTrustRevoking { func revoke(_ record: RememberedRobot) throws }
struct UnavailableRememberedDeviceStorage: RememberedDeviceStorage {
    func readForReview() throws -> RememberedRobot? { throw RememberedDeviceError.unavailable }
    func remove(_ reference: String) throws { throw RememberedDeviceError.unavailable }
}
struct UnavailableDeviceTrustRevoker: DeviceTrustRevoking {
    func revoke(_ record: RememberedRobot) throws { throw RememberedDeviceError.unavailable }
}
enum RememberedDeviceFeedback {
    case unavailable, requested, preferenceCleared, revocationRequired, storageCleanupRequired, forgotten
    var text: String {
        switch self {
        case .unavailable: return "Remembered pairing is unavailable in this build. Every session still needs matching numbers and physical consent."
        case .requested: return "Preference requested for future enrollment. It is held only while this app is open; no device or credential is saved."
        case .preferenceCleared: return "The unsaved preference was cleared. No durable device trust was created."
        case .revocationRequired: return "Forget Device could not confirm revocation. Keep the record until the operator verifies revocation."
        case .storageCleanupRequired: return "Revocation completed, but secure storage cleanup failed. The operator must finish removal before re-enrollment."
        case .forgotten: return "Device trust was revoked and its secure reference removed. Fresh interactive enrollment is required."
        }
    }
}
struct RememberedPairing {
    private let storage: RememberedDeviceStorage
    private let revoker: DeviceTrustRevoking
    private(set) var record: RememberedRobot?
    private(set) var requested=false
    private(set) var feedback: RememberedDeviceFeedback = .unavailable
    var activationAvailable: Bool { false }
    var mayAuthorizeSession: Bool { false }
    init(storage: RememberedDeviceStorage = UnavailableRememberedDeviceStorage(),
         revoker: DeviceTrustRevoking = UnavailableDeviceTrustRevoker()) {
        self.storage=storage; self.revoker=revoker
    }
    mutating func requestRemember(_ enabled: Bool) {
        requested=enabled; feedback=enabled ? .requested : .preferenceCleared
    }
    mutating func loadForReview() {
        do { record=try storage.readForReview() }
        catch { feedback = .unavailable }
    }
    func requiresReenrollment(_ candidate: DurableRobotIdentity) -> Bool { record?.identity != candidate }
    mutating func forget() {
        guard let existing=record else { requested=false; feedback = .preferenceCleared; return }
        do { try revoker.revoke(existing) }
        catch { feedback = .revocationRequired; return }
        do { try storage.remove(existing.secureItemReference) }
        catch { feedback = .storageCleanupRequired; return }
        record=nil; requested=false; feedback = .forgotten
    }
}
