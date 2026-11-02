# Exact Reviewed Native Voice Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans, sole native writer, final fresh read-only review.

**Goal:** Bind actual direction Owner to exact returned review and explicit current native context without minting authority.
**Architecture:** Existing Bridge exact-instance checks; native Permit context-matchdefaultfalse; single-use reviewed validator and retained Owner composition.
**Tech Stack:** C++17, existing SDK/Protocol/Bridge/Clock/Core fixtures, ASan/UBSan and ESP-IDF5.5.4.
**Spec:** ../specs/2026-11-02-muse-reviewed-native-permit.md

## Global Constraints
No device/live/credential/security/merge or cross-owner writes; preserve physical/native unavailable defaults and all existing behavior outside additive composition.

## Review Focus
Different protocol/clock instances cannot cross-bind identical IDs. Native callback revocation/time drift cannot authorize next IO. Every target field/direction/expiry must match, not just generation. Missing native context implementation must stay unavailable after review. Stop/retained cleanup must not rely on current native permission.

### Task1: Exact review/authority/Owner composition
Files: review_bridge.hpp pure identity checks; muse_voice_codec_owner.hpp native match default; new muse_voice_review_permit.hpp; actual Core/Bridge test+runner/CMake; documentation.
- [ ] Write/run RED: completed review and matching synthetic native context compose actual Owner/Core; boolean-only/unavailable authority rejects before resource/codec; masks and wrong protocol/clock instances refuse.
- [ ] Implement single-use BindReview, exact target/revision/directions/current clock validation around native matches and permission calls, safe reentry/revocation/wipe and retained Owner forwarding.
- [ ] GREEN all target fields, source budget/provider/review replacement, mono/wall expiry/rollback, native callout revoke/clock changes, held ticket/current time, no authority from localreceipt, original cleanup afterrevoke.
- [ ] Full applicable host suite+3target configs, immutable dependency/source/preservation evidence, independent exact-head review, publish stack-correct PR; no CI waiting/live actions.
