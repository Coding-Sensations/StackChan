import Foundation
import SQLite3
import Darwin

/// Opens the HomeLab SQLite file read-only and reads exactly two things: the gate row's
/// state and expiry, and counts of credential rows. An engine-level authorizer denies every
/// other table, column, pragma and write, so no credential hash, certificate digest, URI,
/// enrollment context or device record can be read even by mistake.
enum RegistryAuthorizer {
    static let pairTable = "stackchan_pair", credentialTable = "device_credentials"
    /// Pure policy so it can be tested without a database.
    static func allows(action: Int32, table: String?, column: String?) -> Bool {
        switch action {
        case SQLITE_SELECT, SQLITE_FUNCTION: return true
        case SQLITE_READ:
            let c = column ?? ""
            switch table ?? "" {
            case pairTable: return ["device", "state", "expiry"].contains(c)
            case credentialTable: return ["device_id", "revoked_at", ""].contains(c)
            default: return false
            }
        default: return false
        }
    }
}

private func registryAuthorizerCallback(_ context: UnsafeMutableRawPointer?, _ action: Int32,
                                        _ a: UnsafePointer<CChar>?, _ b: UnsafePointer<CChar>?,
                                        _ c: UnsafePointer<CChar>?, _ d: UnsafePointer<CChar>?) -> Int32 {
    // For SQLITE_READ: a = table, b = column. For SQLITE_FUNCTION: b = function name.
    if action == SQLITE_FUNCTION { return (b.map { String(cString: $0).lowercased() == "count" } ?? false) ? SQLITE_OK : SQLITE_DENY }
    let table = a.map { String(cString: $0) }, column = b.map { String(cString: $0) }
    return RegistryAuthorizer.allows(action: action, table: table, column: column) ? SQLITE_OK : SQLITE_DENY
}

struct SQLiteRegistryReader: PersistentRegistryReading {
    enum ReadError: Error { case open, query }
    let databaseURL: URL
    let deviceID: String
    init(databaseURL: URL, deviceID: String) { self.databaseURL = databaseURL; self.deviceID = deviceID }

    /// Read-only connection with the column allow-list installed. Internal so tests can prove
    /// that credential columns are unreadable through it.
    static func openReadOnly(_ url: URL) throws -> OpaquePointer {
        var db: OpaquePointer?
        guard sqlite3_open_v2(url.path, &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, nil) == SQLITE_OK, let handle = db else {
            sqlite3_close(db); throw ReadError.open
        }
        sqlite3_busy_timeout(handle, 250)
        guard sqlite3_set_authorizer(handle, registryAuthorizerCallback, nil) == SQLITE_OK else { sqlite3_close(handle); throw ReadError.open }
        return handle
    }

    func snapshot() throws -> RegistrySnapshot {
        // A missing database means nothing was ever enrolled; it is never created here.
        guard FileManager.default.fileExists(atPath: databaseURL.path) else { return .empty }
        let handle = try Self.openReadOnly(databaseURL)
        defer { sqlite3_close(handle) }

        var state: PairRowState = .unpaired
        var expiry: Int?
        if let row = try pairRow(handle) {
            expiry = row.expiry
            switch row.state {
            case "pending": state = .pending
            case "active": state = .active
            case "revoked": state = .revoked
            default: state = .unrecognized
            }
        }
        let active = try count(handle, "SELECT COUNT(*) FROM device_credentials WHERE device_id = ?1 AND revoked_at IS NULL")
        let revoked = try count(handle, "SELECT COUNT(*) FROM device_credentials WHERE device_id = ?1 AND revoked_at IS NOT NULL")
        return RegistrySnapshot(pairState: state, expiryUnix: expiry, activeCredentialCount: active, revokedCredentialCount: revoked)
    }

    /// "no such table" means the table was never created (nothing enrolled); anything else is a failure.
    private func prepare(_ db: OpaquePointer, _ sql: String) throws -> OpaquePointer? {
        var stmt: OpaquePointer?
        let rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nil)
        if rc == SQLITE_OK { return stmt }
        sqlite3_finalize(stmt)
        if String(cString: sqlite3_errmsg(db)).hasPrefix("no such table") { return nil }
        throw ReadError.query
    }
    private func pairRow(_ db: OpaquePointer) throws -> (state: String, expiry: Int)? {
        guard let stmt = try prepare(db, "SELECT state, expiry FROM stackchan_pair WHERE device = ?1") else { return nil }
        defer { sqlite3_finalize(stmt) }
        sqlite3_bind_text(stmt, 1, deviceID, -1, unsafeBitCast(-1, to: sqlite3_destructor_type.self))
        switch sqlite3_step(stmt) {
        case SQLITE_ROW: return (sqlite3_column_text(stmt, 0).map { String(cString: $0) } ?? "", Int(sqlite3_column_int64(stmt, 1)))
        case SQLITE_DONE: return nil
        default: throw ReadError.query
        }
    }
    private func count(_ db: OpaquePointer, _ sql: String) throws -> Int {
        guard let stmt = try prepare(db, sql) else { return 0 }
        defer { sqlite3_finalize(stmt) }
        sqlite3_bind_text(stmt, 1, deviceID, -1, unsafeBitCast(-1, to: sqlite3_destructor_type.self))
        guard sqlite3_step(stmt) == SQLITE_ROW else { throw ReadError.query }
        return Int(sqlite3_column_int64(stmt, 0))
    }
}

