# Stock feature restoration and acceptance

Official reference: M5Stack StackChan `1b5765599fba8aaad1811d9a79358ccc7051f5f3` (firmware 1.5.1). The reported regression was reproduced by source inspection of flashed image `000a245183e288df4cafdcefb4af16892c8ebef3efc933c7fcb64b7c4176c4f3`, built from `1cedafdad113b3fb501b4ee5cc6e84b0671a427d`.

Cause: custom Setup replaced every callback except brightness/volume with LockedFeatureWorker. Global read-only Settings denied Confirm persistence; the generated SsidManager discarded saves. Several launcher entries were placeholders. This was a custom-firmware regression.

The restoration uses real original classes/workers and an explicit-start session before radios, microphone, camera or motors. Original services without joined shutdown return through MCU reset; motor VM_EN is explicitly switched off before exit and again during stopped boot. The board's actual off state still requires measurement/device validation. Software resets suppress saved AI autostart so Stop/Home remains usable. General Confirm supplies a new opt-in before power-on/reset boot can autostart AI; old saved boot_ai alone is insufficient.

Successful Wi-Fi choices use one versioned CRC-protected wifi/config_v1 NVS blob. Legacy Wi-Fi keys remain untouched as fallback during provisioning; a separately confirmed firmware installation exports and reads back the current saved networks into the original legacy keys before any OTA image write. Failed export blocks installation and preserves the custom blob. Account/calibration namespaces remain unchanged by provisioning. Both native and companion saved-network readers use the same SSID manager. A failed association does not save credentials; backend failures remain visible instead of returning Home as though saved. No Wi-Fi secrets belong in logs, receipts or chat.

## Evidence boundaries

Source restoration means the real implementation is reachable. Host checks exercise bounded credential encoding/decoding, tamper/truncation rejection, gated writes, commit failure, legacy-key preservation, scoped setting writes and existing Tools/game algorithms. An ESP-IDF compile establishes compilation/linking, not successful radio, cloud, servo or microphone operation. Every restored feature below still needs device acceptance on the replacement image. Prior menu/flash acceptance does not establish it.

| Original feature | Source implementation | Software check | Required device acceptance |
|---|---|---|---|
| Launcher/icons/carousel/screensaver | Original launcher, distinct installed app names | Source review/build | Scroll every entry; return repeatedly; 30s idle wake; no unintended hardware activity |
| Initial setup | Real phone Wi-Fi worker reachable from Setup/Wi-Fi Setup; original Welcome/servo/Wi-Fi wizard restored, reached through a stopped preview on an unconfigured cold boot | Source review/build | New/unconfigured cold boot offers Welcome; Start/Skip, servo test and phone setup behave; successful commit marks configured and reboot retains it |
| Change Wi-Fi | Actual original phone BLE setup plus official browser AP; scoped durable save after connection | Credential/store host checks; source review/build | Phone app handshake, select another network, reboot and reconnect; no-network, wrong password, retry, open/32-byte SSID, cancel; saved network used by Companion/App Center/Stock AI |
| Browser advanced settings | Original activation URL, TX power, remember BSSID and sleep mode restored with fixed NVS types/keys and explicit setup gate | Portable URL/power validator; source/build | Read current values, valid submit, invalid/duplicate fields rejected; expired setup rejected; reboot retains values; no automatic firmware install |
| Phone binding/configured status | Original handshake and app_config/is_configed scope | Settings host check; source review/build | StackChan World discovers/connects; ID/status and successful configured state survive reboot |
| Brightness | Original worker; scoped display/brightness Confirm | Scoped-policy checks; source review/build | Preview, Cancel rollback, Confirm, reboot retained value |
| Volume | Original worker; codec current-session getter; preview suppressed persistence, Confirm saves audio/output_volume | Scoped-policy checks; source review/build | Preview/Cancel/Confirm, audible output, re-entry and reboot value; mute behavior |
| Timezone/RTC/SNTP | Original worker; correct system/tz key; offline RTC initialized, network SNTP preserved | Timezone scope check; source review/build | Change zone, reboot, correct clock; network time sync |
| AI General | Real worker; idle movement setting and explicit new boot opt-in | Source review/build | Off/Low/Medium/High actual frequency; Confirm/reboot; opt-in starts Stock, Stop/Home returns muted selector |
| AI power saving | Real worker; Stock runtime initializes/polls original power timer | Source review/build | Idle timeout; touch/speech wake; charging shutdown policy; Off setting |
| Servo calibration/test | Actual ZeroCalibrationWorker with powered hardware only after Start; scoped servo keys | Source review/build | Home/Set Home/Reset Default/Done; limits; saved calibration; VM_EN off after Stop/Home/reboot |
| Microphone test | Actual MicTestWorker, exclusive Native audio owner | Audio ownership/model checks; source review/build | Record/playback, retry, stop, switch to Companion/Stock with no lingering audio |
| RGB strip | Actual RgbTestWorker; IO init leaves motor power off | Source review/build | Colors/brightness/exit; motors remain unpowered |
| Account/status/reset | Actual AccountWorker, account write scope; original reset confirmation | Source review/build | Read status, back/retry. Unbind/factory reset is a separate explicit destructive device test; never part of automatic acceptance |
| Avatar service | Original AppAvatar, network/camera/video/messages/calls/motion | Source review/build | Bound account; messages, calls/video, remote motion; disconnect/retry; Stop ends radio/camera/audio/motor use |
| Stock AI | Original Application through Hal.startXiaozhi; all original user-only MCP tools, assets, identity, persistent protocol/volume/brightness, reminder/head/IMU/motion/speech/camera | Source review/build | Activation, wake word, listening/speaking, expression, head-pet/IMU, bounded MCP movement/LED/reminders, system/screen information, screenshot/preview, asset URL selection/download, confirmed manual update; original hotspot saves; Stop Home; repeat startup |
| ESP-NOW | Original sender/receiver app in isolated radio runtime | Source review/build | Role/channel/peer IDs, filter, pose, laser, loss/retry; laser and motor off on exit |
| EzData | Original app, MQTT pairing/data controls | Source review/build | Pairing/code/account data, controls, failure/retry, motor-off exit |
| Dance | Original BLE avatar/motion/RGB app | Source review/build | Phone connects, motion/RGB commands, repeated sessions, stop/no stale BLE callbacks |
| App Center | Original vendor catalog/details, actual firmware download/ESP image validation/activation through separate local replacement confirmation | Model/bounds/source checks; build | Catalog success/failure/cancel; selected source visible; Cancel writes nothing; separate confirmed replacement installs and restarts; failure retains current boot app, Retry reconfirms; return/switch without radio-driver collision |
| Firmware/version | Ten-tap original version worker; real query and separate locally confirmed download/installation | Source review/build | Correct version/query result; target/source visible; Cancel; deliberately accepted install replaces this custom firmware; error/retry/back |
| Tools/Game additions | IR learn/send, NFC URL keyboard/emulation, 2048 retained | Existing NEC/NFC/2048 and target-layout checks | IR timeout/send; NFC read/stop; keyboard navigation; 2048 gestures/back; switch into every restored service |
| Shared resource ownership | Mira/Stock/Native exclusivity; used radios restart only into stopped preview | Audio CAS/source checks; build | All ordering combinations; no duplicate Wi-Fi init, codec overlap or unexpected automatic restart |

