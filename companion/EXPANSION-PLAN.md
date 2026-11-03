# Michael’s Stack-chan expansion and integration plan

Updated 8 October 2026. **Later implementation update:** custom boot/consent UI now compiles; see BOOT-IMPLEMENTATION.md for the current image and evidence. Earlier image/test entries below remain historical evidence. This is the plan for the existing project, with implementation and acceptance status recorded separately.

## Goal

Stack-chan becomes a portable physical companion for Michael’s existing Mira conversation, voice and memory. Mira is the default coordination hub and can use explicitly authorized agents and tools. Michael can also select Claude, Codex or Hermes directly. All routes use the same authorized home/device layer, while keeping each provider’s conversation identity and permissions explicit.

Keep cloud reasoning off the robot where practical. The robot handles its microphones, speaker, display, touch, camera and movement. It should work wherever there is a verified internet route. Production must connect directly over authenticated WSS to an independently reachable remote companion service and work with Michael’s Mac switched off. The Mac is for development/flashing or an optional paired diagnostic host. A nearby trusted computer can provide an optional companion service after pairing and capability discovery; plugging into USB for power does not grant access to that computer.

## Existing project and evidence

- Folder: `$STACKCHAN_ROOT`, outside synced Documents/iCloud.
- Fork: https://github.com/mrkillbob/StackChan. GitHub identity was authenticated and verified as `mrkillbob`.
- Official source: [m5stack/StackChan](https://github.com/m5stack/StackChan), pinned baseline `1b5765599fba8aaad1811d9a79358ccc7051f5f3`; source declares version 1.5.1. Michael reports installed firmware 1.5.1 and setup through StackChan World. A source build is not proven byte-identical to his installed image.
- Local branch: `companion-audio-prototype`. Native audio foundation: `38d070e02da090879fe337067816c366614b9bc6`. Authenticated streaming and firmware components: `497357c3c40992c953f8216142996f626b9a4e50`. These commits have not been pushed, and no GitHub Actions runs were started.
- Toolchain: ESP-IDF 5.5.4 at `735507283d5b2f9fb363a1901172dbd9e847945d`, Python 3.13.15, Xtensa GCC 14.2.0 (`esp14.2.0_20260121`), CMake 3.30.2, Ninja 1.12.1 and esptool 4.12.0. Host native compilation used Apple Clang 21.0.0.

| Area | Evidence | Acceptance still needed |
| --- | --- | --- |
| Firmware | New compile passed in 30.37 seconds, one worker, network denied; dependency lock unchanged | Physical boot and actual companion activation |
| New image | 3,789,424 bytes; SHA-256 `1b2c3189e71aa4923b7181fc6689475c7eeee2f5fcca572b607c49af2b77a29f`; valid ESP32-S3 image checksum | No flashing has occurred |
| Native audio | Three native test executables passed, including concurrent ring and callback-policy tests; worker library load passed | Physical CoreAudio routing, permissions, isolation and latency |
| Authenticated streaming | Nine tests passed; finite localhost fixture passed with actual Opus/resampling, four generated uplink and three generated return packets | Production TLS, real pairing, robot microphone/speaker and native Mira voice |
| Earlier offline work | 35 bridge tests, eight codec/resampler tests and ten synthetic worker tests passed | These are retained evidence, not new live-device tests |
| Expressions | Seven approved 320×240, 24 fps MP4 states verified | Display playback and state synchronization |
| SD card | 4 GB card formatted FAT32/MBR as `STACK-CHAN`, write/read verified, safely ejected; Michael reports it is now inserted | Firmware mounting and assets remain unqualified |

Receipts live in `companion/host/native-audio/VERIFICATION.json`, `companion/host/streaming/VERIFICATION.json`, `companion/host/streaming/FIRMWARE-VERIFICATION.json` and `notes/SD-CARD-FORMAT-RECEIPT.json`. Successful compilation and synthetic tests do not establish live robot operation. Earlier measured endpoint processing around 1.44 ms excludes capture accumulation, OS, network, provider and physical audio delays.

## 1. Finish audio and the custom robot experience

Implement the custom boot path, touch consent, visible listening/speaking/muted states and companion lifecycle. A custom boot/UI entry point now compiles. Its trusted grant provisioning, physical audio and recovery acceptance remain open; flashing alone would not deliver the requested experience.

Make the direct remote WSS gateway the production audio/session path. The firmware accepts an authorized endpoint URI and does not require a Mac-specific audio transport, but deployment, real grant issuance/revocation, production TLS and provider/session routing remain unimplemented or unqualified. No remote endpoint is activated.

The [native voice documentation](https://learn.chatgpt.com/docs/features/voice) describes conversation-linked voice in supported clients. The inspected public documentation and callable tools do not establish an external raw-audio attach/export endpoint for Michael’s existing Mira conversation. [MCP Events](https://developers.openai.com/plugins/build/mcp-events) supports discrete events, explicitly without streaming; it is not evidence of a bidirectional voice transport. This is a remaining integration blocker, not proof that every possible future route is unavailable.

The [GPT-Live API](https://developers.openai.com/api/docs/guides/live) supports remote full-duplex audio and delegation to an existing backend, using a trusted server API credential and separately billed API sessions. It does not document attaching that API voice session to an existing native Mira voice call, or automatically sharing ChatGPT conversation memory. It is technically a candidate for a new cloud voice frontend with an explicit context bridge, not a verified fulfillment of the existing-Mira requirement. Do not create credentials, activate billable sessions, clone Mira’s voice or silently substitute a new agent.

Keep the native Mac CoreAudio bridge as an optional, separately approved diagnostic prototype. Its allowlisted Pro Tools Audio Bridge 2-A/2-B routes use mono 48 kHz PCM on channel 1; direction, isolation and native app routing still need qualification. Even if qualified, this path depends on the running Mac/client and therefore cannot pass production portability acceptance. It is not a mandatory prerequisite for the remote implementation.

The implementation already includes explicit consent and acknowledgement, generations/epochs, sequence checks, bounded queues, half-duplex gating and a two-second heartbeat deadline. WebSocket handshake validation now has compiled regression coverage; qualify actual TLS, connection ownership and shutdown before activation. Current interruption invalidates queued output; it does not prove physical speaker silence or provider cancellation. Spoken barge-in remains a later requirement.

Acceptance: start from mute; authorize a real conversation; verify Mira’s identity and intended conversation continuity; record intelligible bidirectional speech and measured end-to-end latency; test disconnect, revocation, restart and stale packets; verify mute and physical stop. For optional Mac diagnostics, confirm unrelated Mac audio is not captured. All physical audio tests require their specific approved scope.

## 2. Make the companion portable

Use revocable pairing and explicit microphone/speaker capabilities. A robot discovers only capabilities its paired host offers. Separate USB power, local network presence and host authorization. Production audio uses authenticated WSS; the localhost WS fixture is test evidence only.

Reuse existing Tailscale or cloud routes only after verifying their availability, TLS trust, access scope and intended endpoint. No new paid service, credential, account permission or remote exposure is implied. Network loss must return the robot to a clear muted state and recover without replaying old speech or actions.

Acceptance: place the robot on a different internet connection with Michael’s Mac switched off, verify the intended Mira voice/session and memory continuity through the remote service, then test reconnection and optional host switching; revoke the device grant; demonstrate rejection of stale/replayed traffic and unpaired hosts. Record where audio is processed and what remains available offline.

## 3. Connect Mira, Claude, Codex and Hermes

Reuse HomeLab’s `DeviceRegistry`, sessions/events, turn generations, request replay, interruption, approval and action deduplication. Current inspected HomeLab revision is `f8f47b8e26cee976ff208722f7c6e667fe19a4aa`, using its existing Python 3.14.7 runtime. No HomeLab files were changed by this work.

HomeLab contains Codex AppServer, Claude Code CLI and Hermes ACP adapter code. Their existence does not establish a live robot connection or account entitlement. Claude Code and Hermes live routing remain unqualified; Hermes is disabled until explicitly verified. Use the [official Claude Code interface](https://code.claude.com/docs/en/cli-reference) and [official Hermes project](https://github.com/NousResearch/hermes-agent) for supported integrations.

Mira stays the default coordinator. Direct agent selection must identify the chosen agent and its separate session; it must not imply shared app memories automatically. No silent provider fallback. The existing Apple on-device STT/local TTS path is useful infrastructure but does not satisfy the same-Mira-voice goal.

Acceptance: one real permitted request on each enabled route, correct session continuity, clear route display, no repeated side effect after retry/interruption, and truthful reporting when a route is unavailable.

## 4. Add home controls through supported interfaces

Expose a small set of authorized home actions through the shared device layer, with readable results, approval where required and deduplication. Preserve independent normal home automation. Integrate Siri, Apple Home and HOOBS through supported interfaces after verifying the existing installation and permissions.

Current inspected Apple App Intents only open HomeLab Companion or mute/unmute HomeLab audio. Existing HOOBS notes are planning evidence, not working light controls. Do not represent either as completed home integration.

Acceptance: begin with read-only device state, then one explicitly authorized reversible action; retry and disconnect without duplication; verify that ordinary Apple Home/HOOBS operation continues independently.

## 5. Add vision, expressions and movement

Start with explicit camera capture to a named destination and visible camera state. Default to no retained camera/audio recordings. Synchronize the approved Mira expressions with actual conversation state, then qualify motion separately.

The [official StackChan hardware guide](https://docs.m5stack.com/en/StackChan) identifies the original CoreS3-based family, camera, microphones, speaker, IR and NFC. Exact physical display revision remains unconfirmed; the inspected board driver contains ILI9342C/E selection logic. Verify that selection on the actual unit. Verify servo coordinate conversion, feedback and calibrated origin before applying physical travel limits.

Acceptance: one intended image reaches the authorized destination; no capture while disabled; expressions match state; motion stops reliably and stays within verified limits without cable strain.

## 6. Expand hardware deliberately

Verify the exact schematic, voltage ratings, pin ownership and power budget for every attachment. I2C, GPIO, UART and USB OTG are candidates; a 5 V connector supply does not establish 5 V GPIO tolerance. The official body mapping is servo TX/RX G6/G7, IR send/receive G5/G10 and body I2C SCL/SDA G11/G12. Grove ports have 5 V/GND and signal pairs A G2/G1, B G9/G8 and C G17/G18. Shared buses and addresses require ownership and collision checks.

Use built-in IR and NFC first. Review the official Flipper source, licensing, protocol implementation and hardware assumptions before copying or porting anything. Sub-GHz, 125 kHz RFID, iButton and an extra MCU are optional future expansions requiring separate suitability and authorization decisions. This plan makes no purchase or feature-parity promise and includes no access-control bypass.

Acceptance: qualify one attachment at a time, prove its electrical compatibility and bounded behavior, and leave audio functional under load.

## SD storage and debugging

Use the prepared card for versioned assets and bounded, redacted diagnostic logs. Verify SD/display SPI ownership before adding a mount driver; shared CoreS3 pin mappings require care, and no working SD mount was found in the inspected current HAL. Never autoformat the card on failure. Validate asset manifests and fall back to a basic built-in display state.

Debug in software first: structured state transitions, generation/sequence counters, queue overruns, heartbeat age, task lifetime and audio timing. Keep secrets and raw media out of routine logs. Michael’s existing Tek scope and probes can later help inspect power, clocks, buses, IR and servo signaling once their exact model, ratings, grounding and connection points are verified. No instrument purchase or probing has occurred.

## Recovery and release acceptance

Preserve the pinned source, dependency lock, build receipts and baseline artifacts. The saved source baseline image is 3,786,944 bytes, SHA-256 `7999234550fa487537fa83892881170626e6a058702a0f0ef30084bab2cc81f5`; it is not a backup of the shipped firmware, NVS, Wi-Fi, pairing or calibration.

The robot’s partition table was read without reading NVS and matches the build table. Download-mode ROM checks confirm ESP32-S3 revision 0.2, 16 MB flash, secure boot disabled and flash encryption disabled. No flash writes, erasure, reset or downloaded stub were performed. Michael explicitly declined a full installed-state backup and encrypted-container setup; that workflow is stopped and is not a prerequisite. Prefer a minimal app/OTA-selection write preserving NVS and device-specific data, with official factory recovery available through M5Burner. See MINIMAL-FLASH-PROPOSAL.md.

The custom boot path must fail closed on NVS initialization errors, rather than inheriting the stock erase-and-retry behavior. It must avoid unintended vendor OTA/cloud activation. M5Burner offers an [official recovery path](https://docs.m5stack.com/en/StackChan/#restore-factory-firmware), but exact image availability and pairing restoration are not yet proven.

Next implementation: resolve a supported remote Mira audio/session route, implement trusted endpoint/grant provisioning and remote gateway lifecycle, and qualify physical boot/audio after a separate exact minimal-flash approval. Custom boot/consent UI now compiles. Mac loopback remains optional diagnostics. Release acceptance requires physical boot, real Mira audio, mute/stop and recovery evidence. Download mode alone is not authorization to flash.
