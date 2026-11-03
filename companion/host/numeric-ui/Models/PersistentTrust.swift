import Foundation

// Read-only view of the persistent host registry (streaming/remembered_pairing.py over the
// HomeLab SQLite store). It exposes state and counts only. It has no field that can hold a
// credential, credential hash, certificate digest, endpoint or enrollment context, and the
// SQLite reader denies reads of those columns at the engine level (see PersistentRegistryReader).

/// State of the `stackchan_pair` gate row, as written by RememberedRegistry.
enum PairRowState: Equatable { case unpaired, pending, active, revoked, unrecognized }

struct RegistrySnapshot: Equatable {
    let pairState: PairRowState
    let expiryUnix: Int?
    /// Non-revoked / revoked credential rows for the one StackChan device. Counts only.
    let activeCredentialCount: Int
    let revokedCredentialCount: Int
    static let empty = RegistrySnapshot(pairState: .unpaired, expiryUnix: nil, activeCredentialCount: 0, revokedCredentialCount: 0)
}

protocol PersistentRegistryReading { func snapshot() throws -> RegistrySnapshot }

/// Backs `LaunchTrustProviding` with the persistent registry. It decides launch presence
/// only; the host still re-authenticates the credential on every connect, so `.valid` here
/// is "a committed, unexpired pairing exists", never "the robot is trusted right now".
struct PersistentRegistryTrust: LaunchTrustProviding {
    let reader: PersistentRegistryReading
    let clock: () -> Int
    init(reader: PersistentRegistryReading, clock: @escaping () -> Int = { Int(Date().timeIntervalSince1970) }) {
        self.reader = reader; self.clock = clock
    }
    func current() -> LaunchTrust {
        guard let s = try? reader.snapshot() else { return .unknown }
        switch s.pairState {
        case .unpaired, .pending, .revoked: return .absent       // pending is never committed trust
        case .unrecognized: return .unknown
        case .active:
            guard let expiry = s.expiryUnix, clock() < expiry else { return .stale }
            return s.activeCredentialCount >= 1 ? .valid : .stale
        }
    }
}
