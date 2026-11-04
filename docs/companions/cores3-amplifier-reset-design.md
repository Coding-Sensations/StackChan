# CoreS3 quiet amplifier reset: design (port implemented, unverified on hardware)

Base: PR #43 head `73cf3b34dfb087023286dfba38b87c0fea596558`
(`local/cores3-audio-research`). Status: codec operation and host tests
implemented (`MuseResetAmplifierChecked`, `firmware/tests/muse_amp_reset_test.cpp`).
Board port: `Aw88298ResetPort` (`firmware/main/hal/board/stackchan.cc`) is injected once
in `GetAudioCodec()` behind the Muse build flags. It drives AW9523 P0_2 through a checked
read-modify-write (`aw9523::SetP0Bit`, host-tested in `aw9523_p0_test`): verifies the pin is
configured as an output, reads the latch, writes only that bit, reads back, and fails on any
bus error or mismatch. It never uses `WriteReg` (which aborts on bus error), and the unsafe,
uncalled `ResetAw88298` was removed. Firmware is NOT compiled in this tree's environment (no
ESP-IDF), so the board code is source-reviewed and host-tested only; not hardware-accepted.
`FlushOutput` stays false and no hardware claim changes. Implementation notes: reuses
`muse_quiescing_` as the busy fence; a reset immediately after a completed discard fails
closed until a fresh stop resets the DMA preload cursor.

## What the source already shows

1. `Aw9523::ResetAw88298()` (`firmware/main/hal/board/stackchan.cc:244`) already
   encodes the board sequence: AW9523 P0 register `0x02` to `0b011` (P0_2
   low), 10 ms, `0b111`, 50 ms. It has **no callers**; it is dead code.
2. M5Unified at `44952cb8` uses the same P0_2 (`0x02` bit 2) differently: set on
   speaker enable, cleared on speaker disable, with AW88298 register writes
   (`0x61`,`0x04`,`0x05`,`0x06`,`0x0C`) after enable. So P0_2 is both the
   amplifier reset line and, in M5Unified, its power-state switch. Neither
   source documents what the amplifier outputs while P0_2 is low or during the
   50 ms release; that is an attended-measurement question, not a source fact.
3. `ResetAw88298` is unsafe to call as-is:
   - It writes the whole P0 output register blind. Any other P0 output bit
     changed after `Aw9523` construction would be overwritten. CONFIG_P0 makes
     only bits 3 and 4 inputs, so bits 0, 1, 2, 5, 6, 7 are all driven outputs.
   - It is private to the board class. The codec's `direct_io_mutex_` does not
     cover it, so I2C traffic and I2S state can interleave with the reset.
   - It reports no result. I could not confirm `I2cDevice::WriteReg` error
     behavior from this tree (the base class is not in `firmware/main` or the
     checked-in components); verify before relying on it.
   - It does not touch the codec driver. `aw88298_cfg.reset_pin` is
     `GPIO_NUM_NC`, so `esp_codec_dev` and the AW88298 driver never know a
     reset happened. After reset the amplifier's registers return to defaults
     while `out_codec_if_`'s enabled/volume cache and the `codec_quiet`
     readback assumptions (reg 4/5) still describe the old state.
4. The existing terminal retirement path (`MuseQuiesceForRetirementChecked`)
   writes mute/system registers, stops both I2S directions, zero-fills stopped
   DMA and reads back. It is one-shot (`muse_last_quiesce_ticket_`), sets
   `muse_quiescent_` permanently, and never restores operation. It is not a
   flush; this design must not weaken it.

## Proposed operation: `MuseResetAmplifierChecked(lease, ticket)`

A new, separate, checked operation on the codec, not a change to quiesce.
It is non-terminal: on success the same lease may continue; on any failure the
custody stays retained exactly as quiesce failure does today.

