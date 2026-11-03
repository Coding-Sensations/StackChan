# StackChan flashing review — no write authorization

Historical image review, with a later custom boot build recorded in BOOT-IMPLEMENTATION.md. Reviewed 8 October 2026 after Michael placed the robot in download mode and inserted the prepared SD card.

## Verified now

- USB VID/PID `303a:1001`, USB-Serial/JTAG; port `/dev/cu.usbmodem31201`. ROM queries succeed with `--before no_reset --after no_reset --no-stub --connect-attempts 1`. Device remains in bootloader.
- ESP32-S3 QFN56 revision 0.2, 40 MHz crystal, 16 MB quad flash; secure boot and flash encryption disabled. The ROM MAC matches the USB serial identity. This does not establish the exact display/controller manufacturing revision.
- Read only flash `0x8000..0x8fff`; no NVS, credentials or installed application were retrieved. First 3072 bytes exactly match the build partition table.
- Image `firmware/build/stack-chan.bin`: 3,789,424 bytes; SHA-256 `1b2c3189e71aa4923b7181fc6689475c7eeee2f5fcca572b607c49af2b77a29f`; `esptool 4.12.0 image_info` reports valid checksum and validation hash, ESP32-S3.
- Target config `CONFIG_BOARD_TYPE_M5STACK_STACK_CHAN=y`; generated flash settings DIO, 80 MHz, 16 MB. Use generated settings as authority; sdkconfig also contains contradictory QIO choice flags, so reconcile those before the next release build.

## Matching partition map

| Partition | Offset | Size |
| --- | --- | --- |
| NVS | `0x9000` | `0x4000` |
| OTA metadata | `0xd000` | `0x2000` |
| PHY initialization | `0xf000` | `0x1000` |
| OTA application 0 | `0x20000` | `0x4f0000` |
| OTA application 1 | `0x510000` | `0x4f0000` |
| Assets | `0xa00000` | `0x400000` |
| Core dump | `0xe00000` | `0x10000` |

The new application fits an OTA slot. Active slot/OTA metadata has not been read, so `0x20000` is a build target, not a proven current boot target. An app-only write must not be proposed until that selection is known.

## Why not flash this image yet

The reviewed earlier image had no boot/UI activation. The later custom boot image now compiles, but trusted grant provisioning and physical acceptance are still incomplete. Native Mac audio has synthetic callback and codec evidence, but no physical IO, native Mira routing, live TLS listener or real robot pairing acceptance. WebSocket handshake/lifecycle review remains open. This image therefore cannot yet deliver the requested custom companion.

No installed firmware backup exists. Saved upstream-built baseline app/bootloader/table files do not reproduce current installed state, and no exact vendor recovery download was qualified. The current generated full flash manifest writes bootloader `0x0`, table `0x8000`, app `0x20000`, OTA metadata `0xd000` and assets `0xa00000`. It does not directly write NVS, but replacing the table/bootloader/OTA metadata changes boot behavior. Stock HAL also erases NVS on certain initialization failures. Keeping NVS sectors out of a write command does not prove preservation of Wi-Fi, pairing or calibration after boot.

## Concrete next steps

1. Finish custom activation and fail-closed NVS behavior; qualify Mac routing and production transport without a firmware write.
2. Obtain explicit permission for a local installed-state backup containing Wi-Fi/pairing secrets. Read the whole 16 MB flash into a restricted, non-synced local recovery folder; checksum it, verify its length and partition contents without printing secrets, and assess restore compatibility. Reading is not erasure, but this sensitive backup is not implied by download-mode entry.
3. Read only the OTA metadata and non-secret app headers as part of that approved recovery review to determine the active slot and installed version. Preserve the other slot if a tested app-only strategy is feasible. A backup checksum alone does not prove successful physical restoration.
4. Verify exact hardware/display compatibility and archive the intended release image, offsets, hashes and recovery recipe. Offer a specific app-only or full-manifest write proposal with its actual affected regions and remaining restoration limits.
5. Only after that exact write is approved, execute the reviewed command, verify written bytes, then explicitly authorize/reset for a bounded first boot with audio and motion initially muted. SD remains inserted; formatting or inserting SD does not authorize flash.

For the installed **esptool 4.12.0**, the supported full-manifest command shape is `python -m esptool --chip esp32s3 --port <verified-port> --baud 115200 --before no_reset --after no_reset --no-stub write_flash @flash_args`, from the build folder with the pinned SDK Python. This is a documented command shape, not an approved command for this image. The generated response file supplies the flash-setting flags and all five offset/file pairs. The final reviewed argv must use either that response file or explicit equivalent flags/offset pairs. Do not add `--erase-all`, `erase_flash`, `--force`, security overrides or an automatic reset. An app-only command instead names the verified chosen slot and one image; it still erases the affected 4096-byte sectors during writing.

Official [download-mode instructions](https://docs.m5stack.com/en/StackChan/#download-mode): use the base USB-C data port; hold RST for about three seconds until the green indicator, then release. Michael has already done this; no repeat is needed. Official [M5Burner recovery](https://docs.m5stack.com/en/StackChan/#restore-factory-firmware) is an available vendor procedure, not proven installed-state rollback. Espressif documents [flash reading and writing](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/esptool/basic-commands.html); the local 4.12.0 CLI help was checked because current documentation uses newer hyphenated command spellings.

Evidence: `DOWNLOAD-MODE-READINESS.json`, `PARTITION-REVIEW.json`, `robot-partition-table.bin` and firmware build/test receipts. No flash write, erase, firmware reset, credential retrieval or stub execution occurred in this review. Serial ROM queries transiently configured the flash interface for reads.
