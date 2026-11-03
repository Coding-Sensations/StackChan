# Local firmware checks

Run from the fork root:

```sh
python3 companion/host/boot-tests/run_stock_restoration_checks.py
```

The runner generates the pinned vendor adapter in a temporary directory, compiles five C++17 programs with the existing compiler and runs credential bounds/CRC/truncation tests, NVS save/failure/legacy-key preservation fixtures, scoped Settings writes, audio ownership/Tools layout and 2048/NEC/NFC model checks. It creates no network service and accesses no device. Temporary executables are removed on exit. A narrow sign-compare exemption matches ESP-IDF for unchanged vendor index guards.

Settings are read-only by default, with explicit feature/key scopes permitting the original Confirm paths. Phone provisioning has a separate explicit-session gate. MQTT/WebSocket activation remains a Stock-owned RAM overlay.

The separate provisioning integration runner and its JSON receipt cover 39 Python tests plus cross-language receiver/window checks. Those tests do not establish live transport or hardware acceptance.

Firmware builds use the project BUILD.md pinned ESP-IDF 5.5.4 export/configuration commands, one low-priority worker, and the reviewed generated source adapter. Compilation establishes compilation/linking only. See companion/STOCK-REGRESSION-AUDIT.md for every required device acceptance check and the deliberate stock-parity differences.
