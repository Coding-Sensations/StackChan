import Foundation

/// Bounded framing for an exclusively owned video pipe. Main-queue serialized.
/// Current process/session ownership is supplied by the caller, never inferred
/// from JSON. Consuming a line is not a CMIO or physical-stop receipt.
final class WebcamWorkerLineDecoder {
    static let maximumChunkBytes = 4096
    static let maximumLineBytes = 8192 // includes exactly one trailing newline
    private let current: () -> Bool
    private let consume: (Data) -> Bool
    private let requestStop: () -> Void
    private var partial = Data()
    private(set) var retired = false
    private var feeding = false

    init(current: @escaping () -> Bool, consume: @escaping (Data) -> Bool,
         requestStop: @escaping () -> Void) {
        self.current = current
        self.consume = consume
        self.requestStop = requestStop
    }

    private func stop() {
        guard !retired else { return }
        retired = true
        partial.removeAll(keepingCapacity: false)
        requestStop() // exact existing owner retains custody until checked joins
    }

    /// Reader must deliver chunks <=4096 bytes without unbounded preprocessing.
    /// At most one unfinished line is retained. A callback may retire ownership;
    /// later lines in the same chunk are then rejected.
    @discardableResult func feed(_ chunk: Data) -> Bool {
        precondition(Thread.isMainThread)
        guard !retired else { return false }
        guard !feeding, current(), chunk.count <= Self.maximumChunkBytes else {
            stop(); return false
        }
        feeding = true
        defer { feeding = false }
        for byte in chunk {
            guard !retired, current() else { stop(); return false }
            if byte == 10 {
                guard !partial.isEmpty else { stop(); return false }
                // Framing strips newline; existing bounded JSON/WCV1 consumer
                // still validates schema, purpose, timestamp and fragment size.
                let line = partial
                partial = Data()
                guard consume(line), !retired, current() else { stop(); return false }
            } else {
                guard partial.count < Self.maximumLineBytes - 1 else {
                    stop(); return false
                }
                partial.append(byte)
            }
        }
        return !retired
    }

    /// Clean EOF means framing only. Even clean EOF revokes downstream delivery.
    /// No incomplete final event is ever forwarded, padded or reused on reopen.
    @discardableResult func finish() -> Bool {
        precondition(Thread.isMainThread)
        guard !retired else { return false }
        let clean = !feeding && current() && partial.isEmpty
        stop()
        return clean
    }

    func revoke() {
        precondition(Thread.isMainThread)
        stop()
    }
}
