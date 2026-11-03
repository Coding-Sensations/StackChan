# Muse Device Voice Setup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL superpowers:executing-plans; sole native writer and fresh whole-branch independent review.

**Goal:** Issue a bounded original admission scope from actual on-device permission gestures and wire it to native prepared startup.
**Architecture:** Dedicated private UI/strong revocable context, default-deny LocalPort extensions, native metadata/scoped stop APIs and noninitializing static codec publication.
**Tech Stack:** Existing C++17/LVGL/Muse SDK/ESP-IDF5.5.4, ASan/UBSan host, cached real LVGL and native compilation.
**Spec:** ../specs/2026-11-03-muse-voice-setup.md

## Global Constraints
No live/device/real secrets/permissions/security/merge. Source-only. Three independent admission rights; mic/output/hold/flush remain separate/unavailable. All contexts survive worker cleanup without UI pointers. Preserve human main and existing features/limits.

## Review Focus
Programmatic/stale/foreign/lost input cannot prepare. Scope cancellation never stops a successor. UI deletion/reentry cannot free executing callbacks. Worker callbacks are thread-safe and reference no UI. No hidden audio initialization or token load from metadata/preparation. Profile closures/boundary hashes remain strict.

### Task1: Native setup issuer and integration
Files: voice setup UI header, LocalPort extensions/native implementation, local setup entry, scoped task stop/identity/clock, codec access publication, profile closure/tests/docs.
- [ ] RED missing setup: actual input must gate three rights/prepare, original context retained/revoked; no metadata lookup initializes codec.
- [ ] Implement bounded reviewed scope/private UI, native pointer fences, native port/metadata/Core lookup and matched stop.
- [ ] GREEN all masks/input faults, clock/identity/network/provider change, close/edit/reentry/expiry/successor and context lifetime; preserve Pair/Chat.
- [ ] Full host/profile/native/real LVGL qualification, current acceptance matrix, exact independent review and draft PR; no CI waiting or activation.
