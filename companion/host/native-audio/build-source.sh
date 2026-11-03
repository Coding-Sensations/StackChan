#!/bin/bash
set -euo pipefail
base="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
out="${STACKCHAN_NATIVE_BUILD_ROOT:-$base/build}"
mkdir -p "$out"
xcrun clang++ -std=c++17 -Wall -Wextra -Werror -dynamiclib "$base/coreaudio_bridge.cpp" -framework AudioUnit -framework CoreAudio -framework CoreFoundation -o "$out/libstackchan_audio_checked.dylib"
# Compile/link only. Loading, sc_open and sc_start remain explicit operations.
shasum -a 256 "$out/libstackchan_audio_checked.dylib"
