import Foundation
import Combine

// Companion shell lifecycle. This file grants no audio authority. It decides what the
// Companion may truthfully show, enforces close ordering, bounds reconnection, and keeps
// late callbacks from an earlier epoch from touching a newer one. Robot physical consent,
// the OS microphone permission and the real audio worker remain owned by existing code.

/// Remembered trust as seen by the launch gate. `.unknown` means the persistent registry
/// could not be read; it is never shown as "Pairing needed" or "Ready".
enum LaunchTrust: Equatable { case absent, stale, valid, unknown }
protocol LaunchTrustProviding { func current() -> LaunchTrust }
/// Fallback when no registry view is supplied (tests, previews).
struct UnavailableLaunchTrust: LaunchTrustProviding { func current() -> LaunchTrust { .absent } }

/// Mira home is always the launch surface. Trust only changes what it truthfully says
/// (pairing needed vs ready/blocked); pairing is a deliberate action from home.
enum LaunchDestination: Equatable {
    case miraLanding
    static func decide(_ trust: LaunchTrust) -> LaunchDestination { .miraLanding }
}

/// Home must retain the worker's controls until its session and cleanup have ended.
/// Pairing is temporary session state, never a durable trust or call-connection receipt.
struct MiraHomeSession {
    let phase: PairingPhase
    let cleanupUnconfirmed: Bool
    var persistentMode = false
    var pairedThisSession: Bool { phase == .paired && !persistentMode }
    var savedPairingSession: Bool { phase == .paired && persistentMode }
    var canOpenSessionControls: Bool { persistentMode && phase == .paired && !cleanupUnconfirmed }
    var showsControls: Bool { phase.busy || cleanupUnconfirmed }
}

/// What a same-call route can really do. `unsupported` is the production truth for a
/// verified route: no start-original-Dot-call, hangup or external raw-audio attachment API
/// exists. `manualAttach` (Path A) means the user runs the call in a client on this Mac and
/// picks the StackChan devices themselves; Companion can neither verify, start nor end it.
enum SameCallCapability: Equatable {
    case unsupported(missing: String)
    case manualAttach(routeID: String)
    case supported(routeID: String, hangup: Bool, returnAudio: Bool)
    var isSupported: Bool { if case .supported = self { return true }; return false }
    var isManual: Bool { if case .manualAttach = self { return true }; return false }
    /// A route the user may open a session on (verified or manual).
    var allowsOpen: Bool { if case .unsupported = self { return false }; return true }
    var canHangUp: Bool { if case .supported(_, let h, _) = self { return h }; return false }
    var returnsAudio: Bool { if case .supported(_, _, let r) = self { return r }; return false }
}
enum SameCallEvent: Equatable {
    case connected      // verified by a supported route only
    case userAsserted   // the user said the call is running; not verified
    case failed, ended
}

/// Whether the call is attached to this epoch. `userAsserted` is a human claim and never
/// counts as a verified call. WSS/PCM readiness never changes this value.
enum CallAttachment: Equatable { case notAttached, userAsserted, attached }

protocol SameCallRoute: AnyObject {
    var capability: SameCallCapability { get }
    /// Starts an attempt. Results must be reported back through `report` with the same epoch.
    func connect(epoch: UInt64, report: @escaping (UInt64, SameCallEvent) -> Void)
    /// Requests hangup of exactly this epoch's call. Only called when `capability.canHangUp`.
    func hangUp(epoch: UInt64, completion: @escaping (Bool) -> Void)
}

/// Optional capability of a manual route: the user attests to the state of their own call.
protocol ManualCallAttesting: AnyObject {
    func userAssertsCallRunning()
    func userReportsCallEnded()
}

