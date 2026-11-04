# CoreS3 reset research and discard exclusion

Local source baseline: `1fe663a494cf79460d54ab57ba609630b9144505`.

The [official CoreS3 pin map](https://docs.m5stack.com/en/core/CoreS3)
identifies AW9523B P0_2 as AW88298 reset. The
[M5Unified CoreS3 implementation at 44952cb8](https://github.com/m5stack/M5Unified/blob/44952cb8e4ac88bf04ad23b9dfd6f4b05c788348/src/M5Unified.inl#L601-L630)
provides a hardware-reset lead, not proof that this repository can safely flush
and resume playback. No external implementation or new dependency is copied
by this change; the existing HAL remains in place.

A repeatable reset would need to coordinate the expander's other pins and bus
users, stop both I2S directions (RX format setup can restart TX), invalidate
software/DSP and DMA state, restore amplifier configuration quietly after
reset, and preserve the exact audio owner's custody through every failure.
The driver's cached state must agree with freshly restored hardware state.
An interrupted reset or reinitialization must retain custody rather than
claiming a flush or allowing a successor to replay old buffers. The existing
terminal retirement API deliberately does not supply those restart guarantees.

## Implemented source improvement

The recursive CoreS3 IO mutex excludes other threads but permits a driver
callback on the same thread to enter another operation. Previously, after
both directions stopped, a volume callback could complete DMA discard while
the outer amplifier write remained in progress. Conversely, a preload
callback could modify amplifier volume during discard.

`MuseDiscardOutputChecked` now rejects entry during an active volume operation
before consuming its ticket. `SetOutputVolume` refuses a nested write while
Muse discard is active. Once the transaction exits, normal volume updates
remain available. No automatic retry or deferred volume write is introduced.

The production-body regression reproduces the old behavior, then verifies
both directions of exclusion, no IO on rejected admission, ticket reuse after
rejection, preserved current ownership and volume updates after discard.
Existing DMA bounds, partial-write, replay, fault and repeat-cycle tests remain.

## Remaining qualification

`CoreVoiceCodec::FlushOutput` remains false. Neither a stopped-DMA overwrite
nor register shutdown proves physical FIFO discard or interruption latency.
A separate reset/reinitialization design and attended real-device testing
are still necessary before advertising repeatable physical flush. Required
observations include repeated interruptions, marker-buffer replay, quiet
restart and failures at each reset/configuration step. No physical device,
flash, microphone, cloud session or credential operation is part of this work.
