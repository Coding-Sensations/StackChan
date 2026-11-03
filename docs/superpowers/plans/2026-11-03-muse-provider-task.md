# Native Muse Provider Task Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL superpowers:executing-plans; sole writer, fresh final independent review.

**Goal:** Wire explicit prepared voice startup to a retained native task and provider UI lifecycle.
**Architecture:** Portable retained worker/checked suspended-retirement primitive; native Window owns original scope/custody/Core borrow, heap Work owns runtime/surface; independent reaper drains private worker and API borrows. Settings dispatch intents only.
**Tech Stack:** Existing SDK/Core/queued LVGL runtime, C++17/FreeRTOS/ESP-IDF5.5.4, ASan/UBSan host and native checks.
**Spec:** ../specs/2026-11-03-muse-provider-task.md

## Global Constraints
No live/device/real credentials/security/merge. No invented admission grants, actor attestation, hardware hold/flush or join receipts. All original contexts/Core outlive cleanup. Large frames on heap; no task on tile entry; profile exclusions remain.

## Review Focus
Task can run before Create returns. done alone cannot free task context. Provider parent may disappear during SDK IO; private surface must survive. Scope callbacks must not run under service mutex. Failed creation/join/UI lock retains original resources; no Start replay. Reaper publication cannot precede original scope/custody release. Cross-thread APIs signal only.

### Task1: Retained task primitive and native provider worker
Files: new muse_voice_task_owner.hpp, muse_voice_task.h/.cc, provider/device lifecycle hooks, actual SDK/task fixture+runner/CMake/docs.
- [ ] RED absent task owner: actual SDK/runtime runs to review; failed cleanup retains before done; done-but-running refuses retirement.
- [ ] Implement bounded task factory/retained cleanup/private suspension retirement and native Window/Work/reaper with exact original scope.
- [ ] GREEN stop/reentry/create failure, scope revocation, failed UI/audio/socket cleanup, done/suspended distinction, API drain and no auto SDK retries.
- [ ] Wire explicit prepared-start/show provider intents and exit revocation; source-derived provider/native checks and profile exclusions.
- [ ] Full host/profile/native/real LVGL where applicable, exact independent review, immutable receipts and stack-correct PR without CI waiting.
