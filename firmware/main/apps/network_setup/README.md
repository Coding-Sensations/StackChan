# Standalone Change Wi-Fi

`MakeWorker()` replaces the gated Setup Change Wi-Fi worker.
`MakeApp()` provides a launcher entry and stopped restart destination.
Both previews offer Start phone app (the existing official WifiSetupWorker /
StackChan World BLE onboarding) and Start browser (the official WifiManager
hotspot with network scan and manual SSID/password entry).

No radios start when the preview opens. Explicit Start requires audio owner
None. A previously initialized shared Wi-Fi driver restarts into another stopped
preview; Start is required again. `ConsumePreviewRequest()` is a one-shot cookie
for that preview, never an active setup/session request.

Active setup lasts at most five minutes. The persistent top-layer Stop / Home,
closing the worker/app, browser exit after successful configuration, and timeout
restart to the stopped shell. This avoids reusing unmatched Wi-Fi/BLE lifecycle
implementations or leaving configuration services in the launcher.

`persistence_gate.h` is dependency-free for the managed Wi-Fi backend.
Root integration must gate backend `SsidManager::SaveToNvs()` on
`CredentialPersistenceAllowed()` and call `NotifyCredentialsSaved(bool)` with
the actual commit result. Normal browser/phone SSID submissions may persist
only their Wi-Fi credential list during this explicit session. Original
credentials remain untouched until the user submits. Vendor AI provisioning
continues to use RAM-only SSIDs outside standalone Change Wi-Fi.

Root must remove the original cleartext password log, validate the phone
setWifi payload lengths/types, bound BLE request queues, and gate the original
AppConfigServer entry/credential submission to this session. No motor or avatar
remote-control listener is activated. Account/calibration/identity and other
NVS namespaces retain their existing policies. Advanced browser settings
writes remain rejected by the pinned backend variant.

The module itself never receives or prints a password, initializes audio or
camera, starts Application/vendor AI, or erases NVS. Official onboarding may
show its own app installation and login instructions; credentials remain in
the official phone app and firmware BLE protocol rather than chat.

No hardware, network, BLE, build or installation was run during implementation.
