# Muse native codec composition

The user requests continued independent C++/ESP source PRs, reusing existing work, with Stock/Mira preserved and no flash/live/token actions. The chosen implementation is native in this existing sole-writer lane plus independent read-only review; routine implementation choices are delegated.

Compose the existing original-lease CoreS3 API and ESP PCM adapter with the retained direction Owner. Generation alone must never mint/borrow an audio owner. Native binding requires the original opaque lease, selected directions, expiry, a fresh existing Clock and an operation gate from that Owner's existing Permit. Legacy generation-only adapters remain unavailable.

Capture returns320-sample16k frames with bounded carry832, one raw480-frame read per call and explicit Pending for legal converter startup/short output. Playback accepts up to1282 samples in at most5 converter/driver chunks, rechecking fresh native authorization at each physical IO. Wipe all owned frames/carry and reset selected DSP history on direction stop. Never replay uncertain writes.

Cleanup after revoke must stop both owned Core directions, including paired TX/never-opened bindings, then clear DSP/join synchronous codec work and retire hardware without releasing logical custody. Owner releases its exact original lease last. Failed stops retain. Observations describe checked open/close only; no actual capture/playing/acoustic inference. Physical flush remains unavailable, so a native PTT path requiring it fails closed rather than acknowledging software clearing as hardware completion. No live route/permission/actor authority is created.

Use current reviewed PR24+PR25 source. Preserve their individual branches, profile/partition/config/trust/assets/other providers. Real authority/startup/flush/observation/heap/device acceptance remain open.
