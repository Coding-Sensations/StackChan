# Checked cold output-only implementation plan

> For agentic workers: use superpowers:executing-plans, sole inline writer, with fresh whole-branch review.

**Goal:** Source-qualify microphone-inactive cold TX/amplifier initialization and exact never-created input evidence.
**Architecture:** Program-lifetime dormant cold Core; single-use original-lease initializer; typed absence and checked disabled-TX evidence consumed by existing retirement. No production caller or image.
**Tech Stack:** C++17/ESP-IDF5.5.4, pinned esp_codec_dev1.5.4, ASan/UBSan production-body fixture.
**Spec:** docs/superpowers/specs/2026-11-03-muse-cold-output.md

## Global Constraints

No RX/ES7210 configuration, PCM or channel enable on cold instance. Preserve ordinary initialization and all five providers/Stock. No uncertain retry, fake stop/close, device operation, credentials or diagnostic image. Keep original lease/resources retained on every partial failure.

## Review Focus

Wrong/stale owner before and during calls; retained singleton/lease lifetime; factory returns null or partial resource despite status; hostile/reentrant entry while initializing/stopping; no legacy/USB/PCM escape after retirement/common release.

### Task1: Cold boundary and retirement composition

Files: Core header/implementation, cold-native implementation, reporter; new cold production fixture/test and runner; CMake test registration; companion docs.
- [ ] Add failing API/behavior tests: no-IO acquisition, TX-only configuration, exact source absence, each SDK/factory/readback fault, lease loss at every call, reentry, one attempt, stale/foreign lease, forbidden entrypoints and successful quiesce/release without fake input flags.
- [ ] Run RED and preserve failure.
- [ ] Implement minimal checked cold API and native TX/AW-only initializer, distinct closure proof, retention and entry guards.
- [ ] Focused GREEN then full host/profile/native checks; document precise source limits and no image.
- [ ] Fresh independent whole-branch review; fix important findings with fault regression.
- [ ] Verify exact reviewed source, commit and publish draft stacked on PR38; observe normal CI admission once without waiting.