## Explicit remaining differences

App Center entries are replacement firmware, as in the official project. Installation removes this build's custom features; both catalog and Stock AI/manual-update paths explain that consequence and show the selected source before separate local confirmation. HTTPS uses the board network stack; HTTP sources are explicitly labeled unencrypted. Downloads are bounded by inactive-partition size, declared length, 4-second I/O timeout and a 3-minute overall deadline. ESP image validation must succeed before boot selection. No automatic update is performed merely because newer metadata exists.

Welcome/servo/phone onboarding, browser advanced POST, original user-only MCP tools, asset downloads, protocol identity/settings and actual confirmed updates are restored in source. Acoustic/BLUFI provisioning remains disabled by the original actual SDK configuration. Deliberate navigation/consent differences: a stopped preview before native services, fresh boot-AI opt-in, suppressed auto-start after software reset, and returning to the launcher rather than restoring original numeric warm-reboot app indexes. Original services are available after Start. These differences retain software separation between Companion, Stock AI and native applications.

Cloud/account-backed stock features still require their original reachable vendor services and a valid user account. The custom live Mira audio route remains unqualified and is a separate external integration blocker; it does not establish a restored stock feature test.

No replacement image is flashed merely because it builds. Device acceptance requires approval/readiness for the exact reviewed image, followed by the checks above. Do not change active network settings or perform account/reset operations automatically.

## Reviewed build

Source `c04acc3c63fefc3c81614d5de88ea2686df71f51` passed independent source review and eight host-check groups. ESP-IDF build exited 0 with one nice-20 worker, network denied, and every tracked source hash unchanged during compilation. Image `a9e360e003dd8f0349a33ab0271e41f0f2250951ec17fd8a0b862d5165fde785`, 3,966,176 bytes, leaves 23% of the app partition free. Offline esptool checksum/image validation passed. Archive: `.tools/restoration-build-a9e360e003dd` at the project root. This image has not been flashed; all physical acceptance remains pending.

## Menu correction after physical feedback

The prior a9e360e003dd image was flashed and verified, but Michael reported its expanded launcher/Setup hierarchy as a regression. It is not fully accepted. The correction is documented in [MENU-RESTORATION-REPORT.md](MENU-RESTORATION-REPORT.md): original seven-app order, one original Setup list, nested Tools pages and reset-safe return routing. Corrected source/image/build/visual evidence supersede the earlier menu acceptance claims. New image 06026c2ab598d0e5d0db40656f500737c6c95f6775b5dc934720659c6baf2c77 has not been flashed; physical Wi-Fi and other acceptance remain pending.
