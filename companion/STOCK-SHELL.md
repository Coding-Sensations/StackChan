# Stock launcher integration

The custom build uses the official StackChan source and Mooncake launcher, icons,
scrolling and Setup menu as its base. Main enters `stock_shell::Run()` when
`STACKCHAN_COMPANION_BOOT` is enabled. Keep that flag enabled for this build.
The normal vendor initialization path can activate hardware and cloud features.

Working source integrations:

- Stock launcher and screensaver; local avatar; stock brightness and volume
  controls for this boot only.
- Companion Pair / Connect / Allow / Mute, with the existing trusted provisioning
  gate. Back closes the provisioning window, cancels pending grants, disconnects
  the socket and waits for codec stop before returning to the launcher.
- Tools: explicit 10-second IR capture (GPIO10), one bounded replay at 38kHz
  (GPIO5), and HTTP(S) NFC URL emulation for at most 60 seconds. These are
  original native implementations; the NFC transport uses four pinned official
  MIT M5Stack libraries. URL and learned IR are RAM-only. Stop and Back stop
  active resources. No IR database, SD storage or general NFC dump/write tool.
- Games: original touch-controlled 2048, silent, with merge/score bounds.

All original app sources and Setup entries remain. Vendor AI, ESP-NOW control,
App Center, EzData, Dance and Setup operations involving credentials, motion,
network setup or persistent changes currently display a gate explanation. Their
full runtime functionality has not been integrated into this custom build.
Launcher startup does not initialize NFC/RMT, camera, servos or vendor cloud.

## Reproduce the additional native components

From the repository root, using Python 3:

```sh
python3 companion/setup_native_nfc.py
```

This downloads only the immutable official archives in `NFC-VENDOR-PINS.json`,
checks archive sizes, SHA256 and MIT license files, rejects unsafe extraction
paths, and verifies existing trees including their complete file sets. It
executes no vendor installer. Components live under ignored `firmware/components`
folders; the bootstrap is required on a clean checkout. Use the existing pinned
ESP-IDF 5.5.4 setup, fetch/verify the original source dependencies, configure,
build the `lang_header` target, then build with one worker:

```sh
cd firmware
idf.py reconfigure
cmake --build build --target lang_header --parallel 1
cmake --build build --parallel 1
```

Portable model validation (no device access):

```sh
clang++ -std=c++17 -Wall -Wextra -Werror \
  -I firmware/main/apps/stock_shell \
  companion/host/boot-tests/test_stock_models.cpp -o /tmp/stock-model-tests
/tmp/stock-model-tests
```

Timing uses ESP-IDF's 64-bit monotonic clock; elapsed-time checks also reject
clock reversal. Tests cross the 32-bit uptime boundary.

## Qualification limits

IR/NFC code compilation does not establish physical operation. The exact device
SKU/panel revision remains unconfirmed. The verified original StackChan family
has onboard IR and ST25R3916 NFC; the physical test must confirm behavior and
resource shutdown on this unit before relying on them.

USB controllers are a separate qualification task: actual controller identity,
HID reports, port/adapter and VBUS power must be checked. This build contains no
USB-host driver, Bluetooth controller pairing or all-controller compatibility.

The Mac deny-auth finite backend smoke test passed health 200 and diagnostics
401 and then closed its listener. No ongoing service or audio provider was
activated. Robot-to-Mac trusted TLS routing and a supported same-conversation
Mira provider remain unresolved. This firmware has not been flashed.

## Executed build result

Full ESP-IDF build passed for source `d8af2e19bdd6e47c806e7408855a6881cea6dfbe`,
with one nice20 worker and network denied. Image: 3,745,952 bytes, SHA256
`e9399ffbc92d776aecdc53b8bbc77edb4c227d87aa79a6062e6c2bcc91ab0031`.
Esptool 4.12 validated its checksum and validation hash. Exact source hashes,
artifact archive and test summary: `STOCK-SHELL-FIRMWARE-BUILD.json`.
No flash, physical IR/NFC/controller acceptance or ongoing bridge service.