/// The production route when nothing else exists: honest about the missing capability.
final class UnsupportedSameCallRoute: SameCallRoute {
    static let missing = "A supported way to start or join the user's existing Mira call and hang it up (and to attach raw two-way audio to it) is not available. WSS/PCM readiness and the local native_session_ref do not constitute a call."
    var capability: SameCallCapability { .unsupported(missing: Self.missing) }
    func connect(epoch: UInt64, report: @escaping (UInt64, SameCallEvent) -> Void) { }
    func hangUp(epoch: UInt64, completion: @escaping (Bool) -> Void) { completion(true) }
}

/// Path A. The user runs the Mira call in a client on this Mac and selects StackChan
/// Microphone / StackChan Speaker there. This route cannot see the call, so it reports
/// nothing by itself: no `.connected` is ever emitted, only an explicit user assertion.
final class ManualMacCallRoute: SameCallRoute, ManualCallAttesting {
    static let routeID = "manual-mac"
    private var epoch: UInt64?
    private var report: ((UInt64, SameCallEvent) -> Void)?
    var capability: SameCallCapability { .manualAttach(routeID: Self.routeID) }
    func connect(epoch: UInt64, report: @escaping (UInt64, SameCallEvent) -> Void) {
        self.epoch = epoch; self.report = report     // bookkeeping only; nothing is started
    }
    /// Unreachable through the lifecycle (`canHangUp` is false). Reports that nothing was hung up.
    func hangUp(epoch: UInt64, completion: @escaping (Bool) -> Void) { completion(false) }
    func userAssertsCallRunning() { if let e = epoch { report?(e, .userAsserted) } }
    func userReportsCallEnded() { if let e = epoch { report?(e, .ended) } }
}

/// Opens the robot link (the wireless listener) for one epoch and reports its state back
/// with that epoch. Production adapts the pairing worker; it never grants consent.
enum RobotLinkEvent: Equatable { case connected, lost(CloseReason) }
protocol RobotLinkOpening: AnyObject {
    func openRobotLink(epoch: UInt64, report: @escaping (UInt64, RobotLinkEvent) -> Void)
}
final class NoRobotLink: RobotLinkOpening {
    func openRobotLink(epoch: UInt64, report: @escaping (UInt64, RobotLinkEvent) -> Void) { }
}

/// Owned local resources. The worker owns capture, playback and the socket, so the only
/// supported stop is its single atomic shutdown. `done(true)` means the exact worker exit
/// confirmed release; there are no separate mute/capture/playback confirmations.
protocol OwnedAudioResources: AnyObject {
    func shutdownAndConfirmRelease(_ done: @escaping (Bool) -> Void)
}

enum CloseStep: String, Equatable { case workerRelease, hangup }

/// Fixed-label reason a session ended or a link was lost. Raw values are stable codes
/// safe to log: no payloads, addresses or secrets.
enum CloseReason: String, Equatable, CaseIterable {
    case userClosed = "user_closed"
    case appQuit = "app_quit"
    case robotClosed = "robot_closed"           // robot-side stop (physical or its own timer)
    case robotReset = "robot_reset"
    case wifiLost = "wifi_lost"
    case consentTimeout = "consent_timeout"
    case sessionLimit = "session_limit"
    case callEnded = "call_ended"
    case routeFailed = "route_failed"
    case protocolError = "protocol_error"
    case reconnectExhausted = "reconnect_exhausted"
    case cleanupFailed = "cleanup_failed"

    var initiator: CloseInitiator {
        switch self {
        case .userClosed: return .user
        case .appQuit, .reconnectExhausted, .cleanupFailed: return .app
        case .robotClosed, .robotReset: return .robot
        case .wifiLost: return .network
        case .callEnded, .routeFailed: return .call
        case .consentTimeout, .sessionLimit, .protocolError: return .worker
        }
    }
    /// A short bounded reconnect into the muted state is allowed only for these.
    var reconnectEligible: Bool { self == .wifiLost || self == .robotReset }
    /// A robot reset always means a brand-new attempt, never a resumed session.
    var forcesFreshAttempt: Bool { self == .robotReset }
    var label: String {
        switch self {
        case .userClosed: return "you closed it"
        case .appQuit: return "the app quit"
        case .robotClosed: return "the robot ended it"
        case .robotReset: return "the robot reset"
        case .wifiLost: return "Wi-Fi was lost"
        case .consentTimeout: return "robot consent timed out"
        case .sessionLimit: return "the session limit was reached"
        case .callEnded: return "the call ended"
        case .routeFailed: return "the call route failed"
        case .protocolError: return "the worker reported a protocol error"
        case .reconnectExhausted: return "reconnecting did not succeed"
        case .cleanupFailed: return "cleanup was not confirmed"
        }
    }
}
enum CloseInitiator: String, Equatable { case user, app, robot, network, call, worker }

