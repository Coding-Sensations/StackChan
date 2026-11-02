# Native Device Voice Consent Plan

> **For agentic workers:** REQUIRED SUB-SKILL superpowers:executing-plans; sole native writer, fresh final independent review.

**Goal:** Real local native consent source and borrowed review widget, without runtime activation/attestation claims.
**Architecture:** Existing admitted SDK projection + original RAM credential/device binding; private native UI approval; serialized retained review borrowing and deferred close.
**Tech Stack:** C++17/ESP-IDF5.5.4/LVGL, existing actual SDK/Core fixtures and inert input ports, ASan/UBSan.
**Spec:** ../specs/2026-11-02-muse-device-consent.md

## Global Constraints
No device/live/realcredential/security/merge. No remote attestation or native Hold/physicalflush claim. Preserve existing working providers/Pair/Chat/config/layout/trust.

## Review Focus
Receipt alone must not issue permission. Different reader/VM/context or credential epoch must revoke. Synthetic/foreign/stale input must not issue a gesture. Scope callback reentry cannot retire executing state. Widget callbacks must drain before deletion and borrow release.

### Task1: Actual context/credential/native grant and widget
Files: worker/admission readonly projection; credential lease noncopy; new device consent/UI/native headers; actual tests+runner/CMake; docs.
- [x] Write/run RED on exact SDK context projection, missing native widget/grant, and reader noncopy trait.
- [x] Implement verified projection, independently bound single-use local authority and native M5 input adapter; private UI approval only; no issuer from receipt-only API.
- [x] Implement VM/target page review and separate choices; native gesture fencing, deferred UI close, intent-only prepare/end; no Hold/physicalflush shortcut.
- [x] GREEN source/credential/target/clock/provider/endpoint changes; callback revocation; all masks; programmatic/lost/stale inputs; quiet construction; widget/borrow cleanup; RAM/nonRAM defaults.
- [ ] Full applicable host+native checks, preservation/provenance evidence, fresh exacthead review and stacked PR; no CI waiting/live actions.
