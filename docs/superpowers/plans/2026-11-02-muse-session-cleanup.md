# Retained Muse Session Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL superpowers:executing-plans, sole native writer, fresh final independent review.

**Goal:** Enforce real audio/session/UI retirement order using existing source owners.
**Architecture:** Transfer exact unused admission; compose reviewed direction Owner; bounded move-only review borrows retain admission until drained. No authority issuer or autoactivation.
**Tech Stack:** C++17/ESP-IDF5.5.4, actual SDK/protocol/admission/Core fixtures with synthetic ports, ASan/UBSan.
**Spec:** ../specs/2026-11-02-muse-session-cleanup.md

## Global Constraints
No device/runtime/realcredentials/security/merge; keep native issuer and physicalflush unavailable. Preserve Stock/Mira/USB/profile/config/trust/assets/layout.

## Review Focus
No SDK retirement before failed audio cleanup. Reentrant close/end must invalidate scopes before any next IO. UI references must remain alive until genuine borrow release. Wrong/used Protocol/Clock/admission cannot transfer. End sends use original remaining scope; uncertain sends never retry, pending audio does not close a healthy session.

### Task1: Actual session owner, native constructor and tests
Files: new muse_voice_session_owner.hpp/native.hpp; admission pure identity/unused properties; reviewed direction Revoke/Status; shared admission fixture extraction/new integrated test+runner/CMake; docs.
- [ ] RED actual admission+SDK+Core: transfer, two-phase admission/review, native default refuses, checked audio failure blocks socket close, socket join failure retains, UI borrow blocks final destruction; no token/endpoint use.
- [ ] Implement single-use serialized Open/Tick/direction operations/End/Close, wrapped original scope callbacks, pure revocation on reentry, bounded RAII UI borrows, explicit destructor/lifetime contract.
- [ ] GREEN failed/reentrant Open/Tick/end/close, no replay/reconnect, wrong identities/used transfer, scope expiry/provider exit, audio/transport/UI order, clock loss during cleanup, validated remote end reply only.
- [ ] Full host suite+3native target configs, source/provenance/preservation evidence, fresh independent exacthead review and correctly stacked PR. No CI waiting/live actions.