/// What can truthfully be said about the call when the session closed.
enum CallAfterClose: String, Equatable {
    case hungUp                 // a supported route hung up this epoch's call
    case keepsRunning           // the user said a call was running and no hangup route exists
    case mayKeepRunning         // a session was open, no hangup route, call state unknown
    case none                   // no session was ever open
}

struct CloseRecord: Equatable {
    let reason: CloseReason
    let epoch: UInt64
    let released: Bool          // exact worker exit confirmed
    let call: CallAfterClose
    var initiator: CloseInitiator { reason.initiator }
    var message: String {
        let head = released ? "Closed because \(reason.label). StackChan is released." : "Closed because \(reason.label), but release was not confirmed."
        switch call {
        case .keepsRunning: return head + " Your Mira call is still running: Companion has no way to hang it up. End it in your call app."
        case .mayKeepRunning: return head + " If a Mira call is running it keeps running: Companion has no way to hang it up. End it in your call app."
        case .hungUp: return head + " The call was hung up."
        case .none: return head
        }
    }
}

struct ReconnectPolicy: Equatable {
    var maxAttempts: Int            // per loss event
    var delays: [TimeInterval]      // before attempt n (the last repeats)
    var window: TimeInterval        // max time from first loss to giving up
    var attemptTimeout: TimeInterval // an attempt that reports nothing in this time has failed
    var maxPerSession: Int          // total reconnect attempts across the session
    static let standard = ReconnectPolicy(maxAttempts: 3, delays: [2, 5, 10], window: 60, attemptTimeout: 20, maxPerSession: 8)
    func delay(forAttempt n: Int) -> TimeInterval { delays.isEmpty ? 0 : delays[min(max(n - 1, 0), delays.count - 1)] }
}
protocol ReconnectScheduling: AnyObject {
    /// `fire` runs once after `delay`. The lifecycle ignores fires it has invalidated.
    func schedule(after delay: TimeInterval, _ fire: @escaping () -> Void)
}
final class DispatchReconnectScheduler: ReconnectScheduling {
    func schedule(after delay: TimeInterval, _ fire: @escaping () -> Void) {
        DispatchQueue.main.asyncAfter(deadline: .now() + delay, execute: fire)
    }
}

enum MiraPresence: Equatable {
    case needsPairing
    case repairRequired
    case trustUnknown               // registry unreadable; neither "paired" nor "needs pairing"
    case blocked(missing: String)   // trusted, but no route (not even a manual one)
    case readyToConnect
    case connecting                 // opening the robot link
    case robotConnected             // robot link up, call NOT attached
    case callAsserted               // robot link up, user says a call is running (unverified)
    case connected                  // robot link up and a supported route verified the call
    case reconnecting(attempt: Int, of: Int)
    case closing
    case closed
    case cleanupIncomplete(Set<String>)
    /// Only a route-verified call is "In call". WSS/PCM readiness and user claims are not.
    var showsActiveCall: Bool { self == .connected }
    var isLive: Bool {
        switch self {
        case .connecting, .robotConnected, .callAsserted, .connected, .reconnecting: return true
        default: return false
        }
    }
}

// MARK: Microphone state strip

