# Stopped Muse DMA Discard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans, sole native writer with final fresh read-only review.

**Goal:** Add checked stopped-DMA zeroing prerequisite, not a physical flush/consent claim.
**Architecture:** Existing Core lease/stop custody plus pinned ESP-IDF get_info/preload API; no new driver/backend.
**Tech Stack:** ESP-IDF5.5.4/C++17, actual Core production extraction with inert driver ports, ASan/UBSan.
**Spec:** ../specs/2026-11-02-muse-dma-discard.md

## Global Constraints
No device/live/credentials/security/merge; preserve frozen PR26 and all other providers/profiles/layout/trust. Digital discard never upgrades physical flush or permission.

## Review Focus
Wrong/stale lease must not touch a successor. Partial/failed driver calls cannot return success/replay a ticket. Paired TX means require both stopped/closed. Reentrant enable/release must not invalidate quiescence. Geometry/byte-count faults must stop bounded work.

### Task1: Core primitive and tests
Files: cores3_audio_codec.h/.cc; shared Core fixture; muse_dma_discard_test.cpp and Python runner, CMake registration.
- [ ] Write/run RED: disabled stopped originallease clears all5760dirtyDMAbytes in23calls≤256bytes; unavailable before stop; ticket replay no IO; never legacy authority/flush.
- [ ] Add enum MuseDiscard{Unavailable,Failed,StoppedDmaCleared}, MuseDiscardOutputChecked(lease,ticket), max16384bytes and busy/monotonic ticket fields, original-scope checks around get_info/preload. Reject reentrant enable/release; no IO retry.
- [ ] GREEN faults: foreign/stale/revoked-held scope; sizes0/misaligned/oversize; enabled/wrongmode/dir; partial/error first/middle/final; final geometry drift; reentrant attempts; new binding ticket reset; partial known stop retained; max-bound calls.
- [ ] Qualify affected sanitized fixtures, full eligible host suite,3real target configurations, source delta/preservation and independent final exact-head review. Publish correctly stacked PR after checking user changes; no CI waiting/device.