/// Selects only the registry identity/path from the same public binding and profile
/// used by remembered_pairing_worker. This is an availability projection, not TLS
/// verification: the worker still validates its complete profile before connecting.
struct ConfiguredRegistryReader: PersistentRegistryReading {
    enum BindingError: Error { case unavailable }
    let projectRoot: URL
    let bindingURL: URL?

    private func object(_ url: URL, ownerOnly: Bool) throws -> [String: Any] {
        let values = try url.resourceValues(forKeys: [.isSymbolicLinkKey, .isRegularFileKey])
        let attributes = try FileManager.default.attributesOfItem(atPath: url.path)
        guard values.isSymbolicLink != true, values.isRegularFile == true,
              let size = attributes[.size] as? NSNumber, size.intValue <= 8192 else { throw BindingError.unavailable }
        if ownerOnly {
            guard (attributes[.ownerAccountID] as? NSNumber)?.uint32Value == getuid(),
                  let mode = attributes[.posixPermissions] as? NSNumber, mode.intValue & 0o077 == 0 else { throw BindingError.unavailable }
        }
        let bytes = try Data(contentsOf: url)
        guard bytes.count <= 8192, let text = String(data: bytes, encoding: .utf8),
              let value = try JSONSerialization.jsonObject(with: bytes) as? [String: Any] else { throw BindingError.unavailable }
        // Both public files have flat keys. Reject duplicate/escaped duplicate keys,
        // matching the worker's strict JSON parser instead of accepting last-value wins.
        let expression = try NSRegularExpression(pattern: #""(?:[^"\\]|\\.)*"\s*:"#)
        var keys = Set<String>()
        for match in expression.matches(in: text, range: NSRange(text.startIndex..., in: text)) {
            guard let range = Range(match.range, in: text) else { throw BindingError.unavailable }
            let token = String(text[range]).dropLast().trimmingCharacters(in: .whitespacesAndNewlines)
            let decoded = try JSONSerialization.jsonObject(with: Data(("[" + token + "]").utf8)) as? [String]
            guard let key = decoded?.first, keys.insert(key).inserted else { throw BindingError.unavailable }
        }
        return value
    }

    func snapshot() throws -> RegistrySnapshot {
        guard let bindingURL = bindingURL else { throw BindingError.unavailable }
        let binding = try object(bindingURL, ownerOnly: true)
        guard let device = binding["device"] as? String,
              device.range(of: "^stackchan-[a-f0-9]{12}$", options: .regularExpression) != nil,
              let digest = binding["ca_sha256"] as? String,
              digest.range(of: "^[a-f0-9]{64}$", options: .regularExpression) != nil,
              let expiry = binding["expiry"] as? NSNumber, String(cString: expiry.objCType) != "c",
              expiry.doubleValue == Double(expiry.int64Value), expiry.int64Value > 0, expiry.int64Value <= 0xffffffff else { throw BindingError.unavailable }
        let base = projectRoot.appendingPathComponent(".tools/persistent-companion").resolvingSymlinksInPath()
        let permissions = try FileManager.default.attributesOfItem(atPath: base.path)[.posixPermissions] as? NSNumber
        guard let mode = permissions, mode.intValue & 0o077 == 0 else { throw BindingError.unavailable }
        let profile = try object(base.appendingPathComponent("profile.json"), ownerOnly: false)
        guard profile["exact_device_id"] as? String == device,
              profile["ca_sha256"] as? String == digest,
              let profileExpiry = profile["remember_until_unix"] as? NSNumber,
              String(cString: profileExpiry.objCType) != "c", profileExpiry.doubleValue == Double(profileExpiry.int64Value), profileExpiry == expiry,
              let path = profile["state_directory"] as? String, path.hasPrefix("/") else { throw BindingError.unavailable }
        let state = URL(fileURLWithPath: path)
        guard state.standardizedFileURL.path == path,
              state.resolvingSymlinksInPath().path == path,
              state.deletingLastPathComponent().path == base.path else { throw BindingError.unavailable }
        let snapshot = try SQLiteRegistryReader(databaseURL: state.appendingPathComponent("home-lab.sqlite3"), deviceID: device).snapshot()
        guard snapshot.pairState != .active || snapshot.expiryUnix == expiry.intValue else { throw BindingError.unavailable }
        // State/count access intentionally cannot verify the stored CA/URI tuple.
        // Availability never grants authority: the worker revalidates that full tuple.
        return snapshot
    }
}