enum RobotMicState: String, Equatable { case unknown, off, armed, streaming, suppressedForPlayback, conflictingTelemetry }
enum HostBridgeState: String, Equatable { case idle, open }
/// The call's own mute lives inside the call app and cannot be read from here.
enum CallMicState: String, Equatable { case unknown }

struct MicrophoneStrip: Equatable {
    let robot: RobotMicState
    let bridge: HostBridgeState
    let call: CallMicState
    var robotText: String {
        switch robot {
        case .unknown: return "Robot mic: capture state unknown (worker does not report capture status)"
        case .off: return "Robot mic: off"
        case .armed: return "Robot mic: armed (nothing flowing yet)"
        case .streaming: return "Robot mic: streaming"
        case .suppressedForPlayback: return "Robot mic: paused while StackChan plays sound"
        case .conflictingTelemetry: return "Robot mic: conflicting worker reports (capture flow and playback suppression)"
        }
    }
    var bridgeText: String { bridge == .open ? "Host bridge: open" : "Host bridge: idle" }
    var callText: String { "Call mic: unknown (set inside your call app)" }
    var lines: [String] { [robotText, bridgeText, callText] }
}

/// Inert speaker-return path. It counts and discards; no audio is routed here.
struct SpeakerPacket: Equatable { let epoch: UInt64; let sequence: UInt64 }
final class SpeakerPath {
    private(set) var enabledEpoch: UInt64?
    private(set) var accepted = 0
    private(set) var droppedStale = 0
    private(set) var droppedDisabled = 0
    private(set) var lastSequence: UInt64 = 0
    var isEnabled: Bool { enabledEpoch != nil }
    func enable(epoch: UInt64) { enabledEpoch = epoch; lastSequence = 0 }
    func disable() { enabledEpoch = nil; lastSequence = 0 }
    func accept(_ packet: SpeakerPacket) -> Bool {
        guard let epoch = enabledEpoch else { droppedDisabled += 1; return false }
        guard packet.epoch == epoch, packet.sequence > lastSequence else { droppedStale += 1; return false }
        lastSequence = packet.sequence; accepted += 1
        return true
    }
}

final class CompanionLifecycle: ObservableObject {
    @Published private(set) var presence: MiraPresence
    @Published private(set) var muted = true            // every open and every reconnect starts muted
    @Published private(set) var physicalConsent = false // set only by the existing robot-consent path
    @Published private(set) var speakerEnabled = false
    @Published private(set) var robotLinked = false
    @Published private(set) var call: CallAttachment = .notAttached
    @Published private(set) var captureFlowing = false
    @Published private(set) var playbackSuppressing = false
    @Published private(set) var closeRecord: CloseRecord?
    @Published private(set) var lastFailure: CloseReason?    // a first open that never connected
    private(set) var epoch: UInt64 = 0
    private(set) var closeLog: [CloseStep] = []
    private(set) var reconnectsThisSession = 0
    let speaker = SpeakerPath()
    private(set) var destination: LaunchDestination
    private let trust: LaunchTrustProviding
    private let route: SameCallRoute
    private let link: RobotLinkOpening
    private let resources: OwnedAudioResources
    private let scheduler: ReconnectScheduling
    private let policy: ReconnectPolicy
    private let now: () -> TimeInterval
    private var closeInFlight = false
    private var timerGeneration: UInt64 = 0
    private var attemptsThisLoss = 0
    private var lossStartedAt: TimeInterval?
    private var sessionWasOpen = false
    private var sessionCallClaimed = false      // a call was attached/asserted in some epoch of this session

    init(trust: LaunchTrustProviding = UnavailableLaunchTrust(), route: SameCallRoute = UnsupportedSameCallRoute(),
         link: RobotLinkOpening = NoRobotLink(), scheduler: ReconnectScheduling = DispatchReconnectScheduler(),
         policy: ReconnectPolicy = .standard, now: @escaping () -> TimeInterval = { ProcessInfo.processInfo.systemUptime },
         resources: OwnedAudioResources) {
        self.trust = trust; self.route = route; self.link = link; self.resources = resources
        self.scheduler = scheduler; self.policy = policy; self.now = now
        let launchTrust = trust.current()
        destination = LaunchDestination.decide(launchTrust)
        presence = Self.idlePresence(launchTrust, route.capability)
    }

