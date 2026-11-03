# Inactive worker fragment emitter

`webcam_worker_emitter.BoundedWebcamWorkerEmitter` is a pure adapter; no production
caller imports it. Existing webcam unavailable defaults remain unchanged. It
creates no owner, permission, clock calibration, physical Stop receipt, IO or CMIO
readiness.

Construct with an existing `WebcamPurpose` and required injected callbacks:
`current(purpose) -> exactly True`,
`map_capture_clock(purpose, original_producer_nanoseconds) -> MappedCapture`, and
`clock() -> monotonic seconds`. These are trusted caller contracts, not a way to
qualify a physical producer. The assembler retains original validated WCV1
nanoseconds in `VideoFrame.producer_capture_ns` when it completes a frame; the
emitter consumes that field without reconstructing time. Older four-argument
synthetic `VideoFrame` construction remains supported but defaults to missing
producer metadata and cannot be emitted. No packet-arrival/host-time fallback
exists. An optional `producer_capture_ns=` argument to `offer` only asserts exact
equality with the retained field and cannot fill missing metadata. The existing
injected mapper cross-checks retained producer time against mapped host capture
on offer and every pull; no qualification is inferred from bytes or JSON.

`offer(frame)` retains one latest frame. A newer sequence
replaces even a partially emitted old frame. `next_event(controls_drained=True,
audio_drained=True)` returns at most one compact JSON payload, without newline;
the future caller may append one newline to its existing worker pipe. The object
contains exactly `type: webcam_fragment` and base64 `wire`, matching Swift
`CompanionWebcamPipeline.handleExistingWorkerEvent`. Wire bytes reuse the existing
WCV1 encoder and stay <=4112; base64 <=5484; payload plus newline <=8192. Both
scheduling flags must be exactly True and never establish camera authority.
Rejections revoke/drop all pending video. `revoke()` does not confirm physical Off
or release the caller's retained Stop cleanup custody. Caller serializes access.

A 307200-byte frame needs 76–83 fragments depending on identifier lengths. One
fragment per 20ms audio tick takes at least 1.52 seconds, exceeding the 250ms
capture freshness limit. The synthetic fixed-clock roundtrip is wire/schema
coverage only. A real scheduler needs separately reviewed bounded delivery and
backpressure behavior; this patch does not supply or qualify that scheduler.
The increasing-clock test intentionally demonstrates rejection before a complete
frame at a 20ms-per-fragment schedule. No frame rate, camera timing, CMIO or live
end-to-end acceptance is claimed.

`test_webcam_worker_emitter.py` uses only synthetic bytes, injected callbacks,
JSON/base64 and the existing pure `WebcamAssembler`. It neither imports the worker
runtime nor starts any process, socket, native library, camera or model. Root
runs it serially under the existing audit/guard. Physical camera/extension/runtime
activation remains a separate agreed scope.

## Proposed scheduling interface (design only)

A future serialized caller could add an `iter_budget` pull iterator with explicit
maximum fragment count (initial ceiling eight), maximum encoded bytes (including
one newline each, ceiling 65536), and an injected monotonic deadline (initial
budget at most 2ms). A required `media_ready()` scheduler callback would check
pending controls/audio immediately before each pull and after each yielded event;
False ends this budget without consuming another fragment. The current exact
purpose/clock checks would still execute for every fragment. Yield one payload
at a time, retain no batch list, and reserve the maximum event size before each
pull so the byte cap cannot be crossed by consuming an unreturnable fragment.
New frame offers retain latest-only replacement semantics. Deadlines and counters
would not renew permission or capture freshness. This interface is not implemented.

The caller must write nonblockingly to a separately bounded pipe buffer; a pure
iterator cannot interrupt a blocking write or enforce elapsed time inside owner
callbacks. Hard fragment/byte caps remain necessary even if clock reads stall.
After any pending control/audio or exhausted deadline, return control to the
existing pump before another video budget. A later review must choose a total
per-frame/scheduler budget and test saturation, not assume eight-fragment bursts
meet 250ms or 10fps. Synthetic progressing-clock/backpressure/interruption tests
can establish local budgeting behavior; only separately scoped live measurement
can qualify the actual scheduler, pipe and CMIO timing.
