# StackChan fork comparison and first source wave

Date: 2026-10-25. Maintained baseline: mrkillbob/StackChan main
`77c629cac129cc79c80eeab5c011cb37d30eaa89`. This branch is independent of draft
PR #15 (`de30819eed4b8bcf20dbbebf91ace36b1d451a00`) and the frozen firmware.

## Existing maintained architecture

| Area | Already present | Remaining gap |
|---|---|---|
| Firmware/UI | ESP-IDF CoreS3 board integration; stock apps, LVGL/Mooncake, servo motion, OTA cancellation and joined navigation cleanup | Physical display/navigation/Wi-Fi/servo acceptance remains separate. The reviewed layout/remembered-runtime composition is a frozen candidate, not this source branch. |
| Pairing/trust | Numeric pairing, pinned TLS, remembered-device registry, session/generation/epoch correlation; microphone and camera purpose checks are distinct | Live saved-Mac reconnect and attended transport test remain unverified. |
| Audio | Bounded Opus transport, native CoreAudio adapter, directional HAL source, current-turn and physical-Listen checks, checked child lifecycle and retained cleanup | Mac endpoint routing and real audio-loop proof remain open. No supported automatic attachment to this exact assistant conversation exists. Manual Mac voice routing is the proposed path. |
| Camera | Bound producer methods, source DVP completion timestamps, permission ledger, checked stop receipts; WCV1 frame validation, native CMIO sink/extension source | `CaptureOriginQualified()` stays false; `PairingController` constructs its pipeline with owner:nil and extensionApproved:false. The worker reports cmio_delivery_bridge_unqualified. A real producer pump, owned reader, app callbacks, and qualified clock/stop bridge still need integration. |
| PR #15 | Producer timestamp preservation, bounded pipe delivery/framing, joined local cleanup, one-shot view-open coordinator | Components are not wired into the running app; local receipts do not establish physical shutdown. |
| Local vision | Scoped local SDK-backed observations, bounded latest-frame processing, gesture policy and apparent-size selection | Not an identity-recognition product; physical camera/CMIO qualification remains required. |
| CI | PR #14 deduplication is merged into main | This local adoption branch is unpublished; no run was dispatched. |

The selected AFE source already locks the wake PCM deque and swaps a snapshot
under the same mutex into a checked encoder with lifecycle/join ownership.
The analogous Brett mutex patch must not overwrite these stronger changes.

## Pinned comparisons and disposition

| Repository and pinned revision | Useful evidence | Adoption decision / license |
|---|---|---|
| [circlemouth/Hermes-StackChan](https://github.com/circlemouth/Hermes-StackChan/tree/018666f30fafbd66e59b6a245db7b6bd16dcc1f8) | Retired generation, late cleanup, cancellation/reaping, human onset preservation, explicit listening vs cooldown regression scenarios | Independently derive missing behavior tests only. Its new ai-server has no verified applicable root/package license; firmware MIT is not a license for the bridge. No code/tests copied. Do not inherit unauthenticated ingress, SanoTTS dependencies, or ordinary-session timeouts. Existing current-session/retained-cleanup tests already cover many scenarios. |
| [chunhuan-lu/StackChan](https://github.com/chunhuan-lu/StackChan/tree/43f9282119f8e11532ed5ebc5ba47c65d06fc8e5) | CRC USB framing, ACK/request IDs and reconnect/state-resync patterns; opt-in agent status | Our provisioning/serial ownership and audio path must stay authoritative. No second serial owner/status integration added. Firmware MIT does not establish permission for tools outside firmware. The mic pump has shutdown/failure concerns and is not a drop-in replacement; no code copied. |
| [hiroshi-y/StackChan](https://github.com/hiroshi-y/StackChan/tree/37b2cfc04ab869a4e475762daf1339f4a9360920) | Small dependency-free gaze helper and host math tests | The implementation, header, tests and MIT license are byte-identical to ours and canonical M5Stack. No duplicate import. Unique radio/CAT/light-sleep changes do not solve our current integration gaps. Extend our existing math with independently authored edge cases instead. |
| [BrettKinny/StackChan](https://github.com/BrettKinny/StackChan/tree/2b5f79cc6914a2f8069031c1030f76c24f981a44) | Camera arbitration, peripheral-linked indicator concepts, wake PCM deque race fix | Examined privacy files carry explicit MIT SPDX headers (Brett Kinny/squarewavesystems or M5Stack). Retain notices for any future copying; no blanket repository license inferred. Do not import guards that clear state after failed stop, LED-only mutex fallback, face-presence-triggered wake, or stub recognition. No code copied. |

None of the four inspected forks provides our missing native Mac CMIO bridge.
TinyVoice (`7000fc3fdca0fa3ffea78311348d3f55d586fbd5`) is deferred: GPL-3.0-only,
Rust 1.91, and no firmware drop-in. No code/dependency added. Wake/text filtering
must never mint capture/action consent or silently discard genuine speech.

## Canonical provenance and implemented change

Canonical source: [m5stack/StackChan at eff0d6b5109663faec8f1d98848b4b0e5246bbfc](https://github.com/m5stack/StackChan/tree/eff0d6b5109663faec8f1d98848b4b0e5246bbfc).
Verified Git blobs shared by canonical, hiroshi-y and our baseline:

- `firmware/LICENSE`: `84d8b86919d765e833ebbdc49f75e514ace1fd46` (MIT).
- `motion_math.cpp`: `f384371358f38fc36b9938417101b1450a0996c2`.
- `motion_math.h`: `9a0642e7b9bbc1d750451a998a757b746899cf23`.
- `motion_math_test.cpp`: `0aa12107de1579071a880db2189af2bf04a472e9`.

Existing copyright/SPDX notices and `firmware/LICENSE` are retained. New tests
and hardening are independently authored, not copied from unlicensed bridge
code. Comparison suggested nonfinite/degenerate/range/orientation regressions;
these exposed two concrete defects in our existing implementation:

1. A finite target `(FLT_MAX, FLT_MAX, FLT_MAX)` yielded pitch 0 instead of 352
   tenths of a degree because float squaring overflowed.
2. NaN mapping reached an undefined float-to-int conversion, confirmed by UBSan.

The fix uses double interpolation and hypot, rejects nonfinite/zero-direction
inputs and reversed limits, and marks invalid angle pairs explicitly. Both real
`Motion::lookAt*` methods return before issuing any servo target for invalid
results. Equal limits remain valid fixed axes. Valid finite input clamping and
documented point orientation are preserved. No timer, permission, IO owner,
capture policy, actuator driver or deployment configuration was added.

## Verification

Red: original large-coordinate test reported `expected 352, got 0`; the original
NaN probe triggered `nan is outside the range of representable values of int`.

Green: both the expanded existing math suite and the actual Motion-controller
suite pass using `clang++ -std=c++17 -DNDEBUG` with
`-fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all`.
The controller suite replaces only the servo/animation IO boundary. It checks
that invalid values preserve the previous target and valid values reach both
servos. Removing either validity guard was detected by that suite. Test checks
remain active under NDEBUG. Both targets are registered in firmware/tests CMake.

Independent source review found no blocker and reran both focused sanitizer
suites. Full ESP-IDF firmware, full host suite, device motion, power, microphone,
camera, DMA and live audio were not exercised by this source-only change. No
installer/script from a fetched fork was executed; no dependencies installed.

This source wave is local only. It does not resolve the capture/voice/bridge
gaps above, publish a branch/PR, merge anything or establish device acceptance.