    private static func idlePresence(_ trust: LaunchTrust, _ capability: SameCallCapability) -> MiraPresence {
        switch trust {
        case .absent: return .needsPairing
        case .stale: return .repairRequired
        case .unknown: return .trustUnknown
        case .valid:
            if case .unsupported(let m) = capability { return .blocked(missing: m) }
            return .readyToConnect
        }
    }

    /// Re-reads the persistent registry view. Only affects a session that is not open and not
    /// already closed, so a late registry change cannot revive or disturb a live session.
    func refreshTrust() {
        switch presence {
        case .needsPairing, .repairRequired, .trustUnknown, .blocked, .readyToConnect:
            let t = trust.current()
            destination = LaunchDestination.decide(t)
            presence = Self.idlePresence(t, route.capability)
        default: break
        }
    }

    var canOpen: Bool { presence == .readyToConnect }
    /// Capture may be armed only with the robot linked, after robot consent, while unmuted,
    /// and after a call is attached or the user has said one is running. A reconnect always
    /// restarts at muted without consent, so it can never re-arm by itself.
    var captureMayArm: Bool { presence.isLive && robotLinked && physicalConsent && !muted && call != .notAttached }
    private var canUnmute: Bool { presence.isLive && robotLinked && physicalConsent && call != .notAttached }

    var micStrip: MicrophoneStrip {
        let robot: RobotMicState
        if captureFlowing && playbackSuppressing { robot = .conflictingTelemetry }
        else if playbackSuppressing { robot = .suppressedForPlayback }
        else if captureFlowing { robot = .streaming }
        else { robot = .unknown }
        return MicrophoneStrip(robot: robot, bridge: robotLinked ? .open : .idle, call: .unknown)
    }

    /// Explicit user gesture (button). Never invoked implicitly by launch.
    func open() {
        guard canOpen, epoch < UInt64.max - 2 else { return }
        attemptsThisLoss = 0; lossStartedAt = nil; reconnectsThisSession = 0
        closeRecord = nil; lastFailure = nil; closeLog = []; sessionWasOpen = true; sessionCallClaimed = false
        beginEpoch()
        presence = .connecting
        startAttempt(epoch: epoch)
    }

    /// Each epoch starts muted, without consent, with no link and no call attached.
    private func beginEpoch() {
        epoch += 1; timerGeneration += 1
        robotLinked = false; call = .notAttached
        disarm()
    }
    private func startAttempt(epoch mine: UInt64) {
        route.connect(epoch: mine) { [weak self] e, event in self?.routeReported(e, event) }
        link.openRobotLink(epoch: mine) { [weak self] e, event in self?.robotLinkReported(e, event) }
    }
    private func disarm() {
        muted = true; physicalConsent = false; captureFlowing = false; playbackSuppressing = false; disableSpeaker()
    }

    private func refreshPresence() {
        guard presence.isLive else { return }
        if !robotLinked {
            if case .reconnecting = presence { return }
            presence = .connecting; return
        }
        switch call {
        case .attached: presence = .connected
        case .userAsserted: presence = .callAsserted
        case .notAttached: presence = .robotConnected
        }
    }

    func routeReported(_ e: UInt64, _ event: SameCallEvent) {
        guard e == epoch, !closeInFlight else { return }
        switch event {
        case .connected:
            // Only a supported route can verify a call; a manual route never can.
            guard route.capability.isSupported, presence.isLive else { return }
            call = .attached; sessionCallClaimed = true; refreshPresence()
        case .userAsserted:
            guard route.capability.isManual, presence.isLive, call != .attached else { return }
            call = .userAsserted; sessionCallClaimed = true; refreshPresence()
        case .failed:
            if presence == .connecting && !robotLinked && call == .notAttached {
                disarm(); timerGeneration += 1; lastFailure = .routeFailed
                presence = route.capability.allowsOpen ? .readyToConnect : .blocked(missing: "Connection failed")
            } else if presence.isLive { disarm(); close(reason: .routeFailed) }
        case .ended:
            guard presence.isLive else { return }
            // Lost call: disarm everything now; leave deliberate reopen to the user.
            sessionCallClaimed = false; disarm(); close(reason: .callEnded)
        }
    }

