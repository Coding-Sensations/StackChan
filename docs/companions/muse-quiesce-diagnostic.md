# Inert quiesce diagnostics and unsupported recovery

`companion::muse::QuiesceDiagnostic` is a caller-owned, single-use source observation. Construction, `Read()` and bounded `Format()` do no IO. There is no Run, reset, recover, retry, release, permission or public receipt-ingestion API, no automatic task/listener/registration and no new device caller.

The existing explicit `CoreS3AudioCodec::MuseQuiesceForRetirementChecked(original_lease, ticket, optional_diagnostic)` may privately fill one collector during its synchronous operation. The default null argument preserves its previous effects. The producer captures the original opaque AudioLease value before callbacks; the collector stores its own private copy/ticket, exposing neither. Source-only example, not an activation instruction:

```cpp
companion::muse::QuiesceDiagnostic report;
// Only an independently authorized original owner may invoke existing cleanup.
auto evidence = core.MuseQuiesceForRetirementChecked(original_lease, ticket, &report);
char text[companion::muse::QuiesceDiagnostic::TextCapacity];
if (report.Format(text, sizeof text)) { /* explicitly display locally */ }
```

The caller retains the collector through the synchronous call. Empty/recording reads expose only default data. A release/acquire one-time publication makes the ready projection immutable, including after the original lease has been retired. Reuse or reentry of a collector refuses before IO and never overwrites earlier evidence. A new collector can observe a rejected operation; it cannot bypass the underlying single-attempt binding. Reporter errors confer no cleanup authority.

Every text report begins `snapshot-only no-authority physical-flush=unqualified`. It contains only fixed labels and numeric register/status/byte evidence, source rejection reason, and explicitly historical held/retained/parked flags. No tokens, addresses/pointers, VM/session/chat/account/device identities or arbitrary private strings are accepted or formatted. Small buffers return false with an empty C string, never a partial success message. Readback fields from failed stages are historical/invalid as indicated by the recorded fault; zero callback status does not override a mismatch. This is not a current hardware, principal, resource-join or acoustic receipt.

Recovery classification is descriptive only. Successful register/stopped-DMA evidence still says `owner-cleanup-still-required`; original checked cleanup/worker joins remain mandatory. A retained failure says `retained-fresh-state-required-no-retry`. Active-operation rejection is no new observation, not proof of failed quiesce. An unmatched key does not borrow another binding's retained/parked state. Original custody loss is reported without authorizing action on the successor.

## Exact pinned recovery limit

Local source inspected: ESP-IDF tag v5.5.4, commit [735507283d5b2f9fb363a1901172dbd9e847945d](https://github.com/espressif/esp-idf/tree/735507283d5b2f9fb363a1901172dbd9e847945d). In [i2s_common.c](https://github.com/espressif/esp-idf/blob/735507283d5b2f9fb363a1901172dbd9e847945d/components/esp_driver_i2s/i2s_common.c), disable rejects an already-READY channel before resetting `dma.curr_ptr`/`rw_pos`; preload uses the existing cursor unless it is null and advances it by bytes copied. A failed/partial write cannot be safely replayed by assuming descriptor zero.

The READY [standard reconfiguration APIs](https://github.com/espressif/esp-idf/blob/735507283d5b2f9fb363a1901172dbd9e847945d/components/esp_driver_i2s/i2s_std.c) provide no same-format cursor-reset contract. Changing slot size can reallocate descriptors; that is not an approved recovery mechanism. Enabling TX just to make disable succeed would generate hardware clocks and may transmit data. Destroying/recreating channels would require requalifying paired codec data interfaces, ISR lifetime and custody. Private driver-structure mutation is excluded.

Therefore no physical recovery mechanism is implemented or inferred here. Failed quiesce retains its original binding, all uncertain retries stay rejected, and physical FlushOutput/PTT remain disabled. A separately designed/qualified fresh hardware state is still required after partial failure.

## Offline evidence and later device scope

Fixtures compile the actual Core/collector source with inert ports, preserving all 57 prior amplifier cases. Added cases cover default/no-IO behavior, complete/retained/unavailable/lost-binding records, source reason, active-versus-failed classification, reentry/reuse, concurrent snapshots, non-overwriting publication, identity-free output and buffer bounds. A failing fixture process ending is not device recovery. No recording, real token/account, endpoint, amplifier/microphone/device operation or firmware installation occurs.

Later device qualification needs an exact reviewed opt-in diagnostic image, current identity/partition/recovery/custody preflight and separate approval for installation/reset, low-volume synthetic output tests, any RX/microphone activation and any external recording/measurement. Measure register and channel state, interruption latency, stale-audio replay across restart and failure recovery; listening alone cannot establish universal acoustic silence. The report cannot approve these actions. No runtime diagnostic or live route is activated by this source PR.
