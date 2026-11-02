# Muse UI Owner Handoff Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL superpowers:executing-plans; sole native writer, final fresh-context independent review.

**Goal:** Allow the native protocol owner to keep SDK IO outside the LVGL lock with queued validated native gestures and immediate revocation.
**Architecture:** Optional queued widget dispatch; atomic signal-only revoke; explicit UI access port around actual widget work; native HAL lock composition and owner-side draining before transport.
**Tech Stack:** Existing C++17/ESP-IDF5.5.4/LVGL/SDK/Core, ASan/UBSan and qualified cached real LVGL.
**Spec:** ../specs/2026-11-02-muse-ui-owner-handoff.md

## Global Constraints
No live/device/real secrets/security/merge. Single protocol owner; UI callbacks queue only under LVGL lock. Caller retains borrowed ports/parent/callbacks through cleanup. No fabricated native hold/flush/observation or task registration.

## Review Focus
Close/End during SDK IO must revoke before another operation without mutating owner state on GUI thread. Stale queued approvals never survive render/target/provider changes. Failed UI lock retains original session. Callback/model lifetime must remain valid through deletion. No socket or physical IO under GUI lock.

### Task1: Queued dispatch and native runtime UI port
Files: consent.hpp atomic barrier, UI.hpp bounded deferred events, runtime.hpp UI scopes, native.hpp HAL port, actual fixtures/tests/CMake/docs.
- [ ] RED callback touches actual SDK/Clock before owner drain, or IO under GUI lock; queued native Allow must defer permission.
- [ ] Implement signal-only revocation and bounded private input queue with owner freshness checks; real native HAL access and SDK-outside-lock runtime flow.
- [ ] GREEN stale/replayed/foreign/lost/aged/overflow events, mid-IO revoke, End cancellation, failed UI lock/cleanup and cross-thread signal; preserve inline tests.
- [ ] Full host/profile/native and cached real LVGL qualification; exact final independent review and stacked PR, no CI waiting.
