# Serialized Muse Runtime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL superpowers:executing-plans, sole native writer and fresh final independent review.

**Goal:** Join actual admitted session and device review/consent under one bounded serialized owner.
**Architecture:** Stable heap owner transfers existing Admission; inline native consent, retained Session and inline widget. Explicit Open/Tick/Show/Play/End/Close, no task or route registration.
**Tech Stack:** Existing C++17 ESP-IDF5.5.4/LVGL/SDK/Core with ASan/UBSan host and native checks.
**Spec:** ../specs/2026-11-02-muse-runtime.md

## Global Constraints
No device/live/real credentials/security/merge. No automatic mic/hold/physicalflush. Reuse original exact scopes/objects; ports and callback contexts outlive complete cleanup; serialized LVGL owner lock. Preserve all existing behavior outside additive source.

## Review Focus
Close/end reentry at credential/SDK/clock/UI callouts must not free executing state. Failed cleanup keeps original authority and transport. No fresh target or receipt-only authority. UI intent drains before more transport/audio operations. Parent/widget callbacks must finish before borrowed ports or session retire. Startup failures retain cleanup, not another admission attempt.

### Task1: Runtime composition
Files: new muse_voice_device_runtime.hpp/native.hpp, actual SDK/Core/widget runtime fixture+runner/CMake, docs.
- [ ] RED missing owner: quiet construction, exact admission transfer and live returned-target review; native user approval binds without audio IO; Close preserves failed original join.
- [ ] Implement stable single-use owner, bounded transport/UI/intent pumping and explicit playback; reentry latches and retained cleanup; native constructor composition only.
- [ ] GREEN malformed/mismatched scope, denied RAM/provider/endpoint, clocks, all reentrant seams, End/Close/Return/Show, cancellation/no replay and failed audio/socket/UI cleanup.
- [ ] Full applicable host/profile/target/native object and real LVGL checks, exact independent review, immutable evidence and correctly stacked PR; no CI wait/device.
