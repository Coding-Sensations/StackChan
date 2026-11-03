# Desk companion integration qualification

The default build starts the muted stock shell with the integrated companion
sources. Audio still requires the existing authenticated session and physical
Listen consent. Camera, gesture, local-vision and wake readiness gates remain
inactive where physical/platform qualification is missing. Source compilation
does not enable those capabilities.

The launcher switches selected screensavers every30 seconds. Whole light,
flashing and red/blue full-background effects, including ChessModels, are removed
from the compiled catalog. Their stable IDs are unavailable tombstones so saved
selection migration remains safe. There are217 remaining choices including DVD;
intentional neutral dark gray remains, and BLACK is true RGB565 zero.

The current named Mac aggregates expose BOTH input and output streams for BOTH
StackChan names. They are not directional endpoints. CoreAudio ignored requested
2/0 and0/2 channel hints in private metadata probes; their default eligibility
properties are not settable. The app source now checks actual channel/stream
directions and refuses to recreate or certify the duplex aliases. Internal
transport remains robot microphone -> worker output on2-A -> Mac input, and Mac
output ->2-B -> worker input -> robot speaker. No display-name swap or route change
is used as a substitute. A supported directional AudioServerPlugIn is now
implemented and independently reviewed in source: public input-only Microphone,
output-only Speaker and hidden Feed/Return. Actual SDK bundle/native/UI builds,
five sanitized C++ binaries, nine native-adapter Python checks and the expanded
pure Swift direction/catalog checks pass. Source tests include deterministic
peer-producer Stop-before-publication retirement. Existing route defaults remain;
the new native path requires explicit opt-in. Installation, exact-owned legacy
alias migration, real OS enumeration, scheduling/latency and signal qualification
remain product/installation gates; see [audio direction](AUDIO-DIRECTION.md).

Validation on the current source includes the real ESP-IDF5.5.4 app build,
2,523-file prior integration review plus scoped final deltas,111 audited Python
checks, four native sanitizer suites, complete SwiftUI/Objective-C source link,
and camera-extension/native-audio source link. All five Swift fixtures have
runtime evidence for their controller logic: Status/RememberedDevice pure checks and the three real
Foundation Process fixtures under the separately reviewed bounded runner.
The latter additionally has26 pure custody-model checks. A sixth pure fixture
checks public endpoint direction and enumeration, separately from controller lifecycle. Screensaver selection,
stable-ID migration and catalog rendering pass ASAN/UBSAN checks.

The final public unbound image is4,680,400bytes, leaving496,944bytes/9.5984% of the
unchanged5,177,344-byte app slot. SHA256:
`16f4d16a19960500dfc78d47ffc66a72c991d84d658943124d9104b819fee4b6`.
This public artifact has no local CA/host/device bindings and must not be used as
an installation profile for an existing pairing. Private deployment builds must
reuse explicitly reviewed trust pins; no keys or credentials belong in Git.
TLS certificate-date verification and the checked DMA shutdown fence remain.

SD currently holds optional IR/state/sample data through existing bounded
loaders. It does not extend executable OTA space. Physical card occupancy has
not been measured. No assets were moved or partitions changed; essential UI
fallbacks and the existing rollback arrangement remain. The selected effects
were removed rather than adding SD IO to the render/audio paths.

No qualified production Hey Mira wake model is supplied or enabled. Experimental
weights are not included; acoustic, frontend/reset and license qualification are
still required. No training, inference/export, paid session or model activation
was performed for this qualification. Historical design notes describe earlier
scaffolds and trials; this current status does not imply physical acceptance.
