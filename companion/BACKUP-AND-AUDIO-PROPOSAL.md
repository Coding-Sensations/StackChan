**Superseded: Michael declined the full-backup/encrypted-container workflow. Do not execute these steps or request a password. No full backup is a release prerequisite.**

# StackChan installed-state backup and Mac audio qualification proposal

Prepared 8 October 2026. Michael subsequently approved the installed-state backup. Its collection has not started: the native encrypted-container command failed before a private password prompt, and Time Machine exclusion is not confirmed. Mac audio tests remain separately unapproved and unexecuted. No flash operation is included.

## Installed-state backup: narrow approval scope

Read the robot's existing 16 MB SPI flash twice through the already verified ROM download connection, without reset, downloaded stub, erasure or flash writes. This captures installed bootloader, partition table, both app slots, OTA selection, assets, NVS and any core dump. NVS or core dump may contain Wi-Fi passwords, pairing/device-service credentials, identifiers and calibration/settings. Do not decode or print secret values. No new cloud credential or account permission is needed.

FileVault was verified **Off**. A chmod-restricted folder alone does not provide encryption. Before collecting data, Michael must supply a container passphrase directly to a trusted native password prompt, never in chat, a tool argument, a file or shell history. Create an AES-256 encrypted local disk image with Apple's Disk Utility or reviewed `hdiutil` interactive workflow. Do not change FileVault, create account credentials or store the passphrase in Keychain without separate instruction. Confirm the container can be reopened before connecting the read process. If a confidential passphrase-entry surface is unavailable, stop before the read.

Store the encrypted container outside synced Documents/iCloud and outside Git, under `/Users/mikedemott/Developer/AgentWork/StackChan/.private-recovery/`. Directory mode 0700; backup files mode 0600. Confirm sync/backup destinations and exclude this private container from any unapproved export. No upload to GitHub, Library, the SD card, cloud storage or the parent conversation. Only non-secret metadata such as length, digest and verification status may be reported.

Using the pinned SDK Python and esptool 4.12.0, after fresh port/chip/security checks, the read-only argv shape is:

```text
python -m esptool --chip esp32s3 --port <verified-port> --baud 115200 --before no_reset --after no_reset --no-stub --connect-attempts 1 read_flash --flash_size 16MB 0 0x1000000 <mounted-encrypted-container>/installed-flash.bin
```

Repeat into a second file in the same container. Each read has its own bounded supervisor; if a read cannot finish within the reviewed deadline, stop rather than retry indefinitely. The ROM does transiently configure its flash interface for reads. No persistent setting changes are intended.

Acceptance: both files exactly 16,777,216 bytes and SHA-256 equal; partition table and image structures valid; non-secret OTA metadata identifies the selected slot; no process, port handle or plaintext copy remains outside the encrypted mount. Retain the first verified copy and a manifest inside the encrypted image. Unmount and verify that remounting requires the passphrase. Report only success/failure, hash, byte count and private container location.

Restore review: confirm same physical device, same flash size, secure boot/encryption state and layout, then prepare the exact original-byte write and verification recipe. A matching duplicate read proves backup integrity, not successful restoration. Do not run a restoration test without its own write approval. Original stock image provenance and encrypted backup protection are distinct from a rebuilt upstream baseline or latest M5Burner firmware. Recovery guarantees must remain limited until actual restore evidence exists.

Previously approved scope: **May I save and verify a complete local backup of this robot, including its Wi-Fi/pairing settings and calibration, inside an encrypted container whose password you enter privately? This performs reads only and does not flash, reset or upload anything.**

## Optional Mac audio diagnostics: separate approval scope

This prototype requires a running Mac/native client and cannot satisfy production operation with the Mac off. It is optional diagnostics, not the production architecture or a mandatory next step. The production path is robot → authenticated remote WSS gateway → verified Mira session route; that final session attachment is not yet supported by an identified public interface. See EXPANSION-PLAN.md for feasibility and acceptance.

Use the existing native CoreAudio bridge and allowlisted UIDs `Pro Tools Audio Bridge 2-A_UID` and `Pro Tools Audio Bridge 2-B_UID`. Begin with fresh read-only checks of device presence, 48 kHz physical rate, available input/output streams and buffer bounds. Do not change global default devices, device sample rates or permissions silently.

First approve a short local virtual-device IO test, with no physical microphone, robot connection, voice provider or saved media. Open only the named virtual endpoints, begin muted, verify callback operation and counters, then inject a bounded generated test signal after explicit arm. Use additional scoped counterpart handles to measure actual 2-A output-to-2-A input and 2-B output-to-2-B input routing. Verify separation of buses and inactive channels. Test epoch invalidation, queue overflow/underflow and mute. Record timing/counters, never raw ambient audio. Close all AudioUnits and owned handles; no persistent service remains. A worker-only synthetic test cannot substitute for this driver test.

Acceptance: exact observed direction, channel mapping, rate/buffer compatibility, measured local driver latency and no leakage into unrelated routes. If permission or route changes are needed, pause with the concrete required change. Finite capture must not read arbitrary default audio devices.

Then separately qualify native Mira voice in the existing conversation. Michael selects the intended supported native client/session and confirms continuity; obtain explicit approval for microphone transmission and voice return through the named routes. Verify actual app input/output selection without private APIs or broad system routing changes. The current iOS call is not evidence that a Mac virtual route is connected to that conversation.

Acceptance: recognizable same Mira voice and intended memory/session continuity, intelligible speech, measured end-to-end delay, mute and interruption behavior, no duplicated actions, and no unrelated audio capture. Model/provider cancellation and physical silence must be reported as measured or unconfirmed. Half-duplex output gating does not establish spoken barge-in.

Finally propose a bounded robot test only after a custom image, installed-state recovery and exact flash have been reviewed and separately approved. The robot currently remains in download mode and has not been flashed. No new provider key, paid service, account permission, driver installation or real pairing grant has been created.
