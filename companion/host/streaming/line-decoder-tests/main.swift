import Foundation

var checks = 0
func check(_ predicate: @autoclosure () -> Bool) {
    precondition(predicate(), "decoder fixture failed at check \(checks + 1)")
    checks += 1
}
func bytes(_ text: String) -> Data { Data(text.utf8) }

// Deliberately mocked boundary: these tests do not run a CMIO pipeline.
final class CompanionWebcamPipeline: WebcamOpenIntentPipeline {
    var received = [Data]()
    var revocations = 0
    var ready = false
    var binding: Int? = nil
    var stopPending = false
    var readyToStart: Bool { ready }
    var hasActiveBinding: Bool { binding != nil }
    var hasPendingStop: Bool { stopPending }
    var starts = 0
    func handleExistingWorkerEvent(_ line: Data) -> Bool { received.append(line); return true }
    func revoke() -> Bool {
        revocations += 1
        guard binding != nil else { return !stopPending }
        binding = nil; stopPending = true
        return false
    }
    func explicitWebcamOn() -> Bool { starts += 1; binding = starts; return true }
}

do { // Fragmentation and coalescing; callbacks retain separate complete events.
    var lines = [Data](); var stops = 0
    let decoder = WebcamWorkerLineDecoder(current: { true }, consume: { lines.append($0); return true }, requestStop: { stops += 1 })
    check(decoder.feed(bytes("{\"type\":")))
    check(lines.isEmpty)
    check(decoder.feed(bytes("\"webcam_off\"}\n{\"type\":\"webcam_off\"}\n")))
    check(lines.count == 2)
    check(lines[0] == lines[1])
    check(decoder.finish())
    check(stops == 1)
    check(!decoder.finish())
    check(!decoder.feed(bytes("later\n")))
    check(stops == 1)
}
do { // Exact line limit, including newline.
    var sizes = [Int](); var stops = 0
    let decoder = WebcamWorkerLineDecoder(current: { true }, consume: { sizes.append($0.count); return true }, requestStop: { stops += 1 })
    check(decoder.feed(Data(repeating: 120, count: 4096)))
    check(decoder.feed(Data(repeating: 120, count: 4095)))
    check(decoder.feed(bytes("\n")))
    check(sizes == [8191])
    check(decoder.finish())
    check(stops == 1)
}
do { // Overlong unfinished line never reaches consumer.
    var received = 0; var stops = 0
    let decoder = WebcamWorkerLineDecoder(current: { true }, consume: { _ in received += 1; return true }, requestStop: { stops += 1 })
    check(decoder.feed(Data(repeating: 120, count: 4096)))
    check(!decoder.feed(Data(repeating: 120, count: 4096)))
    check(received == 0 && stops == 1)
}
do { // Partial EOF is rejected; no manufactured newline.
    var received = 0; var stops = 0
    let decoder = WebcamWorkerLineDecoder(current: { true }, consume: { _ in received += 1; return true }, requestStop: { stops += 1 })
    check(decoder.feed(bytes("{\"type\":\"webcam_fragment\"")))
    check(!decoder.finish())
    check(received == 0 && stops == 1)
}
do { // Owner changes during callback: second event cannot enter replacement.
    var valid = true; var received = 0; var stops = 0
    let decoder = WebcamWorkerLineDecoder(current: { valid }, consume: { _ in received += 1; valid = false; return true }, requestStop: { stops += 1 })
    check(!decoder.feed(bytes("first\nsecond\n")))
    check(received == 1 && stops == 1)
}
do { // Wrong generation at ingress.
    var received = 0; var stops = 0
    let decoder = WebcamWorkerLineDecoder(current: { false }, consume: { _ in received += 1; return true }, requestStop: { stops += 1 })
    check(!decoder.feed(bytes("first\n")))
    check(received == 0 && stops == 1)
}
do { // Consumer failure is terminal, even with complete following events.
    var received = 0; var stops = 0
    let decoder = WebcamWorkerLineDecoder(current: { true }, consume: { _ in received += 1; return false }, requestStop: { stops += 1 })
    check(!decoder.feed(bytes("first\nsecond\n")))
    check(received == 1 && stops == 1)
}
do { // Oversized chunk, including many short lines, is bounded at ingress.
    var received = 0; var stops = 0
    let decoder = WebcamWorkerLineDecoder(current: { true }, consume: { _ in received += 1; return true }, requestStop: { stops += 1 })
    check(!decoder.feed(Data(repeating: 10, count: 4097)))
    check(received == 0 && stops == 1)
}
do { // Reentrant reader callbacks cannot reorder or share partial state.
    var decoder: WebcamWorkerLineDecoder!; var stops = 0
    decoder = WebcamWorkerLineDecoder(current: { true }, consume: { _ in
        check(!decoder.feed(bytes("nested\n"))); return true
    }, requestStop: { stops += 1 })
    check(!decoder.feed(bytes("outer\n")))
    check(stops == 1)
}
do {
    let pipeline = CompanionWebcamPipeline(); pipeline.binding = 1; var pending = 0
    let adapter = WebcamOwnedPipeAdapter(pipeline: pipeline, current: { true }, cleanupPending: { pending += 1 })
    check(adapter.acceptOwnedRead(bytes("{\"type\":\"webcam_off\"}\n")))
    check(pipeline.received.count == 1)
    check(adapter.ownedEOF())
    adapter.ownerLost()
    check(pipeline.revocations == 1 && pending == 1)
}
do { // Explicit open waits for existing permission/clock/extension gates once.
    let pipeline = CompanionWebcamPipeline()
    let lifecycle = WebcamOpenIntentLifecycle(pipeline: pipeline)
    let first = lifecycle.openView()!
    check(pipeline.starts == 0)
    check(!lifecycle.ownerStateChanged(for: first))
    pipeline.ready = true
    check(lifecycle.ownerStateChanged(for: first))
    check(pipeline.starts == 1)
    check(!lifecycle.ownerStateChanged(for: first))
    check(lifecycle.closeView(first) == false) // fake pipeline retains cleanup
    check(pipeline.revocations == 1 && pipeline.stopPending)
}
do { // Close cancels pending intent; old owner callbacks cannot restart it.
    let pipeline = CompanionWebcamPipeline()
    pipeline.ready = true
    let lifecycle = WebcamOpenIntentLifecycle(pipeline: pipeline)
    let old = lifecycle.openView()!
    check(pipeline.starts == 1)
    check(!lifecycle.closeView(old) && pipeline.stopPending)
    check(!lifecycle.ownerStateChanged(for: old))
    let replacement = lifecycle.openView()!
    check(pipeline.starts == 1 && !lifecycle.ownerStateChanged(for: replacement))
    pipeline.stopPending = false // Simulate the joined owner releasing custody.
    check(lifecycle.ownerStateChanged(for: replacement))
    check(!lifecycle.ownerStateChanged(for: replacement))
    check(pipeline.starts == 2)
}
do { // Failed start is consumed; same view notifications cannot duplicate it.
    final class FailingPipeline: WebcamOpenIntentPipeline {
        var readyToStart = true; var hasActiveBinding = false; var hasPendingStop = false; var starts = 0
        func explicitWebcamOn() -> Bool { starts += 1; return false }
        func revoke() -> Bool { true }
    }
    let pipeline = FailingPipeline(); let lifecycle = WebcamOpenIntentLifecycle(pipeline: pipeline)
    let token = lifecycle.openView()!
    check(pipeline.starts == 1)
    check(!lifecycle.ownerStateChanged(for: token))
    check(pipeline.starts == 1)
}
do { // Concrete adapter forwards gates, start and revoke to the exact pipeline.
    let pipeline = CompanionWebcamPipeline(); pipeline.ready = true
    let adapter = CompanionWebcamOpenIntentAdapter(pipeline: pipeline)
    check(adapter.readyToStart && !adapter.hasActiveBinding && !adapter.hasPendingStop)
    check(adapter.explicitWebcamOn() && pipeline.starts == 1 && adapter.hasActiveBinding)
    check(!adapter.revoke() && adapter.hasPendingStop && !adapter.hasActiveBinding)
}
for action in ["read", "eof", "ownerLost"] {
    // A delayed callback belongs to old binding 1, never replacement 2.
    let pipeline = CompanionWebcamPipeline(); pipeline.binding = 1
    var current = true; var pending = 0
    let adapter = WebcamOwnedPipeAdapter(pipeline: pipeline, current: { current }, cleanupPending: { pending += 1 })
    pipeline.binding = 2; current = false
    if action == "read" { check(!adapter.acceptOwnedRead(bytes("stale\n"))) }
    else if action == "eof" { check(!adapter.ownedEOF()) }
    else { adapter.ownerLost() }
    check(pipeline.binding == 2 && !pipeline.stopPending)
    check(pipeline.revocations == 0 && pipeline.received.isEmpty && pending == 0)
}
do { // Even a current process callback cannot deliver to a different purpose.
    let pipeline = CompanionWebcamPipeline(); pipeline.binding = 1
    let adapter = WebcamOwnedPipeAdapter(pipeline: pipeline, current: { true }, cleanupPending: {})
    pipeline.binding = 2
    check(!adapter.acceptOwnedRead(bytes("foreign-purpose\n")))
    check(pipeline.binding == 2 && pipeline.revocations == 0 && pipeline.received.isEmpty)
}
do { // Construction without an active binding cannot attach itself later.
    let pipeline = CompanionWebcamPipeline()
    let adapter = WebcamOwnedPipeAdapter(pipeline: pipeline, current: { true }, cleanupPending: {})
    pipeline.binding = 2
    check(!adapter.acceptOwnedRead(bytes("late-binding\n")))
    check(pipeline.binding == 2 && pipeline.revocations == 0 && pipeline.received.isEmpty)
}
do { // Owner validation can synchronously install a replacement before return.
    let pipeline = CompanionWebcamPipeline(); pipeline.binding = 1
    let adapter = WebcamOwnedPipeAdapter(pipeline: pipeline, current: {
        pipeline.binding = 2; return true
    }, cleanupPending: {})
    check(!adapter.acceptOwnedRead(bytes("reentrant-replacement\n")))
    check(pipeline.binding == 2 && pipeline.revocations == 0 && pipeline.received.isEmpty)
}
do { // Lost authority still stops the original active owner exactly once.
    let pipeline = CompanionWebcamPipeline(); pipeline.binding = 1; var pending = 0
    let adapter = WebcamOwnedPipeAdapter(pipeline: pipeline, current: { false }, cleanupPending: { pending += 1 })
    check(!adapter.acceptOwnedRead(bytes("retired\n")))
    adapter.ownerLost(); check(!adapter.ownedEOF())
    check(pipeline.binding == nil && pipeline.stopPending && pipeline.revocations == 1 && pending == 1)
}
print("\(checks) decoder/adapter checks passed; no device/CMIO activation")