    func robotLinkReported(_ e: UInt64, _ event: RobotLinkEvent) {
        guard e == epoch, !closeInFlight, presence.isLive else { return }
        switch event {
        case .connected:
            robotLinked = true; timerGeneration += 1      // cancels any pending attempt timeout
            attemptsThisLoss = 0; lossStartedAt = nil
            refreshPresence()
        case .lost(let reason):
            let neverConnected = presence == .connecting && !robotLinked
            disarm(); robotLinked = false
            if neverConnected {
                // First open failed before any link existed: nothing to reconnect to.
                timerGeneration += 1; lastFailure = reason
                presence = Self.idlePresence(trust.current(), route.capability)
            } else if reason.reconnectEligible {
                beginEpoch()                    // the old epoch is dead now: late reports for it are stale
                scheduleReconnect(reason)       // also reached when a reconnect attempt itself fails
            } else {
                close(reason: reason)
            }
        }
    }

    private func scheduleReconnect(_ reason: CloseReason) {
        let t = now()
        if reason.forcesFreshAttempt { attemptsThisLoss = 0 }
        let start = lossStartedAt ?? t
        lossStartedAt = start
        if attemptsThisLoss >= policy.maxAttempts || reconnectsThisSession >= policy.maxPerSession || t - start > policy.window {
            close(reason: .reconnectExhausted); return
        }
        let attempt = attemptsThisLoss + 1
        timerGeneration += 1
        let generation = timerGeneration
        presence = .reconnecting(attempt: attempt, of: policy.maxAttempts)
        scheduler.schedule(after: reason.forcesFreshAttempt ? 0 : policy.delay(forAttempt: attempt)) { [weak self] in
            self?.reconnectFired(generation)
        }
    }

    private func reconnectFired(_ generation: UInt64) {
        guard generation == timerGeneration, !closeInFlight, case .reconnecting = presence else { return }
        attemptsThisLoss += 1; reconnectsThisSession += 1
        beginEpoch()                    // fresh epoch: muted, no consent, no call; old callbacks are stale
        let mine = epoch, attemptGeneration = timerGeneration
        startAttempt(epoch: mine)
        // An attempt that never reports is a failed attempt, so retrying stays bounded.
        scheduler.schedule(after: policy.attemptTimeout) { [weak self] in
            guard let self = self, attemptGeneration == self.timerGeneration, self.epoch == mine,
                  case .reconnecting = self.presence, !self.closeInFlight else { return }
            self.robotLinkReported(mine, .lost(.wifiLost))
        }
    }

