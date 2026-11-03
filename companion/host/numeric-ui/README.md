# StackChan Companion source

This source includes the manual original-call flow, matching-number pairing,
remembered enrollment/forget controls, physical audio consent, bounded sessions
and checked cleanup. The public build has empty trust pins and expiry zero;
those source capabilities are not evidence of a configured or active session.
The existing saved device pairing is separate from this source qualification.

USB and verified Wi-Fi TLS carry pairing; there is no Bluetooth transport.
Click Pair StackChan, then physically tap Pair on the robot.
Compare the six digits and confirm on both devices. Cancel/Retry closes the attempt.
On installed a67e6c2, an audio-scoped matching confirmation is explicitly labeled
"Pair & authorize mic/speaker" and gives physical consent for that one local session.
Companion has no microphone-only mute: leave physical Listen off when audio is not
authorized, or stop the whole audio session. A stop reports release only after a clean
worker exit; a call in the user's call app may continue. The robot physical stop also ends
the session.
Audio is blocked by default. A separately approved local audio check requires both
`--approved-test` and `--approved-audio`, a matching fresh one-use authorization,
a valid `expected_boot_nonce` in that fresh authorization, and both matching-number
confirmations before any audio admission. A mismatched Ready aborts before key
loading, listener creation, protocol write or grant; its nonce/reset evidence survives.
The pairing backend suppresses application DTR/RTS updates and clears/verifies HUPCL
while retaining one bounded SCNP write. OS USB behavior still requires physical
boot continuity qualification; these controls cannot prove reset-free hardware.

Default app launch is an inert preview. Live mode requires a separately approved
exact firmware image and test, `--approved-test`, and removal of the project pause
marker by the approved test operator. Keep the marker until that approval.
The window has no token field, clipboard operation, or credential log.

The comparison code is a human session check within approved TLS trust, not a
password or a generic cryptographic SAS protocol. Both confirmations remain
required for enrollment. Camera and wake providers remain unavailable until
their separate qualification gates are satisfied.

Build-only qualification without explicit signing, app launch or fixture execution:

```sh
companion/host/numeric-ui/script/build_and_run.sh --compile-only
companion/host/numeric-ui/script/test_offline.sh --compile-only
```

The helpers compile the current Swift models/services/views and the required
Objective-C bridge. Compile-only app output defaults to the dedicated project
`.tools/numeric-ui/source-only/StackChanPairing.app`; `STACKCHAN_UI_BUILD_ROOT`
can select another output directory. An existing `_CodeSignature` directory is
rejected in this mode. The compiler/linker may emit platform-default Mach-O code
signature data; no explicit codesign, installed-app replacement or activation is
performed by compile-only mode. It does not certify an artifact as never signed.

`STACKCHAN_OFFLINE_BUILD_ROOT` selects the fixture output/module-cache directory.
`--compile-only` on the offline helper compiles all eight fixtures and runs none.
The default offline mode executes the fixtures without resource supervision.
For bounded process-fixture qualification, `script/run_process_fixtures.py` uses
the separate audited no-fork policy documented in `script/FOUNDATION-FIXTURES.md`.
Synthetic Python workers register and stop before their test bodies; the runner
checks kernel birth, lineage and session, records custody, then resumes them.
Real Foundation Process pipes, termination handlers and signal escalation remain
under test. The original strict single-group watchdog is retained byte-identical;
this runner explicitly permits owned child process groups within its session.
Compilation alone is never runtime acceptance.

`--build-only` retains the existing explicit ad-hoc signing and bundle verification
behavior. Other build-and-run modes can launch the app and are outside source-only
qualification. No helper should be run in a mode that exceeds the current operator's
authorization. The installed, idle app is not modified by these source checks.

Live operation requires separately approved bindings/configuration and existing
operator/device consent gates. `STACKCHAN_BINDING_CONFIG` selects an explicit
owner-only binding file; `HOME_LAB_ROOT` supplies the existing local host source.
The build helper embeds the canonical checkout root as `StackChanRoot` in the
bundle before signing, so normal LaunchServices launch can resolve it without
inheriting `STACKCHAN_ROOT`. Rebuild when moving the checkout.
Set `STACKCHAN_PYTHON` to the absolute executable in the approved Python virtual
environment (paths containing spaces are supported). An invalid explicit value
fails setup without falling back. With no override, the worker uses this
checkout's `.venv/bin/python3` or `.venv/bin/python`, then `python3` in an absolute
directory of the inherited `PATH`. Empty/relative PATH entries are ignored. No
interpreter or dependencies are provisioned by the build, and resolution does
not launch a probe. Finder's PATH may be limited, so an explicit value is preferred.
Neither value is populated by the public source build. No real credential, private
key, current certificate validity or physical card content is inferred here.
The window has no token field, clipboard operation or credential log.

Opus/FFmpeg libraries use platform discovery, with `/opt/homebrew/lib` and
`/usr/local/lib` fallbacks on macOS and normal `.so` loader search on Linux.
Custom installations can set `STACKCHAN_OPUS_LIBRARY`,
`STACKCHAN_SWRESAMPLE_LIBRARY`, and `STACKCHAN_AVUTIL_LIBRARY` to absolute library
paths. An empty, relative or unloadable override fails without fallback. Select
libraries compatible with the Python process architecture and the existing codec
API; this does not install libraries or enable audio.

The worker anchors its finite deadline at robot Ready, including its startup and
control-loop margin. This does not prove boot continuity, physical audio timing,
route restoration or capture shutdown on actual hardware. See
[offline boundaries](COMPANION-OFFLINE.md) and [integration seams](HOME-INTEGRATION.md)
for the historical design context; current qualification evidence takes precedence
over earlier installed-image and certificate descriptions in those documents.

The candidate requests the genuine public v2 Microphone/Speaker endpoints. Route
availability uses the complete read-only four-endpoint plugin/provenance,
direction and visible-catalog verifier; duplex aliases cannot satisfy it. The
worker also requires an explicit owner-only directional binding and matching
native-library hash. An old binding intentionally rejects the candidate UI's new
route; prepare and review app/binding/library adoption together. Existing installed
apps/profiles are not replaced by a source build. A separate staged app identifier
can be used for a bounded acceptance window without assuming existing privacy
grants transfer. See [native backend selection](../native-audio/README.md).
