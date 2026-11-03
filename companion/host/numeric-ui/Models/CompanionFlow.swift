import Foundation

enum CompanionStage: String, CaseIterable {
    case preparing, callRequired, pairing, physicalConsent, conversation, restoring, closed
    var title: String {
        switch self {
        case .preparing: return "Preparing Companion"
        case .callRequired: return "Original call required"
        case .pairing: return "Pairing StackChan"
        case .physicalConsent: return "Physical mic consent"
        case .conversation: return "Conversation in progress"
        case .restoring: return "Restoring and verifying"
        case .closed: return "Session closed"
        }
    }
}

enum PairingActivity: String { case physicalConsent = "physical_consent", conversation }

// A worker closing its local resources cannot verify global OS route restoration.
// No production verifier is supplied. The live supervisor must bind its own
// independently verified receipt to this exact attempt before this can close.
struct RestorationReceipt {
    let attemptID: UUID
    let originalRoutesRestored: Bool
    let ownedProcessesClosed: Bool
    let usbReleased: Bool
    let listenerReleased: Bool
    func verifies(_ id: UUID) -> Bool {
        attemptID == id && originalRoutesRestored && ownedProcessesClosed && usbReleased && listenerReleased
    }
}
protocol SessionRestorationVerifying {
    func receipt(for attemptID: UUID) -> RestorationReceipt?
}

enum PairingRecovery: String {
    case approval, paused, trustExpired, trustChanged, network, robotReady, reset
    case rejected, connection, grant, audio, audioBackpressure, protocolFailure, workerExit, setup
    static func project(_ label: String?) -> PairingRecovery? {
        guard let label = label else { return nil }
        switch label {
        case "approval_required", "approval_consumed": return .approval
        case "paused": return .paused
        case "certificate_expired": return .trustExpired
        case "certificate_changed", "unpinned_files": return .trustChanged
        case "lan_unavailable": return .network
        case "ready_missing", "invalid_receipt", "unsupported_receipt": return .robotReady
        case "reset_detected", "window_changed": return .reset
        case "rejected": return .rejected
        case "transport_lost", "pair_connection_closed", "receipt_missing", "incomplete_write": return .connection
        case "grant_failed", "revocation_failed": return .grant
        case "audio_transport_failed", "audio_cleanup_failed": return .audio
        case "audio_backpressure": return .audioBackpressure
        case "protocol_failed", "input_failed", "invalid_message", "invalid_command": return .protocolFailure
        case "worker_exited": return .workerExit
        case "expired", "cancelled", "closed", "audio_complete", "cleanup_unconfirmed",
             "usb_open_may_reset", "verify_robot_then_physical_pair", "physical_pair_required": return nil
        default: return .setup
        }
    }
    var guidance: String {
        switch self {
        case .approval: return "A fresh scoped trial approval is required. Previous approval cannot be reused."
        case .paused: return "Pairing is paused. Ask the trial operator to finish the approved setup before trying again."
        case .trustExpired: return "The trust profile has expired. A separately approved replacement is required before another trial."
        case .trustChanged: return "The reviewed trust files changed. Stop and have the operator verify the exact trust profile."
        case .network: return "The approved network address is unavailable. Have the operator verify the network before a fresh trial."
        case .robotReady: return "No valid robot Ready was received. For the next approved trial, connect the robot, open Companion and leave physical Listen off before pairing."
        case .reset: return "The robot restarted or its pairing window changed. Leave physical Listen off and start a fresh approved attempt with a new Ready check."
        case .rejected: return "The robot rejected pairing. Check both numbers match on the correct robot before a fresh approved attempt."
        case .connection: return "The connection or acknowledgement was lost. Microphone capture state is unknown; use the robot's physical stop if audio may still be active, then verify USB and the approved network before a fresh attempt."
        case .grant: return "Temporary access could not be issued or revoked. The operator must verify revocation before another attempt."
        case .audio: return "Audio stopped. Microphone capture state is unknown until cleanup is verified. Check the original call, audio bridge and restored routes before a fresh trial."
        case .audioBackpressure: return "The worker reported microphone audio backpressure. Capture state is unknown until cleanup is confirmed; verify bridge load and restoration before a fresh trial."
        case .protocolFailure: return "The worker sent an invalid status or command. Microphone capture state is unknown; verify worker closure and route restoration."
        case .workerExit: return "The worker exited unexpectedly. Microphone capture state is unknown; cleanup and route restoration must be independently verified."
        case .setup: return "Preparation or transport failed. Leave physical Listen off and have the operator check the approved setup before a fresh attempt."
        }
    }
}
