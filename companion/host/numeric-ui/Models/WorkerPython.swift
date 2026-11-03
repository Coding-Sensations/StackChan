import Foundation

/// Resolve only local executable files; this never probes or launches an interpreter.
enum WorkerPython {
    static func resolve(environment: [String: String], root: URL) -> URL? {
        func executable(_ path: String) -> URL? {
            guard path.hasPrefix("/") else { return nil }
            var isDirectory: ObjCBool = false
            guard FileManager.default.fileExists(atPath: path, isDirectory: &isDirectory),
                  !isDirectory.boolValue, FileManager.default.isExecutableFile(atPath: path) else { return nil }
            return URL(fileURLWithPath: path)
        }
        // An explicit invalid override must not silently select a different environment.
        if let configured = environment["STACKCHAN_PYTHON"] { return executable(configured) }
        for name in ["python3", "python"] {
            if let python = executable(root.appendingPathComponent(".venv/bin/" + name).path) { return python }
        }
        for directory in (environment["PATH"] ?? "").split(separator: ":", omittingEmptySubsequences: false) {
            guard directory.hasPrefix("/") else { continue }
            if let python = executable(URL(fileURLWithPath: String(directory)).appendingPathComponent("python3").path) { return python }
        }
        return nil
    }
}
