import Foundation

/// The words the Mira home shows for each lifecycle state, kept free of SwiftUI so tests can
/// check that the UI stays truthful. Only a route-verified `.connected` may say "In call".
enum MiraPresentation {
    static func homeStatus(_ presence: MiraPresence, session: MiraHomeSession) -> String {
        switch presence {
        case .closing, .cleanupIncomplete: return status(presence)
        default: break
        }
        if session.savedPairingSession && !presence.isLive { return "Saved pairing" }
        if session.pairedThisSession && !presence.isLive { return "Paired for this session" }
        if session.showsControls && !presence.isLive { return "Session cleanup pending" }
        return status(presence)
    }

    static func homeDetail(_ presence: MiraPresence, session: MiraHomeSession, workerActivity: PairingActivity? = nil,
                           closeRecord: CloseRecord?, lastFailure: CloseReason?) -> String {
        switch presence {
        case .closing, .cleanupIncomplete:
            return detail(presence, workerActivity: workerActivity, closeRecord: closeRecord, lastFailure: lastFailure)
        default: break
        }
        if session.savedPairingSession && !presence.isLive { return "Saved wireless trust is selected. Listener readiness, call continuity, microphone capture and playback are not verified here." }
        if session.pairedThisSession && !presence.isLive { return "The pairing is temporary. Companion cannot verify or join your existing Mira call through a supported route; call continuity and speaker playback remain unverified." }
        return detail(presence, workerActivity: workerActivity, closeRecord: closeRecord, lastFailure: lastFailure)
    }

    static func status(_ presence: MiraPresence) -> String {
        switch presence {
        case .connected: return "In call with Mira"
        case .callAsserted: return "Robot connected · call set by you (not verified)"
        case .robotConnected: return "Robot connected · call not attached"
        case .connecting: return "Connecting to StackChan…"
        case .reconnecting(let n, let of): return "Reconnecting (\(n) of \(of)) · fresh consent required"
        case .closing: return "Closing…"
        case .closed: return "Closed"
        case .blocked: return "Not connected"
        case .readyToConnect: return "Ready"
        case .cleanupIncomplete: return "Cleanup not confirmed"
        case .trustUnknown: return "Pairing status unavailable"
        case .needsPairing, .repairRequired: return "Pairing needed"
        }
    }

    static func workerAudioStatus(_ activity: PairingActivity?) -> String {
        switch activity {
        case .physicalConsent: return "The worker reports waiting for physical consent; microphone capture state is unknown."
        case .conversation: return "The worker reports conversation activity; microphone capture state is unknown."
        case nil: return "Worker audio activity and microphone capture state are unknown."
        }
    }

    static func detail(_ presence: MiraPresence, workerActivity: PairingActivity? = nil, closeRecord: CloseRecord?, lastFailure: CloseReason?) -> String {
        switch presence {
        case .blocked(let missing): return "Companion is paired, but it cannot join your existing Mira call yet. " + missing
        case .cleanupIncomplete(let steps):
            return "Audio worker release is not confirmed: " + steps.sorted().joined(separator: ", ") + ". Microphone capture state is unknown; verify before relaunching."
        case .connected: return Self.workerAudioStatus(workerActivity)
        case .robotConnected:
            return "StackChan is connected to this Mac. Companion is not attached to your Mira call and cannot start, verify or hang it up. Start or join the call yourself and choose StackChan Microphone and StackChan Speaker there. " + Self.workerAudioStatus(workerActivity)
        case .callAsserted:
            return "You said your call is running on this Mac. Companion cannot see it. " + Self.workerAudioStatus(workerActivity)
        case .connecting: return "Opening the link to StackChan. Worker audio activity and microphone capture state are unknown."
        case .reconnecting: return "The link to StackChan dropped. Retrying a few times with a fresh session. The robot must give physical consent again. " + Self.workerAudioStatus(nil)
        case .readyToConnect:
            if let failure = lastFailure { return "Last attempt did not connect because \(failure.label). " + Self.workerAudioStatus(nil) }
            return Self.workerAudioStatus(nil)
        case .trustUnknown: return "Companion could not read its saved pairing. It is not treated as paired or unpaired. Quit and reopen, or pair again if this persists."
        case .repairRequired: return "The saved pairing has expired or is incomplete on this Mac. Pair again when you are ready; nothing starts a call."
        case .needsPairing: return "No saved pairing is available on this Mac. Pair deliberately when you are ready; nothing starts a call."
        case .closed: return closeRecord?.message ?? ""
        case .closing: return "Stopping the audio worker. Worker release is not confirmed; microphone capture state is unknown."
        }
    }
}
