# Minimal flash proposal — not authorized or executed

Michael declined full backup/encryption (Sentinel_78388b2c9e84819196164db569c38aba). No backup reads or password steps will continue. This proposal writes only the custom app and OTA-selection metadata, not the generated full flash_args bundle.

| Offset | Exact archived file | Bytes | SHA-256 |
| --- | --- | --- | --- |
| 0x20000 | .tools/custom-boot-182752bf8543/stack-chan.bin | 2,577,504 | 182752bf85437a1f3828c0ab286e8b052b2fe07bcfa21a654caa30346b7ae2fe |
| 0xd000 | .tools/custom-boot-182752bf8543/ota_data_initial.bin | 8,192 (all FF) | 7d2c7ac4888bfd75cd5f56e8d61f69595121183afc81556c876732fd3782c62f |

The image was compiled from code commit c2480f20f55a494dead17be70d74db54c9781d9c. The recorded physical partition table matches the build: NVS 0x9000..0xcfff, OTA data 0xd000..0xefff, ota_0 starts 0x20000 and is 0x4f0000 bytes, ota_1 starts 0x510000. Sector erase ranges are [0x20000,0x296000) and [0xd000,0xf000). No whole-chip erase. Existing bootloader, partition table, NVS, PHY, second app slot, assets, coredump, eFuses and SD are not targeted.

Resetting OTA metadata is needed because the currently selected slot has not been read. With no factory partition and erased OTA metadata, standard ESP-IDF boot selection tries ota_0 and initializes selection after loading it. The existing stock bootloader's exact version/bytes remain unverified, so physical boot compatibility is not guaranteed. This trades an 8 KB selection reset for avoiding a device-state backup or guessed active-slot write. Do not claim the untouched second slot contains a known recovery version.

Expected behavior: muted custom companion screen, no servo/camera/vendor cloud startup. It does not yet provide remote Mira audio or a real grant provisioning interface. Proposed scope after confirmation: fresh chip/port/layout/security checks, exactly these two writes, byte verification, and one reboot to inspect the muted UI. Do not flash until Michael confirms that concrete scope. No autonomous stock recovery write if it fails; report the observed failure first.

## Recovery and device data

The official original StackChan K151/K151-R guide prescribes M5Burner → search StackChan → Only Official → latest factory firmware. Select the factory/StackChan World firmware, not UiFlow2, generic CoreS3 UserDemo or another Xiaozhi distribution. https://docs.m5stack.com/en/StackChan/#restore-factory-firmware . This recovery procedure and an actual vendor recovery artifact are now verified; see below. The official GitHub repository has no published release assets in the inspected API catalog. The rebuilt baseline is not the shipped image.

App-only writes leave NVS unchanged. Source stores servo calibration in servo/zero_pos_1 and zero_pos_2 and board identity in board/uuid; the custom boot does not initialize servos, replace UUID, erase NVS on initialization failure, or enable writable Settings. No irretrievable calibration/identity loss was identified for this proposed write; exact shipped data contents were not inspected. If a later recovery erases NVS, the previous UUID/calibration values cannot be reconstructed from a downloaded firmware file. Vendor setup provides servo recalibration and phone re-pairing; Michael accepts re-entering Wi-Fi/pairing. Do not unbind the vendor account as part of this proposal.

Hardware compatibility: original controller family matches reported StackChan World setup and ROM ESP32-S3 rev0.2/16MB. Official guide documents ILI9342C/E panel variants; compiled driver handles panel selection, but actual display revision/boot remain unqualified. Official ROM download recovery remains the fallback route, with no physical restore test claimed.


## Downloaded factory recovery artifact

The official M5Stack catalog entry **StackChan-UserDemo V1.5.1**, published 2026-07-31, is now downloaded and offline verified. Full merged image: 12,783,789 bytes, SHA-256 `411578a2ebca2cfe3541fdc32aeddbc4703cf912a5daa7e1253f69306d6e1d87`; source `https://m5burner-cdn.m5stack.com/firmware/746f9662f48ac465cccf49bcad941414.bin`. Its embedded app declares 1.5.1, ESP-IDF5.5.4, ESP32-S3 and 16MB, with valid checksum/digest. Partition layout matches the prior physical review. Artifacts and catalog snapshot are in `.tools/stock-recovery-v1.5.1`; exact manifest is STOCK-RECOVERY-VERIFICATION.json. The vendor app differs from the rebuilt baseline; shipped-byte identity remains unproven.

The full merged image writes at 0x0 and contains blank NVS, so a full factory restore clears settings, board identity and servo calibration even without a separate whole-chip erase. Do not use that bundle for the proposed custom update. An exact embedded factory app is also saved (3,790,288 bytes, SHA-256 `c4839b0a77d030725da125db41167eda426c44ccf6e518776bac5a4d1b722dd1`), supporting a separately reviewed app-only restore at 0x20000 plus OTA init at0xd000 while existing bootloader/table/assets remain intact. No restore was executed.
