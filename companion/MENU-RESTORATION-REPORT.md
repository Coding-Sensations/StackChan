# Stock menu hierarchy restoration

Michael reported that Setup functions had become individual launcher tiles. Source inspection confirmed a custom registration change, not a fallback or rendering bug: Servo, Microphone, RGB, Account, Updates, Welcome and standalone Wi-Fi were installed as launcher apps; a duplicate custom App Center listed those apps too. The official Setup and launcher view files had remained unchanged.

The launcher now contains the official seven apps in official order: AI.AGENT, AVATAR, ESPNOW.REMOTE, APP.CENTER, EZDATA, DANCE, SETUP. Companion, Tools and Games follow. APP.CENTER uses its original metadata/icon/theme and restored real catalog/install behavior. No individual setting or tool is installed as a launcher tile.

SETUP contains the original six sections and twelve rows, with original fonts, colors, spacing, vertical scroll, status/home widgets and ten-tap version behavior. The added Welcome row is removed; onboarding remains available for unconfigured boot. A stock hardware/account/update row goes directly to its original worker through isolated resource ownership. A used driver runtime restarts into the stopped Setup menu; the user selects the row again. Done/Back and Wi-Fi completion return to Setup after required driver reset. One-shot reset cookies carry only the destination, never permission to start a service. Original settings scopes, commit behavior and driver separation are preserved.

Tools follows the requested hierarchy: launcher -> one Tools tile -> Infrared/NFC selection -> dedicated graphical Learn/Send/NFC/Edit/Emulate pages. Its hardware actions still require their explicit page controls. Disabled Send/Emulate actions now look dim. Companion and Games remain separate additions.

## Verification

Actual LVGL/UITK rendering of the official and candidate Setup view produced byte-identical pixels at four scroll positions (0, 360, 780 and max 836). Actual extracted Tools UI code rendered nine pages/variants, including selection, graphics, URL keyboard and visibly disabled empty actions. Source comparison found no baseline differences in launcher/Setup view code, common widgets or original assets. The render harness stubs Tools hardware, does not dispatch actions and omits firmware status/home overlays; these are software visuals, not device screenshots.

Ten host-check groups passed: menu hierarchy, actual one-shot/wrong-reset navigation cookie, advanced settings, installer policy, credentials, Wi-Fi persistence/migration, scoped settings, Tools layout/navigation and IR/NFC/2048 models. Independent review found no blocking source finding, then matched all 272 build source hashes with zero mismatches.

Reviewed source: `8332120220d9f428af310e23409276cc829107b5`. ESP-IDF build exited 0 with one nice-20 worker, network denied and source hashes unchanged. Image: `06026c2ab598d0e5d0db40656f500737c6c95f6775b5dc934720659c6baf2c77`, 3,956,816 bytes, 24% app partition free. Offline esptool checksum/image validation passed. Archive: `.tools/menu-restoration-build-06026c2ab598` in the project root. Visual evidence: `.tools/menu-visual-evidence-8332120`.

No flash or hardware operation occurred during this correction. Device still has the prior `a9e360e003dd8f0349a33ab0271e41f0f2250951ec17fd8a0b862d5165fde785` image, which has a reported menu regression and is not fully accepted. After a separately authorized flash of this exact correction, physical acceptance must cover Setup scroll/rows, worker Done/Back, Wi-Fi save/reboot, top-level app exits and nested Tools navigation. Original radio/cloud/account behavior and Mira live audio remain separate acceptance items.
