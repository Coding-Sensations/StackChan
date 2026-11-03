import Foundation
import SQLite3

// W1 checks. Inert: temporary SQLite files with synthetic values only, fake schedulers and
// clocks, fake link/route objects. No audio, no sockets, no processes, no real registry,
// no credentials.

final class FixtureResources: OwnedAudioResources {
    var log: [String] = []
    var fail = false
    func shutdownAndConfirmRelease(_ done: @escaping (Bool) -> Void) { log.append("worker"); done(!fail) }
}
final class FixtureLink: RobotLinkOpening {
    var reporter: ((UInt64, RobotLinkEvent) -> Void)?
    var opens: [UInt64] = []
    func openRobotLink(epoch: UInt64, report: @escaping (UInt64, RobotLinkEvent) -> Void) { opens.append(epoch); reporter = report }
}
final class FixtureVerifiedRoute: SameCallRoute {
    var reporter: ((UInt64, SameCallEvent) -> Void)?
    var hangups: [UInt64] = []
    let cap: SameCallCapability
    init(hangup: Bool) { cap = .supported(routeID: "fixture", hangup: hangup, returnAudio: false) }
    var capability: SameCallCapability { cap }
    func connect(epoch: UInt64, report: @escaping (UInt64, SameCallEvent) -> Void) { reporter = report }
    func hangUp(epoch: UInt64, completion: @escaping (Bool) -> Void) { hangups.append(epoch); completion(true) }
}
final class FixtureScheduler: ReconnectScheduling {
    var pending: [(delay: TimeInterval, fire: () -> Void)] = []
    func schedule(after delay: TimeInterval, _ fire: @escaping () -> Void) { pending.append((delay, fire)) }
    /// Fire the oldest pending timer.
    func fireFirst() { guard !pending.isEmpty else { return }; pending.removeFirst().fire() }
    func fireAll() { let all = pending; pending = []; all.forEach { $0.fire() } }
}
final class Clock { var t: TimeInterval = 1000; var now: () -> TimeInterval { { self.t } } }
struct FixtureTrust: LaunchTrustProviding { let value: LaunchTrust; func current() -> LaunchTrust { value } }
final class MutableTrust: LaunchTrustProviding { var value: LaunchTrust = .absent; func current() -> LaunchTrust { value } }
struct FixtureReader: PersistentRegistryReading {
    let result: Result<RegistrySnapshot, Error>
    func snapshot() throws -> RegistrySnapshot { try result.get() }
}
struct Boom: Error {}

func check(_ ok: Bool, _ what: String, line: Int = #line) {
    if !ok { FileHandle.standardError.write(Data("FAIL line \(line): \(what)\n".utf8)); exit(1) }
}

struct Rig {
    let life: CompanionLifecycle, link: FixtureLink, sched: FixtureScheduler, clock: Clock, res: FixtureResources
    let manual: ManualMacCallRoute?
}
func rig(manual: Bool = true, policy: ReconnectPolicy = .standard, trust: LaunchTrust = .valid) -> Rig {
    let link = FixtureLink(), sched = FixtureScheduler(), clock = Clock(), res = FixtureResources()
    let route: ManualMacCallRoute? = manual ? ManualMacCallRoute() : nil
    let life = CompanionLifecycle(trust: FixtureTrust(value: trust), route: route ?? UnsupportedSameCallRoute(), link: link,
                                  scheduler: sched, policy: policy, now: clock.now, resources: res)
    return Rig(life: life, link: link, sched: sched, clock: clock, res: res, manual: route)
}
/// Opens, brings the robot link up, records robot consent, asserts the call and unmutes.
func armedRig(policy: ReconnectPolicy = .standard) -> Rig {
    let r = rig(policy: policy)
    r.life.open(); r.link.reporter?(r.life.epoch, .connected)
    r.life.robotConsentReported(epoch: r.life.epoch); r.life.attestCall(running: true); r.life.setMuted(false)
    return r
}

func sqlite(_ db: OpaquePointer?, _ sql: String) {
    var err: UnsafeMutablePointer<CChar>?
    check(sqlite3_exec(db, sql, nil, nil, &err) == SQLITE_OK, "fixture sql failed: \(sql)")
}

@main struct W1LifecycleChecks {
    static func main() {
        workerPython(); trustMapping(); sqliteReader(); configuredReader(); coldLaunchRoute(); presenceAndManualRoute(); closeReasons(); reconnect(); micStrip(); projection(); consentRevocation(); stopAudioSession(); webcamPipeOwnership()
        print("PASS W1 lifecycle: registry trust, robot-vs-call presence, bounded reconnect, microphone strip, confirmed audio-worker stop; inert fixtures only")
    }

    static func webcamPipeOwnership() {
        // Real pipeline/stop-consumer code with explicitly synthetic owner and
        // attestation fixtures. No native bridge, camera, audio or device IO.
        final class Owner: AuthenticatedWebcamOwner {
            var current = WebcamPurposeBinding(device:"fixture",conversation:"conversation",turn:"old",generation:1,epoch:1)
            var stops = 0
            var transportClockAndStopQualified: Bool { true }
            func issueFromExplicitUIIntent() throws -> WebcamPurposeBinding { current }
            func isCurrent(_ b: WebcamPurposeBinding) -> Bool { b == current }
            func mapProducerCapture(_ ns: UInt64, binding: WebcamPurposeBinding) throws -> WebcamMappedCapture {
                WebcamMappedCapture(hostTime:0,uncertainty:0)
            }
            func requestCheckedStop(_ b: WebcamPurposeBinding) -> WebcamStopReceipt? { stops += 1; return nil }
        }
        for action in ["read", "eof", "ownerLost"] {
            let owner=Owner(), session=NSObject()
            let evidence=CameraStopEvidenceConsumer(current:{_ in true},driverOrigin:{_,_ in true},
                physicalQualification:{ challenge,report in
                    PhysicalCameraStopAttestation(challenge:challenge,reportSerial:report.reportSerial,
                        qualificationReference:"synthetic.fixture.only",sensorCeased:true,dmaQuiescent:true,
                        noNewFrames:true,startBarrierHeld:true)
                },noNewerStream:{_ in true},clock:{0})
            var sinkStops=0, pending=0
            let pipeline=CompanionWebcamPipeline(owner:owner,extensionApproved:true,clock:{0},
                sinkRevoke:{sinkStops += 1},stopEvidence:evidence)
            check(pipeline.explicitWebcamOn(), "synthetic old binding opens")
            let old=owner.current
            let adapter=WebcamOwnedPipeAdapter(pipeline:pipeline,current:{owner.isCurrent(old)},cleanupPending:{pending += 1})
            check(!pipeline.revoke(), "old purpose retains pending stop")
            let context=CameraStopContext(session:session,binding:old,boot:"fixture",authorityID:1,scopeExpires:10)
            let challenge=evidence.begin(context)!
            let report=CameraDriverStopReport(binding:old,boot:"fixture",authorityID:1,challenge:challenge.nonce,
                stopSequence:challenge.sequence,reportSerial:1,outcome:"DriverClosed",required:15,completed:15,
                revoked:true,joined:true,wiped:true,custodyReleased:true,closeUncertain:false,everStarted:true)
            let proof=evidence.observe(session:session,report:report)!
            check(pipeline.confirmCheckedStop(WebcamStopReceipt(binding:old,hardwareClosed:true,qualifiedPhysical:proof)),
                "only synthetic qualified receipt releases fixture custody")
            owner.current=WebcamPurposeBinding(device:"fixture",conversation:"conversation",turn:"replacement",generation:2,epoch:1)
            check(pipeline.explicitWebcamOn(), "replacement binding opens")
            if action == "read" { check(!adapter.acceptOwnedRead(Data("stale\n".utf8)), "late read rejected") }
            else if action == "eof" { check(!adapter.ownedEOF(), "late EOF rejected") }
            else { adapter.ownerLost() }
            check(pipeline.binding == owner.current && !pipeline.stopPending, "replacement binding survives stale callback")
            check(owner.stops == 1 && sinkStops == 1 && pending == 0, "stale adapter cannot stop replacement sink or owner")
        }
    }