    /// Existing robot protocol reports physical consent; this object never grants it.
    func robotConsentReported(epoch e: UInt64) {
        guard e == epoch, robotLinked, presence.isLive else { return }
        physicalConsent = true
    }
    /// The worker returned to its physical-consent state, so the previous grant ended.
    /// Keep the link/call presence, but disarm until fresh physical consent and an explicit
    /// unmute arrive.
    func robotConsentRevoked(epoch e: UInt64) {
        guard e == epoch, robotLinked, presence.isLive else { return }
        disarm()
    }
    /// The user's statement about their own call (Path A). Ignored unless the route is manual.
    func attestCall(running: Bool) {
        guard let manual = route as? ManualCallAttesting, presence.isLive else { return }
        if running { manual.userAssertsCallRunning() } else { manual.userReportsCallEnded() }
    }
    /// Bridge activity reported by the existing worker; informational, never grants anything.
    func bridgeActivityReported(epoch e: UInt64, captureFlowing flowing: Bool, playbackSuppressing suppressing: Bool) {
        guard e == epoch, robotLinked, presence.isLive else { return }
        // These are worker observations, not commands. Show the observed flow even if
        // Companion's separate local consent projection says it should not be armed.
        captureFlowing = flowing; playbackSuppressing = suppressing
    }
    /// Updates only the lifecycle's local projection; it does not command the audio worker.
    func setMuted(_ value: Bool) {
        if value { muted = true; disableSpeaker(); return }
        guard canUnmute else { return }
        muted = false
    }
    /// There is no separately acknowledged worker mute command. Stop the owned audio worker
    /// and wait for its exact process exit before reporting the Companion session released.
    func stopAudioSession(_ finished: ((Bool) -> Void)? = nil) {
        close(reason: .userClosed, finished)
    }
    func setSpeaker(_ on: Bool) {
        if !on { disableSpeaker(); return }
        guard presence.isLive, robotLinked, physicalConsent, !muted, route.capability.returnsAudio else { return }
        speaker.enable(epoch: epoch); speakerEnabled = true
    }
    private func disableSpeaker() { speaker.disable(); speakerEnabled = false }

    /// Window close and Quit. Idempotent. The local state is disarmed synchronously and the
    /// single atomic worker shutdown starts immediately; "closed" is reported only after the
    /// exact worker exit confirms release.
    func close(reason: CloseReason = .userClosed, _ finished: ((Bool) -> Void)? = nil) {
        // Pairing views can start a new worker after Stop closed the call lifecycle.
        // Recheck the shared resource owner on Quit instead of trusting that older close.
        if presence == .closed {
            if closeInFlight { finished?(false); return }
            closeInFlight = true
            resources.shutdownAndConfirmRelease { [weak self] ok in
                guard let self = self else { finished?(ok); return }
                self.closeInFlight = false
                self.closeLog.append(.workerRelease)
                if !ok {
                    self.closeRecord = CloseRecord(reason: reason, epoch: self.epoch, released: false,
                                                   call: self.closeRecord?.call ?? .none)
                    self.presence = .cleanupIncomplete([CloseStep.workerRelease.rawValue])
                }
                finished?(ok)
            }
            return
        }
        if closeInFlight { finished?(false); return }
        closeInFlight = true
        let hadCall = presence.isLive
        let callWasAsserted = sessionCallClaimed
        let canHangUp = route.capability.canHangUp
        epoch += 1; timerGeneration += 1             // late connect/packet/timer callbacks are now stale
        let closingEpoch = epoch - 1
        disarm(); robotLinked = false; call = .notAttached; presence = .closing
        var issues = Set<String>()
        var hungUp = false
        let finish: () -> Void = { [weak self] in
            guard let self = self else { finished?(issues.isEmpty); return }
            self.closeInFlight = false
            let after: CallAfterClose
            if hungUp { after = .hungUp }
            else if canHangUp { after = .none }
            else if callWasAsserted { after = .keepsRunning }
            else if hadCall || self.sessionWasOpen { after = .mayKeepRunning }
            else { after = .none }
            self.closeRecord = CloseRecord(reason: reason, epoch: closingEpoch, released: issues.isEmpty, call: after)
            self.presence = issues.isEmpty ? .closed : .cleanupIncomplete(issues)
            finished?(issues.isEmpty)
        }
        resources.shutdownAndConfirmRelease { [weak self] ok in
            guard let self = self else { finish(); return }
            self.closeLog.append(.workerRelease)
            if !ok { issues.insert(CloseStep.workerRelease.rawValue) }
            if hadCall && canHangUp {
                self.route.hangUp(epoch: closingEpoch) { [weak self] ok in
                    self?.closeLog.append(.hangup)
                    if ok { hungUp = true } else { issues.insert(CloseStep.hangup.rawValue) }
                    finish()
                }
            } else { finish() }
        }
    }
}
