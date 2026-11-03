import Foundation
import Combine
import Darwin

final class PairingController: ObservableObject {
    // No transport/clock/approved-extension/checked-stop evidence yet.
    // These hooks cannot request capture; readiness starts and remains false.
    lazy var webcamFramePipeline=CompanionWebcamPipeline(owner:nil,verifiedInstalledBridge:webcamBridge,extensionApproved:false) // never ready without actual issuer/approval
    let webcamBridge=WebcamBridgeAdapter() // inert; attach only verified installed sink
    lazy var webcam = WebcamControls(begin: { [weak self] in
        guard let self, self.webcamFramePipeline.explicitWebcamOn() else { return false }
        self.send("webcam_on");return true
    }, revoke: { [weak self] in
        guard let self else { return false }
        let hadVideo=self.webcamFramePipeline.binding != nil
        let stopped=self.webcamFramePipeline.revoke()
        _=self.webcamBridge.revokeLocalExtension()
        if hadVideo && self.child != nil && self.persistentMode { self.send("webcam_off") }
        return stopped
    })
    let gestures=GesturePerceptionControls(receiver:CompanionThumbnailReceiver(owner:ExistingWorkerThumbnailUnavailable(),clock:{ ProcessInfo.processInfo.systemUptime }))
    // Inert construction; no optIn, frame submit, processLatest or model request.
    let localVision = CompanionVisionService(currentAuthenticatedCameraOwner: { _ in false },
        clock: { ProcessInfo.processInfo.systemUptime }, requestIntent: { _ in })
    @Published private(set) var cameraPermissionFeedback = "Camera permission unavailable · device confirmation and physical qualification pending"
    @Published private(set) var phase: PairingPhase = .idle
    @Published private(set) var code: String?
    @Published private(set) var remaining: Int?
    @Published private(set) var confirmationSent = false
    @Published private(set) var cleanupPending = false
    @Published private(set) var cleanupIssues: Set<PairingCleanupIssue> = []
    @Published private(set) var activity: PairingActivity?
    @Published private(set) var audioRemaining: Int?
    @Published private(set) var recovery: PairingRecovery?
    @Published private(set) var originalCallConfirmed = false
    @Published private(set) var audioRouteEndpointsAvailable = false
    @Published private(set) var restorationConfirmed = false
    @Published private(set) var rememberRequested = false
    @Published private(set) var rememberFeedback: RememberedDeviceFeedback = .unavailable
    private var forgetAwaitingAcknowledgement = false
    @Published private(set) var persistentMode = false
    @Published private(set) var rememberedStatus = "Ready for manual enrollment. No audio starts during enrollment."
    private var audioDeviceRoute = AudioDeviceRoute(endpointsAvailable: false)
    private let audioRouteAvailability: () -> Bool
    private var candidateHost: URL { URL(fileURLWithPath: #filePath).deletingLastPathComponent().deletingLastPathComponent().deletingLastPathComponent() }
    var workerLaunchEnvironment: [String: String] {
        var environment = ProcessInfo.processInfo.environment
        environment["STACKCHAN_ROOT"] = root.path
        environment["PYTHONDONTWRITEBYTECODE"] = "1"
        environment["PYTHONPATH"] = [URL(fileURLWithPath: environment["HOME_LAB_ROOT"] ?? "/nonexistent/stackchan-home-lab").appendingPathComponent("src").path, root.appendingPathComponent("bridge/host-adapter").path,
          root.appendingPathComponent("bridge/local-endpoint").path, root.appendingPathComponent(".tools/bridge-python").path,
          candidateHost.appendingPathComponent("native-audio").path].joined(separator: ":")
        return environment
    }
    var rememberedWorkerLaunchArguments: [String] { ["-B", candidateHost.appendingPathComponent("streaming/remembered_pairing_worker.py").path] }
    var canEnrollRemembered: Bool { !cleanupPending && cleanupIssues.isEmpty && (child == nil || (persistentMode && (phase == .idle || phase.terminal))) }
    var canConnectRemembered: Bool { canUseStackChan }
    var canUseStackChan: Bool { audioDeviceRoute.canUseStackChan && !cleanupPending && cleanupIssues.isEmpty && (child == nil || (persistentMode && (phase == .idle || phase.terminal || (phase == .paired && activity == nil)))) }
    func enrollRemembered() {
        guard canEnrollRemembered else { return }
        persistentMode = true
        if child == nil { start() }
        if child != nil { phase = .waiting_robot; rememberedStatus = "On StackChan, choose Pair and Remember. Compare the number and confirm on both screens."; send("enroll") }
    }
    func confirmRemembered() {
        guard persistentMode, phase == .comparing, !confirmationSent else { return }
        confirmationSent = true; send("confirm")
    }
    func refreshAudioRouteAvailability() {
        let available = audioRouteAvailability()
        audioDeviceRoute = AudioDeviceRoute(endpointsAvailable: available)
        audioRouteEndpointsAvailable = available
    }
    func useStackChan() {
        guard canUseStackChan, let payload = audioDeviceRoute.workerPayload() else { return }
        persistentMode = true
        if child == nil { start() }
        if child != nil { phase = .opening; rememberedStatus = "Preparing the StackChan audio listener. Physical Listen is required to enable audio."; send(payload) }
    }
    private var rememberedPairing = RememberedPairing()
    private let restorationVerifier: SessionRestorationVerifying?
    private(set) var attemptID = UUID()
    let approved: Bool
    let audioApproved: Bool
    let audioMode: CompanionAudioMode
    let oneAttempt: Bool
    private let root: URL
    private var child: Process?
    private var input: FileHandle?
    private var output: FileHandle?
    private var buffer = Data()
    // Every shutdown request joins one in-flight stop: all completions are kept and each runs once.
    private var shutdownCompletions: [(Bool) -> Void] = []
    private var escalationGeneration: UUID?
    private(set) var escalationSchedules = 0
    // Test-only hooks; the defaults are the production behavior. `escalationDelays` shortens the
    // terminate/kill/deadline timers. `finalizationGate`, when set, holds every pipe read and the
    // termination handler's retirement until the test opens it (bounded to 5 s per waiter).
    var escalationDelays: (terminate: Double, kill: Double, deadline: Double) = (4, 6, 8)
    var finalizationGate: DispatchSemaphore?
    private var generation = UUID()
    private var requiresProcessExit = false
    private var lastWorkerExitConfirmed: Bool?
    private var receivedUptime = ProcessInfo.processInfo.systemUptime
    private let processFactory: (() -> Process)?
    init(approved: Bool, root: URL, processFactory: (() -> Process)? = nil, audioApproved: Bool = false, oneAttempt: Bool = true, audioMode: CompanionAudioMode = .transportTest, restorationVerifier: SessionRestorationVerifying? = nil, audioRouteAvailability: @escaping () -> Bool = { AudioRouteCatalog.requiredEndpointsAvailable() }) { self.restorationVerifier = restorationVerifier; self.audioMode = audioMode; self.oneAttempt = oneAttempt; self.approved = approved; self.audioApproved = approved && audioApproved; self.root = root; self.processFactory = processFactory; self.audioRouteAvailability = audioRouteAvailability }
    var audioDisclosure: String {
        if audioMode == .nativeConversation {
            return "Microphone audio goes to your original dot voice call. That call's audio plays on StackChan. This app does not record audio. Match the number on both screens. The robot’s Pair & authorize mic/speaker confirmation gives physical consent for this temporary session."
        }
        return "Transport test: microphone frames are counted and discarded locally. The prepared test cue plays on StackChan. No audio recording or forwarding to a voice call. Confirm the matching number on both screens to permit this temporary test."
    }
    var audioSubtitle: String {
        audioMode == .nativeConversation ? "Original dot call · Microphone and speaker" : "Transport test · Local cue and microphone discard"
    }
    var workerLaunchArguments: [String] {
        let worker = candidateHost.appendingPathComponent("streaming/numeric_pairing_worker.py")
        var args = ["-B", worker.path, "--approved-test", "--persistent-trust", "--port", "/dev/cu.usbmodem31201",
            "--cert", root.appendingPathComponent(".tools/persistent-companion/server.pem").path,
            "--key", root.appendingPathComponent(".tools/persistent-companion/server-key.pem").path,
            "--cue", root.appendingPathComponent(".tools/native-call-preparation/transport-speaker-cue.wav").path]
        if audioApproved { args += ["--approved-audio", "--audio-mode", audioMode.rawValue] }
        return args
    }
    var requiresOriginalCall: Bool { audioApproved && audioMode == .nativeConversation && !originalCallConfirmed }
    var stage: CompanionStage {
        if persistentMode {
            if cleanupPending || !cleanupIssues.isEmpty { return .restoring }
            if phase == .paired { return activity == .conversation ? .conversation : (activity == .physicalConsent ? .physicalConsent : .closed) }
            return phase.terminal ? .closed : .pairing
        }
        if !approved { return .preparing }
        if cleanupPending || phase == .restoring || !cleanupIssues.isEmpty { return .restoring }
        if phase.terminal {
            return audioApproved && audioMode == .nativeConversation && !restorationConfirmed ? .restoring : .closed
        }
        if requiresOriginalCall && phase == .idle { return .callRequired }
        if phase == .idle || phase == .opening { return .preparing }
        if phase == .paired && audioApproved { return activity == .conversation ? .conversation : .physicalConsent }
        return .pairing
    }
    var readyNow: Bool { approved && phase == .idle && !requiresOriginalCall && !cleanupPending && cleanupIssues.isEmpty }
    var canAcknowledgeCall: Bool { approved && requiresOriginalCall && !cleanupPending && cleanupIssues.isEmpty && (phase == .idle || (persistentMode && (phase == .paired || phase == .closed))) }
    func acknowledgeOriginalCall() {
        guard canAcknowledgeCall else { return }
        originalCallConfirmed = true
    }
    func refreshRestoration() {
        guard phase.terminal, child == nil, cleanupIssues.isEmpty, !cleanupPending,
              restorationVerifier?.receipt(for: attemptID)?.verifies(attemptID) == true else { return }
        restorationConfirmed = true
    }
    func setRememberRequested(_ enabled: Bool) {
        rememberedPairing.requestRemember(enabled)
        rememberRequested = rememberedPairing.requested; rememberFeedback = rememberedPairing.feedback
    }
    var canForgetRemembered: Bool {
        if persistentMode && cleanupIssues == [.grant] { return true }
        return !cleanupPending && cleanupIssues.isEmpty && (child == nil || persistentMode)
    }
    func forgetRemembered() {
        webcam.ownerLost(); localVision.disable(); gestures.ownerLost()
        guard canForgetRemembered else { return }
        if child == nil { persistentMode = true; start() }
        if persistentMode, child != nil {
            forgetAwaitingAcknowledgement = true; send("forget"); rememberedStatus = "Revoking Mac trust and closing owned sessions. Also choose Forget on StackChan."; return
        }
    }
    func forgetDevice() {
        rememberedPairing.forget()
        rememberRequested = rememberedPairing.requested; rememberFeedback = rememberedPairing.feedback
    }
    var canConfirm: Bool { approved && phase == .comparing && code != nil && !confirmationSent }
    var primaryTitle: String {
        if !cleanupIssues.isEmpty { return "Cleanup needed" }
        if requiresOriginalCall && phase == .idle { return "Call required" }
        if stage == .conversation { return "Conversation active" }
        if stage == .restoring { return "Verifying cleanup" }
        if phase == .comparing || phase == .waiting_confirmation { return confirmationSent ? "Waiting for StackChan" : (audioApproved ? "Confirm & connect" : "Confirm match") }
        if phase.terminal { return oneAttempt ? "New approval needed" : "Retry pairing" }
        return phase == .idle ? "Pair StackChan" : "Pairing…"
    }
    var canPrimary: Bool { !persistentMode && approved && !requiresOriginalCall && !requiresProcessExit && !cleanupPending && cleanupIssues.isEmpty && !(phase.terminal && stage == .restoring) && (phase == .idle || (phase.terminal && !oneAttempt) || canConfirm) }
    var canCancel: Bool { (approved || persistentMode) && phase.busy && phase != .restoring && !cleanupPending }
    var instruction: String {
        if persistentMode { return rememberedStatus }
        if !approved { return "Prepared preview. Pairing needs approval for one temporary USB/Wi-Fi check." }
        if !cleanupIssues.isEmpty {
            var checks = [String]()
            if cleanupIssues.contains(.audio) { checks.append("confirm the original microphone and speaker routes are restored and the worker has released audio") }
            if cleanupIssues.contains(.grant) { checks.append("verify the temporary pairing grant was revoked") }
            if cleanupIssues.contains(.usb) { checks.append("verify the USB pairing session is closed") }
            return "Cleanup is not confirmed. Do not try again until you " + checks.joined(separator: ", and ") + "."
        }
        if stage == .restoring {
            return "The local session is stopping or closed. Microphone capture state is unknown until worker release is confirmed. The operator must independently verify original microphone/speaker routes and owned process, USB and listener closure. This app cannot yet verify that supervisor receipt."
        }
        if requiresOriginalCall && phase == .idle {
            return "After the operator completes the approved route preparation, start a fresh voice call in this same original dot conversation. Confirm it is open below. Automatic call launch is unavailable."
        }
        if let recovery = recovery { return recovery.guidance }
        if phase.terminal && oneAttempt { return "Session ended. Microphone capture state is unknown until cleanup is verified. A fresh approved check is needed before another attempt." }
        switch phase {
        case .idle: return "Ready now. Open Companion on StackChan and leave physical Listen off. Click Pair StackChan to begin."
        case .opening: return "Opening USB before pairing. Wait for the robot to settle."
        case .usb_ready, .waiting_robot: return "Keep USB connected. On the robot, tap Pair."
        case .comparing, .waiting_confirmation:
            if audioApproved { return confirmationSent ? "Confirm the same number on StackChan only if both numbers match. Its Pair & authorize mic/speaker button gives physical consent for the temporary microphone and speaker session." : "Compare this number with the correct StackChan screen. Confirm on both only if they match. The robot’s Pair & authorize mic/speaker button gives physical consent for the temporary microphone and speaker session." }
            return confirmationSent ? "Confirm the same number on StackChan. Only accept if both numbers match." : "Compare this number with the correct StackChan screen. Confirm on both only if they match."
        case .staging: return "Both confirmations received. Waiting for StackChan’s acknowledgement."
        case .restoring: return "Closing temporary access. Keep physical Listen off while cleanup is verified."
        case .paired:
            if activity == .conversation {
                return "The worker reports conversation activity. Microphone capture state is unknown; physical playback and call destination are not verified here. Use Cancel or the physical stop to end."
            }
            return audioApproved ? "Pairing confirmed. Waiting for the robot’s physical microphone consent acknowledgement from its matching-number confirmation. Use Cancel on this Mac or Back on StackChan to stop." : "Pairing acknowledged. Leave physical Listen off. Audio needs separate approval."
        case .cancelled: return "Cancelled. Any temporary grant is revoked. Retry starts a fresh attempt."
        case .expired: return "The session expired. Retry starts a fresh attempt; no previous grant is reused."
        case .failed: return "Pairing stopped. Microphone capture state is unknown; verify worker cleanup and leave physical Listen off before a fresh retry."
        case .closed: return "Session closed. Temporary access ended."
        }
    }
    func primary() {
        guard canPrimary else { return }
        if canConfirm { confirmationSent = true; send("confirm"); return }
        let retry = phase.terminal && child != nil
        confirmationSent = false; code = nil; remaining = nil; audioRemaining = nil; activity = nil; recovery = nil; restorationConfirmed = false; attemptID = UUID()
        if child == nil { start() }
        if child != nil { phase = .opening; send(retry ? "retry" : "prepare") }
    }
    private func start() {
        if processFactory == nil && (!FileManager.default.fileExists(atPath: root.path) || ProcessInfo.processInfo.environment["STACKCHAN_BINDING_CONFIG"] == nil) {
            phase = .failed; recovery = .setup; rememberedStatus = "Approved local configuration unavailable."; return
        }
        let launchEnvironment = workerLaunchEnvironment
        let workerPython = processFactory == nil ? WorkerPython.resolve(environment: launchEnvironment, root: root) : nil
        if processFactory == nil && workerPython == nil {
            phase = .failed; recovery = .setup
            rememberedStatus = "Python unavailable. Configure STACKCHAN_PYTHON with the approved virtual environment's executable."
            return
        }
        let process = processFactory?() ?? Process(), incoming = Pipe(), outgoing = Pipe()
        // A worker may close stdin before terminationHandler reaches the main queue.
        // Suppress SIGPIPE only on our command descriptor so EPIPE reaches cleanup.
        guard fcntl(incoming.fileHandleForWriting.fileDescriptor, F_SETNOSIGPIPE, 1) == 0 else {
            try? incoming.fileHandleForReading.close(); try? incoming.fileHandleForWriting.close()
            try? outgoing.fileHandleForReading.close(); try? outgoing.fileHandleForWriting.close()
            phase = .failed; recovery = .setup; return
        }
        let activeGeneration = UUID(); generation = activeGeneration; buffer.removeAll(); requiresProcessExit = false
        lastWorkerExitConfirmed = nil
        if processFactory == nil {
        process.executableURL = workerPython
        process.arguments = persistentMode ? rememberedWorkerLaunchArguments : workerLaunchArguments
        process.currentDirectoryURL = root
        process.environment = launchEnvironment
        }
        process.standardInput = incoming; process.standardOutput = outgoing
        // Raw exceptions/IO are never reflected into the UI or a log file.
        process.standardError = FileHandle.nullDevice
        // Pipe reads and the termination notice arrive on unrelated queues. A worker that prints a
        // final status (for example a cleanup report) and exits at once must not have that status
        // dropped by the termination block. PipeReader is the only reader of the pipe: it is
        // nonblocking for its whole life and is never switched back, so a handler that was already
        // queued cannot block or raise on an empty pipe that a descendant still holds open. Reads
        // and enqueues happen under its lock; termination retires it (draining what is left) before
        // enqueuing its block, so a queued handler that runs afterwards reads nothing.
        guard let reader = PipeReader(fd: outgoing.fileHandleForReading.fileDescriptor) else {
            try? incoming.fileHandleForReading.close(); try? incoming.fileHandleForWriting.close()
            try? outgoing.fileHandleForReading.close(); try? outgoing.fileHandleForWriting.close()
            phase = .failed; recovery = .setup; return
        }
        let gate = finalizationGate
        outgoing.fileHandleForReading.readabilityHandler = { [weak self] handle in
            if let gate = gate { _ = gate.wait(timeout: .now() + 5); gate.signal() }
            let outcome = reader.read { data in
                DispatchQueue.main.async {
                    guard self?.generation == activeGeneration else { return }
                    self?.receive(data)
                }
            }
            switch outcome {
            case .eof, .retired: handle.readabilityHandler = nil   // stop a source that would spin
            case .failed:
                // A hard read error means output may be lost: never let it pass as a quiet release.
                handle.readabilityHandler = nil
                DispatchQueue.main.async {
                    guard let self = self, self.generation == activeGeneration else { return }
                    self.failProtocol()
                }
            case .data, .wouldBlock, .interrupted: break
            }
        }
        process.terminationHandler = { [weak self] process in
            if let gate = gate { _ = gate.wait(timeout: .now() + 5); gate.signal() }
            reader.retire { retirement in
                let remainder = retirement.data
                DispatchQueue.main.async {
                    guard let self = self, self.generation == activeGeneration, self.child === process else { return }
                    self.webcam.ownerLost(); self.localVision.disable(); self.gestures.ownerLost()
                    if !remainder.isEmpty { self.receive(remainder) }
                    guard self.generation == activeGeneration else { return }
                    // Exit without a complete final JSON line is not evidence of cleanup.
                    let incompleteStatus = !self.buffer.isEmpty
                    self.generation = UUID()
                    self.buffer.removeAll()
                    self.output?.readabilityHandler = nil; self.output = nil; self.input = nil; self.child = nil
                    // A read error or an incomplete final drain means output may be missing: not a release.
                    let released = Self.releaseVerdict(exitedCleanly: process.terminationReason == .exit && process.terminationStatus == 0,
                                                       cleanupIssuesEmpty: self.cleanupIssues.isEmpty, readFaulted: retirement.faulted || incompleteStatus)
                    if (retirement.faulted || incompleteStatus) && self.recovery == nil { self.recovery = .protocolFailure }
                    self.lastWorkerExitConfirmed = released
                    if self.phase == .cancelled { self.updateRememberedReleaseStatus(released) }
                    if self.phase.busy { self.phase = .failed; self.code = nil; self.recovery = self.recovery ?? .workerExit }
                    self.cleanupPending = !released; self.requiresProcessExit = false
                    self.refreshRestoration()
                    self.finishShutdown(released)
                }
            }
        }
        do { try process.run(); child = process; input = incoming.fileHandleForWriting; output = outgoing.fileHandleForReading }
        catch { outgoing.fileHandleForReading.readabilityHandler = nil; phase = .failed; recovery = .setup }
    }
    private func receive(_ data: Data) {
        guard !data.isEmpty else { return }
        buffer.append(data)
        guard buffer.count <= 8192 else { failProtocol(); return }
        while let end = buffer.firstIndex(of: 10) {
            let line = buffer.subdata(in: 0..<end); buffer.removeSubrange(0...end)
            if let notice = CameraPermissionNotice.decode(line) {
                cameraPermissionFeedback = notice.feedback
                continue // Feedback only; never enables capture or trusts a status bool.
            }
            if webcamFramePipeline.handleExistingWorkerEvent(line) {
                if webcamFramePipeline.binding == nil { webcam.ownerLost() }
                continue
            }
            guard let status = PairingStatus.decode(line) else { failProtocol(); return }
            if (requiresProcessExit && !status.phase.terminal && status.phase != .restoring) || (cleanupPending && !status.phase.terminal && status.phase != .restoring) { continue }
            phase = status.phase; if phase.terminal || phase == .restoring { webcam.ownerLost(); localVision.disable(); gestures.ownerLost() }; code = status.code; remaining = status.remaining
            if persistentMode {
                if phase == .paired {
                    rememberedStatus = status.activity == nil
                        ? "Pairing stored. Select the StackChan audio devices in your Voice app, then choose Use StackChan."
                        : "Audio listener ready. " + MiraPresentation.workerAudioStatus(status.activity)
                }
                if phase == .cancelled {
                    rememberedStatus = "Use StackChan stop was requested. Worker release is not confirmed; microphone capture state is unknown. Wireless trust remains saved unless you chose Forget."
                }
                if phase == .failed { rememberedStatus = "Operation failed. Microphone capture state is unknown; verify cleanup before retrying." }
                if phase == .closed { rememberedStatus = "Listener closed. Remembered trust is still saved." }
            }
            activity = status.activity; audioRemaining = status.audioRemaining
            if let cause = status.recovery, recovery == nil { recovery = cause }
            if phase == .restoring { cleanupPending = true }
            if !status.cleanupIssues.isEmpty { cleanupIssues = status.cleanupIssues }
            if persistentMode && forgetAwaitingAcknowledgement && status.rememberedForgetVerified {
                cleanupIssues.remove(.grant)
                forgetAwaitingAcknowledgement = false
                rememberedStatus = "Mac trust revocation is confirmed. Also choose Forget on StackChan."
            }
            receivedUptime = ProcessInfo.processInfo.systemUptime
            if persistentMode && requiresProcessExit && status.rememberedCleanupVerified {
                cleanupIssues.remove(.audio);cleanupIssues.remove(.grant)
            }
            if phase.terminal { confirmationSent = false; cleanupPending = !cleanupIssues.isEmpty }
        }
    }
    /// Release is confirmed only by an ordered finalization: clean exit, no reported cleanup issue, and
    /// every byte of final output read without error. The deadline timer never produces this verdict.
    static func releaseVerdict(exitedCleanly: Bool, cleanupIssuesEmpty: Bool, readFaulted: Bool) -> Bool {
        exitedCleanly && cleanupIssuesEmpty && !readFaulted
    }
    /// Malformed-protocol contract (existing behavior, preserved and tested): a status the controller cannot
    /// decode marks the attempt failed (`recovery == .protocolFailure`), sets cleanup pending and asks the
    /// worker to stop. If the worker then exits 0 having reported no cleanup issue and every byte was read
    /// cleanly, that exit is accepted as a confirmed release. Only a worker-reported cleanup issue, a non-clean
    /// exit, a read error or an incomplete drain keeps the release unconfirmed.
    private func failProtocol() {
        webcam.ownerLost(); localVision.disable(); gestures.ownerLost()
        phase = .failed; code = nil; recovery = .protocolFailure; cleanupPending = true
        shutdown { [weak self] closed in self?.cleanupPending = !closed }
    }
    private func updateRememberedReleaseStatus(_ released: Bool) {
        guard persistentMode else { return }
        rememberedStatus = released
            ? "Use StackChan stopped. Worker process release is confirmed; wireless trust remains saved unless you chose Forget."
            : "Use StackChan stop is not confirmed. Microphone capture state is unknown; verify cleanup before retrying. Wireless trust remains saved unless you chose Forget."
    }
    func explicitCameraPermission(_ mode: CameraPermissionMode) {
        guard persistentMode, phase == .paired, activity != nil, !cleanupPending, input != nil,
              let proposal = try? CameraPermissionProposal(requestID: UUID().uuidString, mode: mode, ttlSeconds: 30),
              let data = try? proposal.localWorkerJSON(),
              let payload = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any] else { return }
        send(payload)
    }
    func requestCameraPermission(_ request: CameraPermissionRequest) {
        guard persistentMode, phase == .paired, !cleanupPending, input != nil else {
            cameraPermissionFeedback = "Camera permission unavailable · current remembered session required"; return
        }
        guard let data = try? request.localWorkerJSON(),
              let payload = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any] else { return }
        send(payload) // Existing Mac local worker pipe, not a new network route.
    }
    func revokeCameraPermission(requestID: String) {
        webcam.ownerLost(); localVision.disable(); gestures.ownerLost()
        guard let data = try? CameraPermissionRequest.explicitOff(requestID: requestID),
              let payload = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any] else { return }
        send(payload) // Actual Stop remains retained/checked by existing worker owner.
    }
    private func send(_ command: String) {
        send(["command": command])
    }
    private func send(_ payload: [String: Any]) {
        guard let input = input,
              let data = try? JSONSerialization.data(withJSONObject: payload) else { return }
        do { try input.write(contentsOf: data + Data([10])) }
        catch { phase = .failed; code = nil; recovery = recovery ?? .connection; cleanupPending = true; requiresProcessExit = true
            try? self.input?.close(); self.input = nil
            stopAfterRequest { [weak self] closed in self?.cleanupPending = !closed }
        }
    }
    func secondsRemaining(at uptime: TimeInterval = ProcessInfo.processInfo.systemUptime) -> Int? {
        guard let remaining = activity == .conversation ? audioRemaining : remaining else { return nil }
        return max(0, remaining - Int(max(0, uptime - receivedUptime)))
    }
    func cancel() { webcam.ownerLost(); localVision.disable(); gestures.ownerLost(); guard canCancel else { return }; cleanupPending = true; code = nil; send("cancel") }
    func shutdown(_ done: @escaping (Bool) -> Void) {
        webcam.ownerLost(); localVision.disable(); gestures.ownerLost()
        guard child != nil else { done((lastWorkerExitConfirmed ?? true) && cleanupIssues.isEmpty); return }
        // A request while a stop is already in flight joins it: no second cancel, no second timer set.
        let alreadyRequested = escalationGeneration == generation
        cleanupPending = true; requiresProcessExit = true
        if !alreadyRequested { send("cancel"); try? input?.close(); input = nil }
        stopAfterRequest(done)
    }
    private func finishShutdown(_ released: Bool) {
        let completions = shutdownCompletions; shutdownCompletions = []
        for completion in completions { completion(released) }
    }
    private func stopAfterRequest(_ done: ((Bool) -> Void)?) {
        guard let process = child else { done?(true); return }
        if let done = done { shutdownCompletions.append(done) }
        let activeGeneration = generation
        guard escalationGeneration != activeGeneration else { return }   // escalation is scheduled once per worker
        escalationGeneration = activeGeneration; escalationSchedules += 1
        DispatchQueue.main.asyncAfter(deadline: .now() + escalationDelays.terminate) { [weak self, weak process] in
            guard let self = self, self.generation == activeGeneration,
                  let process = process, self.child === process, process.isRunning else { return }
            process.terminate()
        }
        DispatchQueue.main.asyncAfter(deadline: .now() + escalationDelays.kill) { [weak self, weak process] in
            guard let self = self, self.generation == activeGeneration,
                  let process = process, self.child === process, process.isRunning else { return }
            // A remembered worker retains exact owners and retries checked close.
            // Never kill that custody process merely because cleanup is slow.
            guard !self.persistentMode else { return }
            Darwin.kill(process.processIdentifier, SIGKILL)
        }
        DispatchQueue.main.asyncAfter(deadline: .now() + escalationDelays.deadline) { [weak self] in
            guard let self = self, self.generation == activeGeneration else { return }
            // The termination block is the only place that confirms a release, and it advances `generation`.
            // Reaching here with the same generation means exit and final output have not been finalized in
            // order, whatever `Process.isRunning` says now, so the release stays unconfirmed. The late
            // termination block still publishes the real result.
            self.cleanupPending = true
            if self.phase == .cancelled { self.updateRememberedReleaseStatus(false) }
            // The escalation cycle is spent; a later request starts a fresh one instead of waiting forever.
            self.escalationGeneration = nil
            self.finishShutdown(false)
        }
    }
}