    static func workerPython() {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent("worker python " + UUID().uuidString)
        try! FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: root) }
        let controller = PairingController(approved: false, root: root)
        check(controller.workerLaunchEnvironment["STACKCHAN_ROOT"] == root.path, "worker receives the resolved controller root, including bundle-selected overrides")
        func executable(_ path: String) -> URL {
            let url = root.appendingPathComponent(path)
            try! FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
            try! Data("inert fixture; never executed".utf8).write(to: url)
            try! FileManager.default.setAttributes([.posixPermissions: 0o700], ofItemAtPath: url.path)
            return url
        }
        let configured = executable("configured venv/bin/python")
        let pathPython = executable("intel prefix/bin/python3")
        let env = ["STACKCHAN_PYTHON": configured.path, "PATH": pathPython.deletingLastPathComponent().path]
        check(WorkerPython.resolve(environment: env, root: root) == configured, "explicit executable with spaces takes precedence")
        check(WorkerPython.resolve(environment: ["PATH": pathPython.deletingLastPathComponent().path], root: root) == pathPython, "PATH Python works without Apple Silicon prefix")
        let venv = executable(".venv/bin/python3")
        check(WorkerPython.resolve(environment: ["PATH": pathPython.deletingLastPathComponent().path], root: root) == venv, "checkout venv takes precedence over PATH")
        for invalid in ["", "relative/python", root.appendingPathComponent("missing").path, root.path] {
            check(WorkerPython.resolve(environment: ["STACKCHAN_PYTHON": invalid, "PATH": pathPython.deletingLastPathComponent().path], root: root) == nil, "invalid explicit Python fails closed instead of choosing another")
        }
        try! FileManager.default.setAttributes([.posixPermissions: 0o600], ofItemAtPath: configured.path)
        check(WorkerPython.resolve(environment: ["STACKCHAN_PYTHON": configured.path], root: root) == nil, "nonexecutable configured file rejects")
        let emptyRoot = root.appendingPathComponent("empty")
        _ = executable("empty/bin/python3")
        _ = executable("empty/python3")
        let previousDirectory = FileManager.default.currentDirectoryPath
        check(FileManager.default.changeCurrentDirectoryPath(emptyRoot.path), "enter inert fixture cwd")
        defer { _ = FileManager.default.changeCurrentDirectoryPath(previousDirectory) }
        check(WorkerPython.resolve(environment: ["PATH": ":.:bin"], root: emptyRoot) == nil, "relative and empty PATH entries never select cwd executables")
        check(WorkerPython.resolve(environment: [:], root: emptyRoot) == nil, "missing interpreter is unavailable, never an author-specific fallback")
    }

    // MARK: (1) LaunchTrustProviding backed by the registry view
    static func trustMapping() {
        func trust(_ s: RegistrySnapshot, at now: Int = 100) -> LaunchTrust {
            PersistentRegistryTrust(reader: FixtureReader(result: .success(s)), clock: { now }).current()
        }
        func snap(_ state: PairRowState, expiry: Int? = 200, active: Int = 1, revoked: Int = 0) -> RegistrySnapshot {
            RegistrySnapshot(pairState: state, expiryUnix: expiry, activeCredentialCount: active, revokedCredentialCount: revoked)
        }
        check(trust(.empty) == .absent, "never enrolled -> absent")
        check(trust(snap(.pending, active: 1)) == .absent, "pending is not committed trust")
        check(trust(snap(.revoked, active: 0, revoked: 1)) == .absent, "forgotten -> absent")
        check(trust(snap(.active)) == .valid, "committed, unexpired, credential present -> valid")
        check(trust(snap(.active), at: 200) == .stale, "expiry instant is expired")
        check(trust(snap(.active), at: 999) == .stale, "expired -> stale")
        check(trust(snap(.active, expiry: nil)) == .stale, "missing expiry -> stale")
        check(trust(snap(.active, active: 0, revoked: 1)) == .stale, "active gate row without a live credential -> stale")
        check(trust(snap(.unrecognized)) == .unknown, "unrecognized state is unknown, not absent")
        check(PersistentRegistryTrust(reader: FixtureReader(result: .failure(Boom()))).current() == .unknown, "read failure is unknown")

        let live = PersistentRegistryTrust(reader: FixtureReader(result: .success(snap(.active))), clock: { 100 })
        let ready = CompanionLifecycle(trust: live, route: ManualMacCallRoute(), resources: FixtureResources())
        check(ready.presence == .readyToConnect && ready.canOpen, "valid trust + manual route is ready, not 'Pairing needed'")
        let unreadable = CompanionLifecycle(trust: PersistentRegistryTrust(reader: FixtureReader(result: .failure(Boom()))), route: ManualMacCallRoute(), resources: FixtureResources())
        check(unreadable.presence == .trustUnknown && !unreadable.canOpen, "unreadable registry is neither paired nor unpaired")
        check(!MiraPresentation.status(unreadable.presence).contains("Pairing needed") && !MiraPresentation.status(unreadable.presence).contains("Ready"), "unknown trust wording")
        let blocked = CompanionLifecycle(trust: live, resources: FixtureResources())
        if case .blocked = blocked.presence {} else { check(false, "valid trust + unsupported route stays blocked") }

        // refreshTrust: picks up a new pairing; never disturbs a live or closed session.
        let mt = MutableTrust(); mt.value = .absent
        let life = CompanionLifecycle(trust: mt, route: ManualMacCallRoute(), resources: FixtureResources())
        check(life.presence == .needsPairing, "starts unpaired")
        mt.value = .valid; life.refreshTrust(); check(life.presence == .readyToConnect, "refresh picks up pairing")
        life.open(); mt.value = .absent; life.refreshTrust(); check(life.presence == .connecting, "refresh never touches a live session")
        life.close(); mt.value = .valid; life.refreshTrust(); check(life.presence == .closed, "refresh never revives a closed session")
    }

    static func configuredReader() {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent("stackchan-configured-" + UUID().uuidString).resolvingSymlinksInPath()
        let base = root.appendingPathComponent(".tools/persistent-companion")
        let state = base.appendingPathComponent("reviewed-alternate-state")
        try! FileManager.default.createDirectory(at: state, withIntermediateDirectories: true)
        try! FileManager.default.setAttributes([.posixPermissions: 0o700], ofItemAtPath: base.path)
        defer { try? FileManager.default.removeItem(at: root) }
        let binding = root.appendingPathComponent("binding.json"), profile = base.appendingPathComponent("profile.json")
        let device = "stackchan-001122334455", digest = String(repeating: "a", count: 64)
        func write(_ value: [String: Any], _ url: URL) { try! JSONSerialization.data(withJSONObject: value).write(to: url); try! FileManager.default.setAttributes([.posixPermissions: 0o600], ofItemAtPath: url.path) }
        write(["device": device, "ca_sha256": digest, "expiry": 4000000000], binding)
        let metadata: [String: Any] = ["exact_device_id": device, "ca_sha256": digest, "remember_until_unix": 4000000000, "state_directory": state.path]
        write(metadata, profile)
        var db: OpaquePointer?
        check(sqlite3_open(state.appendingPathComponent("home-lab.sqlite3").path, &db) == SQLITE_OK, "configured fixture db")
        sqlite(db, "CREATE TABLE stackchan_pair (device TEXT, state TEXT, expiry INTEGER)")
        sqlite(db, "CREATE TABLE device_credentials (device_id TEXT, revoked_at TEXT)")
        sqlite(db, "INSERT INTO stackchan_pair VALUES ('stackchan-001122334455','active',4000000000)")
        sqlite(db, "INSERT INTO device_credentials VALUES ('stackchan-001122334455',NULL)")
        sqlite(db, "INSERT INTO stackchan_pair VALUES ('stackchan-80456b561404','revoked',1)")
        sqlite3_close(db)
        let reader = ConfiguredRegistryReader(projectRoot: root, bindingURL: binding)
        check(PersistentRegistryTrust(reader: reader, clock: { 1700000000 }).current() == .valid, "nonhistorical device and alternate checked state drive Home")
        var reopened: OpaquePointer?
        check(sqlite3_open(state.appendingPathComponent("home-lab.sqlite3").path, &reopened) == SQLITE_OK, "reopen configured db")
        sqlite(reopened, "UPDATE stackchan_pair SET expiry=3999999999 WHERE device='stackchan-001122334455'")
        check(PersistentRegistryTrust(reader: reader).current() == .unknown, "active row must match approved profile expiry")
        sqlite(reopened, "UPDATE stackchan_pair SET expiry=4000000000 WHERE device='stackchan-001122334455'")
        sqlite3_close(reopened)
        write(["device": device, "ca_sha256": digest, "expiry": 4000000000.25], binding)
        check(PersistentRegistryTrust(reader: reader).current() == .unknown, "fractional binding expiry rejected")
        write(["device": device, "ca_sha256": digest, "expiry": 4000000000], binding)
        var wrong = metadata; wrong["exact_device_id"] = "stackchan-ffffffffffff"; write(wrong, profile)
        check(PersistentRegistryTrust(reader: reader).current() == .unknown, "mismatched profile device fails closed")
        wrong = metadata; wrong["state_directory"] = root.path; write(wrong, profile)
        check(PersistentRegistryTrust(reader: reader).current() == .unknown, "state outside profile base rejected")
        write(metadata, profile)
        try! FileManager.default.setAttributes([.posixPermissions: 0o644], ofItemAtPath: binding.path)
        check(PersistentRegistryTrust(reader: reader).current() == .unknown, "nonprivate binding rejected")
        write(["device": device, "ca_sha256": digest, "expiry": 4000000000], binding)
        try! Data(("{\"device\":\"" + device + "\",\"device\":\"" + device + "\",\"ca_sha256\":\"" + digest + "\",\"expiry\":4000000000}").utf8).write(to: binding)
        check(PersistentRegistryTrust(reader: reader).current() == .unknown, "duplicate binding keys rejected")
    }

    static func coldLaunchRoute() {
        var checked = 0
        let controller = PairingController(approved: true, root: URL(fileURLWithPath: "/tmp"), processFactory: { Process() }, audioRouteAvailability: { checked += 1; return false })
        let adapter = PairingControllerRobotLink(); adapter.bind(controller)
        adapter.openRobotLink(epoch: 1) { _, _ in }
        check(checked == 1 && !controller.audioRouteEndpointsAvailable, "cold Home connection refreshes direction gate without visiting pairing UI; unavailable still refused")
        var availableChecks = 0
        let available = PairingController(approved: true, root: URL(fileURLWithPath: "/tmp"), processFactory: {
            let process = Process(); process.executableURL = URL(fileURLWithPath: "/nonexistent/inert-stackchan-fixture"); return process
        }, audioRouteAvailability: { availableChecks += 1; return true })
        let readyAdapter = PairingControllerRobotLink(); readyAdapter.bind(available)
        readyAdapter.openRobotLink(epoch: 2) { _, _ in }
        check(availableChecks == 1 && available.audioRouteEndpointsAvailable && available.phase == .failed,
              "available cold route reaches the inert worker start without requiring pairing-page onAppear")

    }

    // MARK: (1b) the SQLite view: state/counts only, credential columns unreadable
    static func sqliteReader() {
        check(RegistryAuthorizer.allows(action: SQLITE_READ, table: "stackchan_pair", column: "state"), "state readable")
        check(RegistryAuthorizer.allows(action: SQLITE_READ, table: "stackchan_pair", column: "expiry"), "expiry readable")
        for col in ["ca", "uri", "context"] { check(!RegistryAuthorizer.allows(action: SQLITE_READ, table: "stackchan_pair", column: col), "\(col) denied") }
        check(!RegistryAuthorizer.allows(action: SQLITE_READ, table: "device_credentials", column: "credential_hash"), "credential hash denied")
        check(!RegistryAuthorizer.allows(action: SQLITE_READ, table: "devices", column: "data"), "device record denied")
        check(!RegistryAuthorizer.allows(action: SQLITE_READ, table: "pairing_codes", column: "code_hash"), "pairing codes denied")
        for action in [SQLITE_INSERT, SQLITE_UPDATE, SQLITE_DELETE, SQLITE_PRAGMA, SQLITE_ATTACH, SQLITE_CREATE_TABLE, SQLITE_DROP_TABLE, SQLITE_TRANSACTION] {
            check(!RegistryAuthorizer.allows(action: action, table: "stackchan_pair", column: "state"), "write/admin action \(action) denied")
        }

        let dir = FileManager.default.temporaryDirectory.appendingPathComponent("stackchan-w1-" + UUID().uuidString)
        try! FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: dir) }
        let missing = dir.appendingPathComponent("absent.sqlite3")
        check((try? SQLiteRegistryReader(databaseURL: missing, deviceID: "stackchan-80456b561404").snapshot()) == .empty && !FileManager.default.fileExists(atPath: missing.path), "missing db is empty and is not created")

        let url = dir.appendingPathComponent("home-lab.sqlite3")
        var w: OpaquePointer?
        check(sqlite3_open(url.path, &w) == SQLITE_OK, "fixture db")
        sqlite(w, "PRAGMA journal_mode = WAL")
        sqlite(w, "CREATE TABLE seed (x TEXT)")      // gives the WAL a header, as the real store has
        let reader = SQLiteRegistryReader(databaseURL: url, deviceID: "stackchan-80456b561404")
        // A bare database (nothing ever enrolled) has no tables: unpaired, and still not modified.
        let bare = dir.appendingPathComponent("bare.sqlite3"); var bw: OpaquePointer?
        check(sqlite3_open(bare.path, &bw) == SQLITE_OK, "bare fixture"); sqlite(bw, "CREATE TABLE unrelated (x TEXT)"); sqlite3_close(bw)
        let bareBefore = try! Data(contentsOf: bare)
        check((try? SQLiteRegistryReader(databaseURL: bare, deviceID: "stackchan-80456b561404").snapshot()) == RegistrySnapshot.empty, "database without registry tables is unpaired")
        check(try! Data(contentsOf: bare) == bareBefore, "reader never modifies the database")
        sqlite(w, "CREATE TABLE stackchan_pair (device TEXT PRIMARY KEY, ca TEXT NOT NULL, uri TEXT NOT NULL, expiry INTEGER NOT NULL, state TEXT NOT NULL, context TEXT)")
        sqlite(w, "CREATE TABLE device_credentials (device_id TEXT PRIMARY KEY, credential_hash TEXT NOT NULL UNIQUE, revoked_at TEXT)")
        sqlite(w, "CREATE TABLE devices (id TEXT PRIMARY KEY, data TEXT NOT NULL)")
        check((try? reader.snapshot()) == RegistrySnapshot.empty, "empty tables are unpaired")
        sqlite(w, "INSERT INTO stackchan_pair VALUES ('stackchan-80456b561404','SYNTHETIC-CA-DIGEST','wss://synthetic.invalid/x',4000000000,'active','SYNTHETIC-CONTEXT')")
        sqlite(w, "INSERT INTO device_credentials VALUES ('stackchan-80456b561404','SYNTHETIC-HASH-NOT-A-CREDENTIAL',NULL)")
        sqlite(w, "INSERT INTO devices VALUES ('stackchan-80456b561404','{\"synthetic\":true}')")
        // Writer connection stays open (as the Python host does); the WAL is not checkpointed.
        let snap = try? reader.snapshot()
        check(snap == RegistrySnapshot(pairState: .active, expiryUnix: 4000000000, activeCredentialCount: 1, revokedCredentialCount: 0), "active snapshot read through WAL: \(String(describing: snap))")
        for secret in ["SYNTHETIC-HASH", "SYNTHETIC-CA", "SYNTHETIC-CONTEXT", "synthetic.invalid"] {
            check(!String(describing: snap as Any).contains(secret), "snapshot never carries \(secret)")
        }
        check(PersistentRegistryTrust(reader: reader, clock: { 1_700_000_000 }).current() == .valid, "registry-backed trust is valid")
        check(PersistentRegistryTrust(reader: reader, clock: { 4_000_000_001 }).current() == .stale, "registry-backed trust expires")

        // Engine-level proof: the reader's own connection cannot read or change anything else.
        let h = try! SQLiteRegistryReader.openReadOnly(url)
        defer { sqlite3_close(h) }
        func prepares(_ sql: String) -> Bool { var s: OpaquePointer?; let ok = sqlite3_prepare_v2(h, sql, -1, &s, nil) == SQLITE_OK; sqlite3_finalize(s); return ok }
        check(prepares("SELECT state, expiry FROM stackchan_pair WHERE device='x'"), "allowed query prepares")
        check(prepares("SELECT COUNT(*) FROM device_credentials WHERE device_id='x' AND revoked_at IS NULL"), "count query prepares")
        check(!prepares("SELECT credential_hash FROM device_credentials"), "credential hash cannot be selected")
        check(!prepares("SELECT * FROM device_credentials"), "star over credentials denied")
        check(!prepares("SELECT context FROM stackchan_pair"), "enrollment context denied")
        check(!prepares("SELECT ca, uri FROM stackchan_pair"), "digest and endpoint denied")
        check(!prepares("SELECT data FROM devices"), "device record denied")
        check(!prepares("INSERT INTO stackchan_pair VALUES ('a','b','c',1,'active',NULL)"), "insert denied")
        check(!prepares("UPDATE stackchan_pair SET state='active'"), "update denied")
        check(!prepares("PRAGMA journal_mode"), "pragma denied")
        check(!prepares("ATTACH DATABASE ':memory:' AS x"), "attach denied")
        check(!prepares("SELECT load_extension('x')"), "other functions denied")

        // Revoked credentials are counted, never read.
        sqlite(w, "UPDATE device_credentials SET revoked_at='2026-10-01T00:00:00Z'")
        sqlite(w, "UPDATE stackchan_pair SET state='revoked'")
        check((try? reader.snapshot()) == RegistrySnapshot(pairState: .revoked, expiryUnix: 4000000000, activeCredentialCount: 0, revokedCredentialCount: 1), "revoked counts")
        check(PersistentRegistryTrust(reader: reader).current() == .absent, "forgotten pairing -> absent")
        sqlite(w, "UPDATE stackchan_pair SET state='surprise'")
        check(PersistentRegistryTrust(reader: reader).current() == .unknown, "unrecognized gate state -> unknown")
        sqlite3_close(w)
        let garbage = dir.appendingPathComponent("garbage.sqlite3")
        try! Data("not a database, only synthetic bytes".utf8).write(to: garbage)
        check(PersistentRegistryTrust(reader: SQLiteRegistryReader(databaseURL: garbage, deviceID: "stackchan-80456b561404")).current() == .unknown, "unreadable database -> unknown, not absent")
    }

    // MARK: (2) presence and (6) ManualMacCallRoute
    static func presenceAndManualRoute() {
        let route = ManualMacCallRoute()
        check(route.capability == .manualAttach(routeID: "manual-mac") && !route.capability.isSupported && !route.capability.canHangUp && !route.capability.returnsAudio && route.capability.allowsOpen, "manual capability")
        route.userAssertsCallRunning()      // before connect: nothing to report to
        var seen: [SameCallEvent] = []
        route.connect(epoch: 4) { e, ev in check(e == 4, "epoch carried"); seen.append(ev) }
        check(seen.isEmpty, "connect alone reports nothing: no call is claimed from readiness")
        route.userAssertsCallRunning(); route.userReportsCallEnded()
        check(seen == [.userAsserted, .ended], "only explicit user statements are reported, never .connected")
        var hung: Bool?; route.hangUp(epoch: 4) { hung = $0 }; check(hung == false, "manual route never claims a hangup")

        let r = rig()
        check(r.life.presence == .readyToConnect, "manual route is openable")
        r.life.open()
        check(r.life.presence == .connecting && r.link.opens == [1] && r.life.muted && !r.life.robotLinked, "open starts the robot link only")
        r.link.reporter?(99, .connected); check(r.life.presence == .connecting, "stale link epoch ignored")
        r.link.reporter?(1, .connected)
        check(r.life.presence == .robotConnected && r.life.robotLinked && r.life.call == .notAttached, "robot connected, call not attached")
        check(!r.life.presence.showsActiveCall, "robot link is not a call")
        let status = MiraPresentation.status(r.life.presence)
        check(status.contains("Robot connected") && status.contains("call not attached") && !status.contains("In call"), "status wording: \(status)")
        check(MiraPresentation.detail(r.life.presence, closeRecord: nil, lastFailure: nil).contains("cannot start, verify or hang it up"), "detail says what it cannot do")
        for presence: MiraPresence in [.robotConnected, .callAsserted, .connected] {
            let detail = MiraPresentation.detail(presence, workerActivity: .conversation, closeRecord: nil, lastFailure: nil)
            check(detail.localizedCaseInsensitiveContains("unknown") &&
                  !detail.localizedCaseInsensitiveContains("muted") &&
                  !detail.localizedCaseInsensitiveContains("microphone is not sent"),
                  "live audio copy cannot infer mute from lifecycle projection: \(detail)")
        }
        check(MiraPresentation.workerAudioStatus(.conversation).contains("worker reports conversation activity") &&
              MiraPresentation.workerAudioStatus(.conversation).contains("capture state is unknown"),
              "worker activity is distinguished from confirmed capture")
        // PCM/WSS readiness (bridge activity, consent) never attaches a call.
        r.life.robotConsentReported(epoch: 1); r.life.bridgeActivityReported(epoch: 1, captureFlowing: true, playbackSuppressing: false)
        check(r.life.presence == .robotConnected && r.life.call == .notAttached, "consent and frames do not attach a call")
        r.life.setMuted(false); check(r.life.muted && !r.life.captureMayArm, "cannot unmute before a call is asserted")
        // A manual route can never verify: a forged .connected is ignored.
        r.life.routeReported(1, .connected); check(r.life.presence == .robotConnected && r.life.call == .notAttached, "manual route cannot report connected")
        r.life.attestCall(running: true)
        check(r.life.presence == .callAsserted && r.life.call == .userAsserted && !r.life.presence.showsActiveCall, "user assertion is not 'In call'")
        let asserted = MiraPresentation.status(r.life.presence)
        check(asserted.contains("not verified") && !asserted.contains("In call"), "asserted wording: \(asserted)")
        r.life.setMuted(false); check(!r.life.muted && r.life.captureMayArm, "arming after consent + unmute + user's statement")
        r.life.setSpeaker(true); check(!r.life.speakerEnabled, "no speaker return on a manual route")

        // Exhaustive: only .connected may ever say "In call".
        let all: [MiraPresence] = [.needsPairing, .repairRequired, .trustUnknown, .blocked(missing: "m"), .readyToConnect, .connecting, .robotConnected, .callAsserted, .connected, .reconnecting(attempt: 1, of: 3), .closing, .closed, .cleanupIncomplete(["x"])]
        for p in all { check(MiraPresentation.status(p).contains("In call") == (p == .connected), "'In call' only when verified: \(p)") }
        // A verified route, when one exists, still needs the robot link too.
        let vr = FixtureVerifiedRoute(hangup: false), vlink = FixtureLink()
        let vl = CompanionLifecycle(trust: FixtureTrust(value: .valid), route: vr, link: vlink, resources: FixtureResources())
        vl.open(); vr.reporter?(1, .connected)
        check(vl.presence == .connecting && vl.call == .attached && !vl.presence.showsActiveCall, "verified call without a robot link is not 'In call'")
        vlink.reporter?(1, .connected); check(vl.presence == .connected && vl.presence.showsActiveCall, "robot link + verified call -> In call")
        vl.attestCall(running: true); check(vl.call == .attached, "user attestation ignored on a verified route")
        // First open that never connects returns to ready with the reason.
        let f = rig(); f.life.open(); f.link.reporter?(1, .lost(.wifiLost))
        check(f.life.presence == .readyToConnect && f.life.lastFailure == .wifiLost && f.res.log.isEmpty, "failed first open -> ready, nothing reconnects")
        f.life.open(); check(f.life.lastFailure == nil && f.life.epoch == 2, "next open is a fresh epoch")
    }

    // MARK: (3) close reasons
    static func closeReasons() {
        let codes = CloseReason.allCases.map { $0.rawValue }
        check(Set(codes).count == codes.count && codes.allSatisfy { $0.range(of: "^[a-z]+(_[a-z]+)*$", options: .regularExpression) != nil }, "close codes are unique fixed snake_case labels")
        check(CloseReason.userClosed.initiator == .user && CloseReason.robotClosed.initiator == .robot && CloseReason.robotReset.initiator == .robot && CloseReason.wifiLost.initiator == .network && CloseReason.callEnded.initiator == .call && CloseReason.appQuit.initiator == .app && CloseReason.sessionLimit.initiator == .worker, "initiator attribution")

        // Manual route, user said a call was running: it keeps running.
        let a = armedRig(); var done: Bool?
        a.life.close(reason: .userClosed) { done = $0 }
        check(done == true && a.life.presence == .closed && a.res.log == ["worker"], "closed after release")
        let rec = a.life.closeRecord!
        check(rec.reason == .userClosed && rec.initiator == .user && rec.released && rec.call == .keepsRunning && rec.epoch == 1, "close record")
        check(rec.message.contains("still running") && rec.message.contains("no way to hang it up") && rec.message.contains("StackChan is released"), "says the call keeps running: \(rec.message)")
        check(a.life.call == .notAttached && a.life.muted && !a.life.captureMayArm && !a.life.robotLinked, "disarmed after close")
        check(MiraPresentation.detail(a.life.presence, closeRecord: a.life.closeRecord, lastFailure: nil) == rec.message, "closed screen shows the record")
        // Not asserted: still honest that it may keep running.
        let b = rig(); b.life.open(); b.link.reporter?(1, .connected); b.life.close(reason: .appQuit)
        check(b.life.closeRecord?.call == .mayKeepRunning && b.life.closeRecord?.reason == .appQuit && b.life.closeRecord!.message.contains("keeps running"), "unknown call may keep running")
        // Never opened.
        let c = rig(); c.life.close(); check(c.life.closeRecord?.call == CallAfterClose.none && !c.life.closeRecord!.message.contains("call"), "no session, no call claim")
        // A verified route with hangup says so.
        let vr = FixtureVerifiedRoute(hangup: true), vlink = FixtureLink()
        let v = CompanionLifecycle(trust: FixtureTrust(value: .valid), route: vr, link: vlink, resources: FixtureResources())
        v.open(); vlink.reporter?(1, .connected); vr.reporter?(1, .connected); v.close()
        check(v.closeRecord?.call == .hungUp && vr.hangups == [1] && v.closeRecord!.message.contains("hung up"), "hangup path")
        // Failed release is reported, with the reason kept.
        let d = rig(); d.res.fail = true; d.life.open(); d.link.reporter?(1, .connected); d.life.close(reason: .appQuit)
        check(d.life.presence == .cleanupIncomplete(["workerRelease"]) && d.life.closeRecord?.released == false && d.life.closeRecord!.message.contains("release was not confirmed"), "unconfirmed release")

        // Session ends attributed by side.
        for (reason, label) in [(CloseReason.robotClosed, "robot"), (.protocolError, "worker"), (.sessionLimit, "worker"), (.consentTimeout, "worker")] {
            let r = armedRig(); r.link.reporter?(r.life.epoch, .lost(reason))
            check(r.life.presence == .closed && r.life.closeRecord?.reason == reason && r.life.closeRecord?.initiator.rawValue == label && r.sched.pending.isEmpty, "\(reason) ends the session without reconnecting")
            check(r.life.muted && !r.life.captureMayArm && r.res.log == ["worker"], "\(reason) disarms and releases")
        }
        let e = armedRig(); e.manual?.userReportsCallEnded()
        check(e.life.presence == .closed && e.life.closeRecord?.reason == .callEnded && e.life.closeRecord?.initiator == .call, "call ended -> call_ended")
        let f = armedRig(); f.life.routeReported(f.life.epoch, .failed)
        check(f.life.presence == .closed && f.life.closeRecord?.reason == .routeFailed, "route failure mid-session -> route_failed")
    }

    // MARK: (4) bounded, muted-only reconnect
    static func reconnect() {
        let r = armedRig()
        check(r.life.captureMayArm && !r.life.muted && r.life.epoch == 1, "armed before the drop")
        r.link.reporter?(1, .lost(.wifiLost))
        check(r.life.presence == .reconnecting(attempt: 1, of: 3), "reconnecting, attempt 1 of 3")
        check(r.life.muted && !r.life.physicalConsent && !r.life.captureMayArm && !r.life.robotLinked && r.life.epoch == 2, "disarmed at the instant of loss, old epoch already dead")
        check(r.sched.pending.count == 1 && r.sched.pending[0].delay == 2 && r.link.opens == [1], "first retry is delayed, nothing opened yet")
        check(r.res.log.isEmpty, "worker listener is kept for the retry; no release yet")
        check(MiraPresentation.status(r.life.presence).contains("fresh consent required") && !MiraPresentation.status(r.life.presence).contains("In call"), "reconnecting wording")
        r.link.reporter?(1, .connected); check(r.life.presence == .reconnecting(attempt: 1, of: 3), "a stale-epoch report cannot revive the old session")
        r.sched.fireFirst()
        check(r.life.epoch == 3 && r.link.opens == [1, 3] && r.life.muted && !r.life.physicalConsent, "fresh epoch, fresh attempt, still muted")
        check(r.sched.pending.count == 1 && r.sched.pending[0].delay == ReconnectPolicy.standard.attemptTimeout, "attempt timeout armed")
        r.link.reporter?(1, .connected); check(r.life.presence == .reconnecting(attempt: 1, of: 3), "epoch 1 callbacks are stale")
        r.link.reporter?(3, .connected)
        check(r.life.presence == .robotConnected && r.life.robotLinked, "back to robot connected, call NOT re-attached")
        check(r.life.call == .notAttached && r.life.muted && !r.life.physicalConsent && !r.life.captureMayArm, "reconnect never re-arms the mic")
        r.life.setMuted(false); check(r.life.muted, "unmute refused until robot consent and a fresh call statement")
        r.life.robotConsentReported(epoch: 1); check(!r.life.physicalConsent, "old consent cannot apply to the new epoch")
        r.sched.fireAll(); check(r.life.presence == .robotConnected, "attempt timeout after success is ignored")
        r.life.robotConsentReported(epoch: 3); r.life.attestCall(running: true); r.life.setMuted(false)
        check(r.life.captureMayArm, "re-arming needs fresh robot consent, a fresh call statement and an explicit unmute")
        check(r.life.reconnectsThisSession == 1, "counted")

        // Exhaustion: three failed attempts with growing delays, then a clean close.
        let x = armedRig()
        x.link.reporter?(1, .lost(.wifiLost))
        var delays = [x.sched.pending[0].delay]
        for n in 1...2 {
            x.sched.fireFirst(); x.sched.pending.removeAll()
            x.link.reporter?(x.life.epoch, .lost(.wifiLost))
            check(x.life.presence == .reconnecting(attempt: n + 1, of: 3), "attempt \(n + 1)")
            delays.append(x.sched.pending[0].delay)
        }
        check(delays == [2, 5, 10], "bounded backoff \(delays)")
        x.sched.fireFirst(); x.sched.pending.removeAll()
        x.link.reporter?(x.life.epoch, .lost(.wifiLost))
        check(x.life.presence == .closed && x.life.closeRecord?.reason == .reconnectExhausted && x.life.closeRecord?.initiator == .app && x.res.log == ["worker"], "gives up and releases")
        check(x.life.reconnectsThisSession == 3 && x.sched.pending.isEmpty, "exactly three attempts")
        // A late timer after the close does nothing.
        let epochAfter = x.life.epoch; x.sched.fireAll(); check(x.life.epoch == epochAfter && x.link.opens.count == 4, "no attempt after close")

        // Attempt that reports nothing times out into the next attempt.
        let t = armedRig(); t.link.reporter?(1, .lost(.wifiLost)); t.sched.fireFirst()
        check(t.life.epoch == 3, "attempt 1 started")
        t.sched.fireFirst()     // attempt timeout
        check(t.life.presence == .reconnecting(attempt: 2, of: 3) && t.life.muted, "silent attempt counts as failed")

        // Time window.
        let w = armedRig(); w.link.reporter?(1, .lost(.wifiLost)); w.sched.fireFirst(); w.sched.pending.removeAll()
        w.clock.t += ReconnectPolicy.standard.window + 1
        w.link.reporter?(w.life.epoch, .lost(.wifiLost))
        check(w.life.closeRecord?.reason == .reconnectExhausted, "window exceeded -> exhausted")

        // Robot reset: always a fresh attempt (immediate, counter restarts) but bounded per session.
        let policy = ReconnectPolicy(maxAttempts: 2, delays: [2, 5], window: 60, attemptTimeout: 20, maxPerSession: 3)
        let s = armedRig(policy: policy)
        s.link.reporter?(1, .lost(.wifiLost)); s.sched.fireFirst(); s.sched.pending.removeAll()
        s.link.reporter?(s.life.epoch, .lost(.wifiLost)); s.sched.fireFirst(); s.sched.pending.removeAll()
        check(s.life.reconnectsThisSession == 2, "two attempts used")
        s.link.reporter?(s.life.epoch, .lost(.robotReset))
        check(s.life.presence == .reconnecting(attempt: 1, of: 2) && s.sched.pending[0].delay == 0, "reset restarts the attempt count and retries at once")
        s.sched.fireFirst(); s.sched.pending.removeAll()
        check(s.life.muted && !s.life.physicalConsent && s.life.reconnectsThisSession == 3, "reset attempt is a fresh epoch, muted")
        s.link.reporter?(s.life.epoch, .lost(.robotReset))
        check(s.life.closeRecord?.reason == .reconnectExhausted, "per-session cap stops reset loops")

        // Close during a reconnect cancels it.
        let c = armedRig(); c.link.reporter?(1, .lost(.wifiLost))
        c.life.close(reason: .userClosed)
        let ep = c.life.epoch; c.sched.fireAll()
        check(c.life.presence == .closed && c.life.epoch == ep && c.link.opens == [1] && c.life.closeRecord?.reason == .userClosed && c.life.closeRecord?.call == .keepsRunning, "user close cancels the retry")
        // A non-eligible reason during a retry ends the session.
        let n = armedRig(); n.link.reporter?(1, .lost(.wifiLost)); n.sched.fireFirst(); n.sched.pending.removeAll()
        n.link.reporter?(n.life.epoch, .lost(.robotClosed))
        check(n.life.closeRecord?.reason == .robotClosed && n.life.presence == .closed, "robot-side stop during a retry is final")
        // Success resets the per-loss counter (but not the session total).
        let k = armedRig(); k.link.reporter?(1, .lost(.wifiLost)); k.sched.fireFirst(); k.sched.pending.removeAll(); k.link.reporter?(k.life.epoch, .connected)
        k.link.reporter?(k.life.epoch, .lost(.wifiLost))
        check(k.life.presence == .reconnecting(attempt: 1, of: 3) && k.life.reconnectsThisSession == 1, "new loss starts a new bounded sequence")
        // Never unmuted at any step above on the new epochs.
        check(k.life.muted && !k.life.speakerEnabled, "still muted")
    }

    // MARK: (5) microphone strip
    static func micStrip() {
        let idle = rig()
        check(idle.life.micStrip.robotText.localizedCaseInsensitiveContains("unknown"), "mic strip is explicit when worker capture telemetry is unavailable")
        check(idle.life.micStrip == MicrophoneStrip(robot: .unknown, bridge: .idle, call: .unknown), "idle strip")
        idle.life.open(); check(idle.life.micStrip.bridge == .idle && idle.life.micStrip.robot == .unknown, "connecting: bridge not open yet")
        idle.link.reporter?(1, .connected)
        check(idle.life.micStrip == MicrophoneStrip(robot: .unknown, bridge: .open, call: .unknown), "linked with no capture telemetry remains unknown")
        idle.life.robotConsentReported(epoch: 1); idle.life.attestCall(running: true)
        check(idle.life.micStrip.robot == .unknown, "consent alone does not prove capture state")
        idle.life.setMuted(false); check(idle.life.micStrip.robot == .unknown, "local arming projection does not prove capture")
        idle.life.bridgeActivityReported(epoch: 1, captureFlowing: true, playbackSuppressing: false); check(idle.life.micStrip.robot == .streaming, "streaming")
        idle.life.bridgeActivityReported(epoch: 1, captureFlowing: true, playbackSuppressing: true)
        check(idle.life.micStrip.robotText.localizedCaseInsensitiveContains("conflict") && !idle.life.micStrip.robotText.localizedCaseInsensitiveContains("paused"), "conflicting capture and suppression telemetry is surfaced without claiming a pause")
        idle.life.bridgeActivityReported(epoch: 7, captureFlowing: false, playbackSuppressing: false); check(idle.life.micStrip.robotText.localizedCaseInsensitiveContains("conflict"), "stale-epoch activity ignored")
        idle.life.bridgeActivityReported(epoch: 1, captureFlowing: true, playbackSuppressing: false)
        idle.life.setMuted(true); check(idle.life.micStrip.robot == .streaming && idle.life.captureFlowing, "local mute projection cannot hide worker-reported capture")
        check(idle.life.micStrip.call == .unknown && idle.life.micStrip.lines.count == 3 && idle.life.micStrip.callText.contains("unknown"), "call mic is always unknown")
        // Worker telemetry is shown even if it contradicts the local gate; the UI must not hide it.
        let n = rig(); n.life.open(); n.link.reporter?(1, .connected); n.life.bridgeActivityReported(epoch: 1, captureFlowing: true, playbackSuppressing: false)
        check(n.life.micStrip.robot == .streaming && !n.life.captureMayArm, "worker-reported flow is visible even before local arming projection")
        n.link.reporter?(1, .lost(.wifiLost))
        check(n.life.micStrip == MicrophoneStrip(robot: .unknown, bridge: .idle, call: .unknown), "reconnecting: capture is unknown, bridge idle")
        n.life.close()
        check(n.life.micStrip == MicrophoneStrip(robot: .unknown, bridge: .idle, call: .unknown), "closed strip")
    }

    // MARK: production link projection
    static func projection() {
        check(RobotLinkProjection.linked(phase: .paired, activity: .physicalConsent) && !RobotLinkProjection.consented(phase: .paired, activity: .physicalConsent), "listener ready, no consent")
        check(RobotLinkProjection.consented(phase: .paired, activity: .conversation), "consent only from the conversation activity")
        check(!RobotLinkProjection.linked(phase: .paired, activity: nil) && !RobotLinkProjection.linked(phase: .comparing, activity: .conversation), "enrollment stored / pairing is not a ready link")
        check(RobotLinkProjection.lossReason(phase: .failed, recovery: .network, localStop: false) == .wifiLost, "network -> wifi_lost")
        check(RobotLinkProjection.lossReason(phase: .failed, recovery: .connection, localStop: false) == .wifiLost, "connection -> wifi_lost")
        check(RobotLinkProjection.lossReason(phase: .failed, recovery: .reset, localStop: false) == .robotReset, "reset -> robot_reset")
        check(RobotLinkProjection.lossReason(phase: .failed, recovery: .audioBackpressure, localStop: false) == .protocolError, "worker fault -> protocol_error")
        check(RobotLinkProjection.lossReason(phase: .cancelled, recovery: nil, localStop: true) == .userClosed, "local stop -> user_closed")
        check(RobotLinkProjection.lossReason(phase: .cancelled, recovery: nil, localStop: false) == .robotClosed, "unexplained stop -> robot_closed")
        check(RobotLinkProjection.lossReason(phase: .expired, recovery: nil, localStop: false) == .sessionLimit, "expired -> session_limit")
    }

    // MARK: consent revocation while the robot listener remains paired
    static func consentRevocation() {
        let grant = RobotLinkProjection.consentTransition(
            phase: .paired, activity: .conversation, previouslyConsented: false
        )
        check(grant == true, "conversation activity grants consent once")
        let noDuplicateGrant = RobotLinkProjection.consentTransition(
            phase: .paired, activity: .conversation, previouslyConsented: true
        )
        check(noDuplicateGrant == nil, "unchanged consent is not reported again")
        let revoke = RobotLinkProjection.consentTransition(
            phase: .paired, activity: .physicalConsent, previouslyConsented: true
        )
        check(revoke == false, "return to physical-consent activity revokes the old consent")

        let r = armedRig()
        r.life.bridgeActivityReported(epoch: r.life.epoch, captureFlowing: true, playbackSuppressing: false)
        check(r.life.captureMayArm && r.life.captureFlowing, "precondition: consented capture is armed and flowing")
        r.life.robotConsentRevoked(epoch: r.life.epoch)
        check(r.life.robotLinked && r.life.presence == .callAsserted, "consent revocation leaves the paired call session present")
        check(!r.life.physicalConsent && r.life.muted && !r.life.captureMayArm && !r.life.captureFlowing,
              "revocation immediately disarms capture and requires explicit unmute")
        r.life.setMuted(false)
        check(r.life.muted, "unmute is refused until fresh physical consent")
        r.life.robotConsentRevoked(epoch: r.life.epoch - 1)
        check(r.life.muted && !r.life.physicalConsent, "stale-epoch revocation cannot change the current session")
        r.life.robotConsentReported(epoch: r.life.epoch)
        check(r.life.muted && r.life.physicalConsent && !r.life.captureMayArm,
              "fresh consent alone does not clear the forced mute")
        r.life.setMuted(false)
        check(r.life.captureMayArm, "fresh consent plus explicit unmute can arm again")
    }

    // MARK: stopping audio uses the owned worker's confirmed shutdown
    static func stopAudioSession() {
        let r = armedRig()
        check(r.life.captureMayArm, "precondition: current session can arm capture")
        r.life.stopAudioSession()
        check(r.life.presence == .closed && r.life.muted && !r.life.robotLinked,
              "stop ends the local session and disarms it")
        check(r.res.log == ["worker"] && r.life.closeRecord?.released == true,
              "stop uses the owned worker release path and records confirmation")

        let failed = rig(); failed.life.open(); failed.link.reporter?(failed.life.epoch, .connected)
        failed.life.robotConsentReported(epoch: failed.life.epoch); failed.life.attestCall(running: true)
        failed.life.setMuted(false); failed.res.fail = true
        failed.life.stopAudioSession()
        check(failed.life.presence == .cleanupIncomplete([CloseStep.workerRelease.rawValue]) &&
              failed.life.closeRecord?.released == false,
              "failed worker exit is never reported as a released audio session")
        failed.res.fail = false
        failed.life.stopAudioSession()
        check(failed.life.presence == .closed && failed.life.closeRecord?.released == true && failed.res.log == ["worker", "worker"],
              "retry after a failed release re-runs worker shutdown and confirms only on clean exit")
    }
}
