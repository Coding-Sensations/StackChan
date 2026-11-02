# Muse Core Audio Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Wire checked CoreS3 and bounded PCM to the retained Muse direction owner without creating authority or claiming physical acceptance.
**Architecture:** Existing C++ owner passes its exact lease and fresh-clock operation gate. A single-use synchronous codec backend delegates existing Core/DSP operations and retains cleanup failures.
**Tech Stack:** C++17, ESP-IDF5.5.4, existing ESP AE converter, actual production Core fault fixtures and voice core.
**Spec:** ../specs/2026-11-02-muse-core-audio.md

## Global Constraints

No flash, real credentials, live IO, security change, merge or paid usage. Root sole writer; existing reviewer read-only. Stock/Mira and layout/trust preserved. Native permits/physical flush unavailable by default.

## Review Focus

- Revocation/expiry in clock/DSP/driver callbacks must prevent the next IO or append.
- Partial capture startup must preserve samples without unbounded reads.
- Wrong/stale leases must neither touch hardware nor revoke a newer owner.
- Paired TX/never-opened resources must still be checked during cleanup.
- DSP reset/carry wipe is not a physical playback flush.

### Task1: Exact resource and fresh owner checks

Files: muse_direct_audio.hpp, muse_voice_codec_owner.hpp; native composition fixture.
- [x] Write/run RED for original lease forwarding, unavailable legacy binding and fresh-clock expiry/revocation.
- [x] Add opaque ResourceLease accessor and fail-closed Codec::ReserveExact; keep old Reserve unavailable for native binding. Add optional existing Clock to Owner and use fresh guarded time for protocol operations.
- [x] Add explicit Complete/Pending/Failed capture result and force checked cleanup of both directions after binding. Run affected host fixtures.

### Task2: Bounded native Core/DSP backend

Files: new muse_voice_core_codec.hpp, muse_voice_pcm.hpp; actual Core fixture extraction and new composition test.
- [x] Write/run RED for320-frame carry, bounded split output, callout revocation, reset/wipe and retained failure.
- [x] Add guarded Core proxy checking Owner operation gate with fresh Clock at every physical IO; use PcmAdapter and832-sample carry,512 temporary conversion,320 playback chunks.
- [x] Add selected-direction DSP reset/HasLease support with cleanup allowed after revoke. Native physical flush returnsfalse and actual capture/playing remain unasserted.
- [x] Run sanitized actual Core/voice-core integration, not mocked duplicate state-machine code.

### Task3: Qualification and publication

- [x] Native target checks on/off, full eligible host suite and18 profile tests, one job; compare immutable parents and resource/layout/trust boundaries.
- [ ] Fresh independent exact-head review; correct findings and rerun affected checks.
- [ ] Publish reviewed source with honest native/physical gates, preserve user merges, verify remote head; normal free CI without waiting.