/// The single read path for a worker's stdout pipe. The descriptor is made nonblocking once and is
/// never switched back, because a readability handler that was already queued may still run after
/// the worker exits. Every read is bounded, EINTR is retried a few times, and EAGAIN, EOF and hard
/// errors are reported as outcomes instead of raised. `retired` is set under the same lock as the
/// reads, so no read starts after retirement.
final class PipeReader {
    enum Outcome: Equatable { case data, wouldBlock, interrupted, eof, retired, failed(Int32) }
    /// What retirement hands back: the final bytes, and whether anything may be missing (an earlier hard read
    /// error, or a final drain that ended on an error, an interrupt or the byte bound instead of EOF/empty).
    struct Retirement { let data: Data; let faulted: Bool }
    private let fd: Int32
    private let limit: Int
    private let lock = NSLock()
    private var isRetired = false
    private var fault = false       // sticky: output may have been lost
    init?(fd: Int32, limit: Int = 16384) {
        let flags = fcntl(fd, F_GETFL)
        guard flags >= 0, fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0 else { return nil }
        self.fd = fd; self.limit = limit
    }
    /// Reads what is available (at most `limit` bytes) and hands it to `deliver` while still holding the lock.
    func read(deliver: (Data) -> Void) -> Outcome {
        lock.lock(); defer { lock.unlock() }
        guard !isRetired else { return .retired }
        let (data, outcome) = readLocked()
        if case .failed = outcome { fault = true }
        if !data.isEmpty { deliver(data) }
        return outcome
    }
    /// Reads the remainder once, marks the reader retired, and runs `body` with the result under the lock.
    func retire<T>(_ body: (Retirement) -> T) -> T {
        lock.lock(); defer { lock.unlock() }
        var data = Data()
        if !isRetired {
            let (remainder, outcome) = readLocked()
            data = remainder
            switch outcome {
            case .eof, .wouldBlock: break                 // drained: nothing more is available now
            default: fault = true                         // error, interrupt or byte bound: incomplete
            }
            isRetired = true
        }
        return body(Retirement(data: data, faulted: fault))
    }
    private func readLocked() -> (Data, Outcome) {
        var collected = Data(), chunk = [UInt8](repeating: 0, count: 4096), interrupts = 0
        while collected.count < limit {
            let count = Darwin.read(fd, &chunk, min(chunk.count, limit - collected.count))
            if count > 0 { collected.append(contentsOf: chunk[0..<count]); continue }
            if count == 0 { return (collected, .eof) }
            let error = errno
            if error == EINTR { interrupts += 1; if interrupts <= 16 { continue }; return (collected, .interrupted) }
            if error == EAGAIN { return (collected, .wouldBlock) }
            return (collected, .failed(error))
        }
        return (collected, .data)     // bound reached; the source fires again if more remains
    }
}
