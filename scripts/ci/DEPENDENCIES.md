# Offline Companion fixture inputs

`pyserial==3.5` and `websockets==15.0.1` are CI-only dependencies; the requirements file pins their universal PyPI wheel SHA256 values. No serial port is opened by installing it. Fixtures inject fake serial ports.

`companion/host/desk_robot/session_contract.py` is restored byte-for-byte from the same-project source commit `0fba4813891b28548ce3e505434a275afb4cc406` (path unchanged), SHA256 `a138cb59277a24277e4fd1505da56e8f7f30e123d08ea312522f053bdf8b1510`. It is an inert policy/state model using only the standard library; it performs no IO, captures, credential handling or dispatch. The existing optional desk-robot fixture imports this source, so missing it was an import failure, not a test exclusion to accept.

Public StackChan CI runs the standalone numeric worker regression suite and, where present, public trust/custody model fixtures. The full streaming integration suite is NOT claimed by that public lane: it imports `home_lab.devices`, `sessions`, `storage` and `contracts` from the private HomeLab repository. Do not copy those private sources into the public fork or silently exclude missing protocol tests. `test_companion_python.py` retains a fail-closed full-suite entry point for approved private HomeLab CI, with `HOME_LAB_ROOT` set to its exact checkout and the public bridge paths supplied from this checkout. It includes localhost listener fixtures; local sandbox-denied listener checks remain unexecuted, not passes. The trusted manual dispatcher is now reviewed and published in HomeLab main; actual private execution still requires its approved runner route and exact reviewed source pins.

The H01 history also predates three public, inert bridge/playback modules used by its protocol fixtures. They are copied without modification from public StackChan commit `71e1e2850e16cd6f2e3d2aa9c0c262fe78a31634`, with these SHA256 values:

- `bridge/host-adapter/host_adapter.py`: `e03a1827806ba828ac7398c486d3bd58e211d8edafaf9d5415664d39a829925f`
- `bridge/local-endpoint/codec.py`: `7cfd32d7e3231f20049598ef5152095ee9c7682e507fd61bbe5b2f378b5a919a`
- `companion/host/desk_robot/device_playback.py`: `a04a0a4e801526bbb21ad00d94583ac0a688be0ea0cfc1ee5eecfc21bc155061`
