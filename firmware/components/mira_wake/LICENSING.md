# Pinned third-party source attribution

These source snapshots preserve upstream notices and LICENSE/COPYING files in place. See SOURCE-PINS.json for registry version metadata, upstream URLs, component hashes, per-file SHA-256 pins, and exact selected sources. No network resolution occurs at build time.

- espressif/esp-tflite-micro 1.3.3~1: Apache-2.0; original LICENSE preserved. Its third_party/flatbuffers and gemmlowp LICENSE files and kissfft COPYING are preserved with their source headers. Preserve notices inside all bundled headers, including ruy.
- espressif/esp-nn 1.1.2: Apache-2.0; original LICENSE preserved.
- esphome/esp-micro-speech-features 1.2.3: original Apache-2.0 LICENSE preserved; source files also retain TensorFlow microfrontend notices. Only this microfrontend is selected.

Upstream examples and test applications are excluded, including model and WAV example inputs. Two locally reviewed replacement sources live separately under patches/ and preserve the upstream file notices. Their original versions remain in the pinned snapshots. Custom CMake registration never executes or includes the upstream component CMakeLists, preventing dependency-manager discovery or duplicate frontend selection.
