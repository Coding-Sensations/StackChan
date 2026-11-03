# Local cold diagnostic harness implementation plan

> For agentic workers: use superpowers:executing-plans; sole inline writer and final fresh independent review.

**Goal:** Produce an explicitly opt-in no-microphone diagnostic source and offline image, never a live run.
**Architecture:** Separate guarded startup; native Arm/Run grant; one synchronous retained original-lease owner using checked cold Core; immutable report. No storage, provider startup or automatic invocation.
**Tech Stack:** ESP-IDF5.5.4/esp_codec_dev1.5.4, C++17, LVGL9, inert production-body tests and cached native toolchain.
**Spec:** docs/superpowers/specs/2026-11-03-muse-local-diagnostic.md

## Global Constraints

No device/serial/reset/flash, microphone/recording, PCM/network/credentials/live session/motion/security changes or merge. Production physical flush/PTT stay false. No image until no-microphone harness is reviewed. Preserve normal initialization and frozen artifacts.

## Review Focus

Forged/stale/foreign native event; cancellation/view lifetime; original lease loss across every call; partial cleanup and program-lifetime retention; diagnostic startup/settings closure and ordinary build preservation.

### Task1: Source harness, native startup and tests
- [ ] Write failing native-input/owner fault tests and option/settings/startup closure tests.
- [ ] Implement opaque local opt-in grant, registered-input UI, one-shot owner and dedicated guarded boot/settings/HAL/board path.
- [ ] Run focused/full host/profile/native tests, preserve RED evidence and fix actual failures.
- [ ] Fresh independent whole-branch review and exact source qualification; commit.

### Task2: Offline artifact and publication
- [ ] Check resource custody, reuse pinned cached public dependencies/SDK; one isolated offline build with no download.
- [ ] Verify exact source/config/image/size/slot/dependency/startup/preservation; independent artifact review.
- [ ] Publish draft stacked PR, verify exact remote head/normal CI admission without waiting, report exact future attended scope and open hardware gates.
