# Directional CoreAudio candidate — source qualified, not installed

This original AudioServerPlugIn implements real one-way devices through the
public Apple SDK. It replaces the aggregate-wrapper approach in source. Current
installed aliases still expose both directions until separately approved
installation and migration; building this directory changes no OS devices.

| Endpoint | UID suffix (`local.mrkillbob.StackChan.hal.`) | Scope | Visibility | Pair |
|---|---|---|---|---|
| StackChan Microphone | `microphone.v2` | input only | public | Feed → Microphone |
| StackChan Speaker | `speaker.v2` | output only | public | Speaker → Return |
| StackChan Internal Feed | `feed.v2` | output only | hidden | Feed → Microphone |
| StackChan Internal Return | `return.v2` | input only | hidden | Speaker → Return |

Each device owns exactly one two-channel native packed Float32 stream at48kHz.
There is no stream in its opposite scope. Only public endpoints may be default
devices, in their actual scope; only Speaker may be system-output default. Hidden
is a discovery flag, not access control. The driver does not open another audio
device, bypass privacy permission, provide PCM properties, create an IPC file,
or use sockets/helpers. Other apps may resolve hidden UIDs through public APIs.

All devices use one immutable mach clock anchor/domain/seed and a16384-frame
zero-timestamp period, above the SDK's10923 minimum. The IO path uses the actual
cycle input/output sample time and actual frame count (maximum2048), independent
of that period. Input reads timestamp minus4096 frames and advertises4096 frames
of device input latency (85.33ms), zero stream latency/safety offset. This is a
conservative synthetic transport setting; real HAL scheduling, actual measured
latency and acceptable conversation delay remain installation acceptance gates.

Two separate32768-frame timestamp rings isolate directions. Atomic packed PCM,
version/frame/epoch metadata and immortal producer lease tags allow multiple
readers without consuming a cursor or racing plain floats. Missing, overwritten,
non-finite or revoked data becomes silence; finite samples clamp to[-1,1].
Callbacks use bounded loops and preallocated storage, no allocation/locks/logs.
A contested writer fails rather than blocking. Registered clients use monotonic
tokens; both0 andUINT32_MAX IDs work. Stopping one producer revokes its frames
without revoking peers. Last-producer Stop retires the timeline immediately.
Stop logically completes without requiring a host retry; a new Start can reopen
only once old callbacks drain. Device/client storage remains allocated for the
plugin lifetime. This is not a physical speaker-drain guarantee.

The existing native AUHAL bridge keeps its legacy2-A/2-B route unchanged.
`sc_open_directional` and `open_directional_for_approved_test` are additive,
explicit entry points: write robot mic to hidden Feed, read robot playback from
hidden Return. Exact UID translation finds hidden endpoints and verifies owner
plugin bundle/class, hidden/virtual flags, scoped streams and48kHz stereo float
format. There is no fallback from failed directional lookup to another device.
The existing Opus/resampler, physical Listen and checked-close gates remain.
The current worker/profile never calls the new opt-in entry point.

Swift's Check action only reads metadata. It verifies all four new UID/name/
owner/direction/visibility tuples and a successful visible-device snapshot with
both public UIDs and no legacy aliases or hidden endpoints. Failed enumeration
cannot certify absence. It never installs, removes, starts IO or changes defaults.

## Offline checks

`tests/driver_test.cpp` statically includes the driver behind a mock host; it does
not dlopen/install it or call HAL IO. It checks property/UID/format/scoped-stream
enumeration, default eligibility, class qualifiers, shared timing, isolated PCM
routes, multiple readers/producers, client retirement, last-stop quiescence and
a deterministic Stop-before-publication race while a peer remains active.
`tests/timeline_test.cpp` covers wraparound, stale epochs, duplicate writes,
clamping/silence and concurrent readers. Native resolver mocks reject incorrect
provenance/formats and hidden-lookup failures. Python mocks cover explicit opt-in
and error cleanup; Swift's pure fixture checks direction/catalog failures.

Compile with the installed Xcode C++17 compiler and macOS SDK; link CoreAudio and
CoreFoundation, and AudioUnit for native bridge tests. Sanitizer checks use
`-fsanitize=address,undefined -Wall -Wextra -Werror`. Bundle compile uses
`-bundle -fvisibility=hidden -mmacosx-version-min=14.0`; copy Info.plist into
`StackChanDirectionalAudio.driver/Contents` and place the executable under
`Contents/MacOS/StackChanDirectionalAudio`. `StackChanAudioFactory` is the sole
export. These are build instructions, not installation instructions. Intel,
distribution signing/notarization and real-host loading are not qualified.

## Dependency and license inspection

The implementation uses only existing Apple SDK/frameworks and C++ standard
library. No third-party driver/library or package was downloaded or incorporated.
The existing installed Focusrite, Parrot and Pro Tools HAL bundles are not copied,
modified or redistributed; legacy Pro Tools routing still depends on the user's
existing installation and its terms.

[libASPL](https://github.com/gavv/libASPL) was evaluated as an alternative shim.
Its [MIT license](https://github.com/gavv/libASPL/blob/main/LICENSE) accompanies
separate [Apple2012](https://github.com/gavv/libASPL/blob/main/LICENSE.apple2012)
and [Apple2020](https://github.com/gavv/libASPL/blob/main/LICENSE.apple2020)
notices. It was not adopted, pinned, installed or linked. No claim of compliance
for a hypothetical future vendored version is made. The AUHAL integration follows
[Apple TN2091](https://developer.apple.com/library/archive/technotes/tn2091/_index.html)
and the installed public AudioServerPlugIn/AudioHardwareBase header contracts.

The remaining approval boundary is reviewed driver installation, audio-service
reload and exact-owned legacy-alias migration, initially followed only by OS
metadata enumeration. Live IO and physical Listen acceptance are later gates.
No driver installation, service reload, alias removal, app replacement, default
route write, microphone/camera capture or playback occurred in these checks.
