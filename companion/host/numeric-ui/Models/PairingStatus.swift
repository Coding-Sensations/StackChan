import Foundation
import CoreFoundation

enum CompanionAudioMode: String {
    case transportTest = "transport_test"
    case nativeConversation = "native_conversation"
}

enum PairingPhase: String {
    case idle, opening, usb_ready, waiting_robot, comparing, waiting_confirmation
    case staging, paired, restoring, cancelled, expired, failed, closed
    var terminal: Bool { [.cancelled, .expired, .failed, .closed].contains(self) }
    var busy: Bool { self != .idle && !terminal }
}
enum PairingCleanupIssue: String, Hashable {
    case audio, grant, usb
}
struct PairingStatus {
    let phase: PairingPhase
    let code: String?
    let acknowledged: Bool
    let remaining: Int?
    let audioRemaining: Int?
    let activity: PairingActivity?
    let recovery: PairingRecovery?
    let cleanupIssues: Set<PairingCleanupIssue>
    let rememberedForgetVerified: Bool
    let rememberedCleanupVerified: Bool
    static func decode(_ data: Data) -> PairingStatus? {
        guard data.count <= 1024,
              let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              Set(object.keys).isSubset(of: ["state", "code", "message", "acknowledged", "remaining", "cleanup", "activity", "audio_remaining"]),
              let state = object["state"] as? String, let phase = PairingPhase(rawValue: state) else { return nil }
        let code = object["code"] as? String
        if let code = code {
            guard phase == .comparing || phase == .waiting_confirmation,
                  code.utf8.count == 6, code.utf8.allSatisfy({ $0 >= 48 && $0 <= 57 }) else { return nil }
        }
        if object["code"] != nil && code == nil { return nil }
        var remaining: Int? = nil
        if let value = object["remaining"] {
            guard let number = value as? NSNumber,
                  CFGetTypeID(number) != CFBooleanGetTypeID(),
                  number.doubleValue.rounded() == number.doubleValue,
                  (0...120).contains(number.intValue) else { return nil }
            remaining = number.intValue
        }
        var ack = false
        if let value = object["acknowledged"] {
            guard let number = value as? NSNumber, CFGetTypeID(number) == CFBooleanGetTypeID() else { return nil }
            ack = number.boolValue
        }
        if phase == .paired && !ack { return nil }
        let message = object["message"] as? String
        if object["message"] != nil && message == nil { return nil }
        var activity: PairingActivity? = nil
        if let rawActivity = object["activity"] {
            guard phase == .paired, ack, let label = rawActivity as? String,
                  let parsed = PairingActivity(rawValue: label) else { return nil }
            activity = parsed
        }
        var audioRemaining: Int? = nil
        if let value = object["audio_remaining"] {
            guard activity == .conversation, let number = value as? NSNumber,
                  CFGetTypeID(number) != CFBooleanGetTypeID(),
                  number.doubleValue.rounded() == number.doubleValue,
                  (0...60).contains(number.intValue) else { return nil }
            audioRemaining = number.intValue
        }
        var cleanupIssues = Set<PairingCleanupIssue>()
        if let rawIssues = object["cleanup"] {
            guard phase == .failed, message == "cleanup_unconfirmed",
                  let values = rawIssues as? [String], !values.isEmpty, values.count <= 3 else { return nil }
            for value in values {
                guard let issue = PairingCleanupIssue(rawValue: value), cleanupIssues.insert(issue).inserted else { return nil }
            }
        } else if message == "cleanup_unconfirmed" { return nil }
        return PairingStatus(phase: phase, code: code, acknowledged: ack, remaining: remaining, audioRemaining: audioRemaining,
                             activity: activity, recovery: PairingRecovery.project(message), cleanupIssues: cleanupIssues, rememberedForgetVerified: phase == .cancelled && message == "remembered_forgotten", rememberedCleanupVerified: phase == .closed && message == "remembered_cleanup_verified")
    }
}