Admission (all under `direct_io_mutex_`, before any IO, ticket not consumed on
rejection, matching the #43 pattern):
current lease via `MuseMatch` and `CommonAudioAuthority().Current`; not retired,
not quiescing/quiescent/discarding; `!muse_volume_active_`;
`!direct_call_ && !muse_stop_active_`; strictly increasing ticket
(`muse_last_reset_ticket_`, new, single use even on failure); and a new
`muse_resetting_` flag that every other Muse entry point, `SetOutputVolume`
and the preload callback must treat like `muse_discarding_`.

Sequence, each step current-lease and binding checked, each recording a phase
and status in an evidence struct (like `MuseQuiesceEvidence`):

1. **Mute first.** `codec_quiet::Quiesce` (existing RMW and readback) so the
   amplifier is already quiet before reset asserts. Failure: retain, stop.
2. **Stop both I2S directions** (`DirectStopChecked` for input then output;
   RX format setup can restart TX, as the existing discard comment notes).
3. **Zero stopped DMA** via `MuseDiscardStopped`, using its own single-use
   ticket. This clears software-visible DMA only; it is not FIFO proof.
4. **Assert reset**: expander P0_2 low using a *read-modify-write* of register
   `0x02` that preserves other bits, then read back the output latch. The
   expander must be reached through one shared, mutex-protected accessor
   exposed to the codec through an injected interface, not by reaching into the
   board class. Hold low for the datasheet minimum (confirm against the
   AW88298 and AW9523B datasheets; the 10 ms in `ResetAw88298` is an existing
   value, not a verified minimum).
5. **Release reset**: RMW P0_2 high, read back, wait for the AW88298
   power-up time (existing code uses 50 ms; unverified).
6. **Re-establish amplifier state**: re-run the codec driver's open/enable path
   so its cached state matches fresh hardware, then re-apply the muted state
   (`codec_quiet`) and verify registers 4 and 5. The result must be
   *registers verified muted*, not unmuted.
7. **Re-arm gates**: reset `direct_*_stopped_` evidence, DMA preload cursor and
   fence generation so a successor cannot replay old buffers; bump the
   output epoch so queued stale output is rejected.

Failure at any step: set `muse_quiesce_retained_`, revoke the lease, report the
phase. Do not retry automatically. A reset that fails between steps 4 and 6
leaves the amplifier in unknown state; custody is retained and the device must
stay muted pending attended diagnosis.

Outcome vocabulary stays honest: `AmplifierResetRegistersVerifiedMuted`. This
does **not** imply physical FIFO discard, audible silence, or interruption
latency. `CoreVoiceCodec::FlushOutput` stays false until attended
observations (below) justify wiring it to this operation.

## Tests (host, production-body fixtures like `muse_dma_discard_test.cpp`)

Fixtures to add: a fake expander accessor recording every register write and
readback; failure and corruption injection at each numbered step (as the existing
`fail_at`/`corrupt_at` amplifier fixture does).

- Admission rejects without consuming the ticket: during volume, discard,
  quiesce, stop, or nested from a callback; stale/old ticket; wrong lease.
- Expander writes preserve non-P0_2 bits (seed arbitrary other bits).
- Order is mute, stop both, discard, assert, release, reconfigure, verify.
- Failure at each step retains custody, revokes the lease, makes no later IO,
  and does not free a successor to capture (`MuseCodecRelease` still refuses).
- After success, the driver cache agrees with hardware fake, DMA cursor and
  epoch are fresh, and a replayed old-generation buffer is rejected.
- Repeat cycles (N resets) show bounded state: no leaked ticket, counter, or
  worker.

## Attended acceptance (needs a scoped device session; nothing here runs it)

Record, per cycle, audible result and instrument evidence, not only status
codes: cold boot, warm start, reset after playback, reset during playback
(interrupted output), repeated retry, mute/stop, PTT press/release, and
failure injection if the firmware exposes it. Measure speaker line for
pop/click at assert and release, residual output after stop, I2C error counts,
reset reasons, heap/PSRAM and worker counts. Stop on persistent output after
stop, unexpected reset, or unintended capture. Leave the device muted
during diagnosis.

## Open items to resolve before implementation

- AW9523B and AW88298 datasheet reset minimum widths and power-up times.
- `I2cDevice::WriteReg`/`TryReadRegs` error semantics and whether the I2C bus
  is shared with touch/PMIC traffic that would need the same lock.
- Resolved in source: `I2cDevice::WriteReg` is `ESP_ERROR_CHECK` (aborts); `TryReadRegs` returns `esp_err_t`.
  The port uses `TryReadRegs` plus raw `i2c_master_transmit`. Still open: whether touch/PMIC/camera
  traffic needs one shared lock around multi-transfer sequences (single transfers are serialized by the bus).
- Datasheet search found no AW88298 reset timing; AW9523B notes are driver comments, not datasheet
  minimums. 10 ms hold / 50 ms settle remain UNVERIFIED existing values.
- Whether the AW88298 driver (`esp_codec_dev` 1.5.4) can be reopened cleanly
  after a hardware reset, or the codec interface must be rebuilt.
- Whether the amplifier is audibly silent while reset is asserted
  (datasheet/measurement); this decides if step 1 muting is sufficient.
