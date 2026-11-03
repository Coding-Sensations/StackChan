# Production source path and next integration boundary

The source contains a production camera implementation and a CMIO sink. Earlier
checkpoint statements that these implementations were absent were too broad;
the missing parts are their admitted lifecycle integration and qualification.

On the robot, `firmware/main/hal/board/stackchan_camera.{h,cc}` exposes
`StartBound`, `CaptureBound`, `CopyBGRA`, `Revoke` and `StopCheckedReceipt`.
`CaptureBound` observes V4L2 DQBUF time/sequence before requeueing the DMA buffer.
The source driver under `firmware/components/espressif__esp_video` provides
`esp_video_capture_clock_contract_revision`, DVP capture clock observations and
V4L2 timestamp/sequence propagation. This is completion timing, not exposure
start. `camera_capture_clock.hpp` deliberately returns false from
`CaptureOriginQualified`; merely parsing timestamps or testing software cannot
enable qualified physical capture. No SDK/model question is needed to identify
these implementation paths. Attended evidence and genuine stop/timing
qualification are still required before runtime activation.

On the Mac, the existing `CompanionWebcamPipeline` in
`companion/host/numeric-ui/Services/WebcamFrameReceiver.swift` already validates
WCV1 frames and uses `WebcamBridgeAdapter`. Its production
`companion/webcam/CompanionSinkBridge.m` puts authorized BGRA sample buffers into
the CoreMediaIO sink queue. `CameraExtension.m` consumes them and sends output
sample buffers. The controller currently supplies no qualified camera owner and
does not approve an installed extension at boot. No extension discovery,
installation, approval or stream start is added by this successor component.

## New local consumer component

`WebcamWorkerLineDecoder.swift` accepts <=4096-byte reads from an exclusive video
pipe, retains at most one <=8191-byte unfinished JSON payload, and forwards only
newline-complete events. EOF with a partial event is rejected. Owner loss,
oversized reads/lines, consumer rejection, blank lines and reentrant feeds retire
the decoder and request stop exactly once. Clean EOF requests stop as well; it
proves framing only. The caller must still join its reader and the existing
pipeline must retain physical stop custody. Stop callbacks do not mint receipts.

`WebcamOwnedPipeAdapter.swift` connects that decoder to the existing pipeline's
worker-event consumer and revocation method. The caller supplies an exact
process/session-generation check. It opens no FD, launches no task, admits no
camera purpose and attaches no extension. Neither new file is instantiated by
the running application. Both live beside the existing service sources, so the
normal service source glob includes them in a future reviewed app build.

Construct the pipe adapter after its intended purpose binding is active. It
snapshots that full binding and cannot adopt a replacement purpose. Delayed
reads, EOF or owner-loss callbacks retire only that decoder when the shared
pipeline has moved to a different binding. The existing W1 CI fixture exercises
these paths through the real pipeline with explicitly synthetic stop evidence.

`WebcamOpenIntentLifecycle.swift` adds a volatile one-shot view-open coordinator
and `CompanionWebcamOpenIntentAdapter` that forwards to the existing pipeline's
`ready`, `binding`, `stopPending`, `explicitWebcamOn()` and `revoke()` contract.
Open waits for those existing gates and clear stop custody, consumes the start
intent before calling the owner, and invalidates its token synchronously on
close. The coordinator and adapter are not yet instantiated by the running
application; permission callbacks, producer pump/reader ownership, and admitted
clock/stop authority remain unintegrated.

The decoder fixtures use Foundation only and a mocked pipeline boundary. They
exercise fragmented/coalesced lines, exact limits, partial EOF, stale ownership,
consumer failure, callback reentrancy and stop custody. They are framing and
adapter tests, not CMIO consumption or camera proof.

## Remaining implementation

An explicitly admitted owner must run the producer pump around the existing
bound camera methods, retain current identity/timing/permission checks and join
it on close. A dedicated pipe reader must bind each read/EOF callback to the
exact generation and stop/join the same reader before replacement. The pipe
adapter revokes only the matching pipeline; it cannot be reused for another
session. Qualified authority/clock/stop observations still need to bridge into
the existing `AuthenticatedWebcamOwner` without treating peer fields as
evidence. Then the application must connect existing view and permission-state
callbacks to the tested coordinator. No extra Stop control is required.
