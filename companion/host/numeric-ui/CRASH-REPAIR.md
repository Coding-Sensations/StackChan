# Companion launch and packaging repair

The completed app must be signed after Info.plist is written. The Swift linker’s
ad-hoc executable signature does not seal the completed bundle. Every build now
runs `verify_bundle.sh`, which checks the plist, executable and strict bundle
signature. `--build-only` builds and validates without launching. `--verify`
also launches through LaunchServices and checks the exact executable path;
it does not prove that a visible window exists or exercise live pairing.

Run this GUI app by opening `StackChanPairing.app` in Finder or through
`build_and_run.sh run`. Do not launch `Contents/MacOS/StackChanPairing` directly
inside Codex’s restricted shell. The captured startup SIGABRT was reproduced
with a strictly valid signed copy in that launch context, before any controller
or worker action. Both the preserved original and signed replacement displayed
the idle window through normal LaunchServices. Signing repairs a distinct
packaging defect; it is not the sole explanation for the registration abort.

The original delivered app and dirty project checkout remain untouched. The
replacement was built from an isolated copy of the approved persistent host
candidate; its compiled worker paths point into that retained worktree. Keep
the worktree alongside the artifact. Launch alone starts no worker. User
operation of Enroll, physical Pair and Remember and Confirm match remains
necessary. Audio is separately authorized.

All five existing Swift offline groups passed. The corrected app’s first idle
window, remembered controls and last-window-close termination were inspected.
Repeat launch remained running without a new crash report. Further visible UI
fixture testing was blocked by the computer-use service’s ScreenCaptureKit
capture error; no permissions were changed to work around it. Final exact
idle diagnostic processes were closed after verifying no workers/listener.
