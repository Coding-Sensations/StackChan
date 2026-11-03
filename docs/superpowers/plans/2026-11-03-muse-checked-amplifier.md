# Checked Muse Amplifier Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL superpowers:executing-plans; sole native source writer, fresh independent branch review.

**Goal:** A checked, evidence-preserving terminal amplifier/DMA quiesce boundary, with physical PTT still disabled.
**Architecture:** Pure bounded AW88298 register adapter plus exact-lease Core composition, one-way parking and retained cleanup; reuse original stop/discard with optional raw error outputs.
**Tech Stack:** Existing C++17, esp_codec_dev1.5.4 interfaces, ESP-IDF5.5.4, ASan/UBSan inert host and native compiler.
**Spec:** ../specs/2026-11-03-muse-checked-amplifier.md

## Global Constraints
No device/live/credentials/security/GitHub merge/paid actions. Never return physical success from register/DMA evidence. Preserve shared provider/audio behavior and approved source ancestry.

## Review Focus
- Cached driver state cannot substitute for fresh reads; every failed call preserves stage/status.
- RX reopen cannot restart TX after evidence; park both directions for this binding.
- Original lease only; stale tickets, reentry and wrong owner do no IO.
- Unknown/failed quiesce cannot be retired or silently retried; partial preload has no qualified cursor reset, so the binding permits no second attempt.
- Successful register/DMA evidence never enables physical flush, native PTT, microphone or speaker.

### Task1: Checked adapter and terminal Core composition
Files: new Muse AW88298 checked header; cores3_audio_codec.h/.cc; actual production-body fixture/interface tests; docs.
- [ ] RED missing exact-lease quiesce API; successful evidence is intermediate only, enables/read/write remain parked, release requires known cleanup.
- [ ] Implement checked register RMW/cache-disable/readback, optional stop status, private stopped-DMA helper, terminal parking/retention and original custody fencing.
- [ ] GREEN injected status/mismatch/partial DMA/paired-TX restart/reentry/stale/wrong owner/revocation/custody loss failures; preserve existing assertions.
- [ ] Full host/profile/native checks; fresh review; correct stacked draft PR and exact remote head, without CI wait.
