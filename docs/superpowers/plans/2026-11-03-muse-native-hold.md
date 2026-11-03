# Native Muse Hold Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL superpowers:executing-plans; sole native source writer and fresh independent reviewer.

**Goal:** Genuine registered held-input source and bounded retained PTT owner wiring with production physical-flush gate closed.
**Architecture:** Private native pointer ticket queried under checked UI access; separate microphone/context grant checks; runtime handles release before normal ticks and one capture per poll. Codec capability defaults unavailable.
**Tech Stack:** Existing C++17/LVGL/ESP-IDF, actual SDK/Core fixtures, ASan/UBSan and cached real-LVGL simulator.
**Spec:** ../specs/2026-11-03-muse-native-hold.md

## Global Constraints
Source only; no device/live/credentials/security/merge. Existing independent audio consent, exact target/generation, five identities/Stock and cleanup preserved. Physical-flush unavailable in production, never replaced with digital discard.

## Review Focus
- Lost/missing release, moved/foreign input and render/provider change must not retain a permit.
- GUI callbacks perform no SDK/clock/codec IO; worker samples native state with checked lock.
- Failed/reentrant cleanup never frees original input/widget/session backing objects.
- Replays/overflow/old tickets cannot revive rights or cancel a successor.
- Physical capability is separate from held input/permission; production Core cannot capture through this change.

### Task1: Native hold source and retained owner integration
Files: new muse_voice_hold_input.hpp; consent/UI/runtime/codec interfaces; fake input boundary and actual production-owner fixture; docs.
- [ ] RED real pointer press and current-state queries gate native Hold; reject synthetic click/data, foreign/released/lost/moved/stale/lock failure; original permissions remain separate.
- [ ] Implement private native hold ticket and source wiring, default-deny physical prerequisite and bounded runtime capture/release ordering.
- [ ] GREEN production UI/runtime with inert codec, exact generation, repeat holds, release/error/revoke/close and concurrency; native Core unavailable.
- [ ] Full host/profile/native/real-LVGL checks, fresh exact-source review, draft PR stacked correctly without waiting on CI.
