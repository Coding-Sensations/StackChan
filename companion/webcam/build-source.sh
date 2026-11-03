#!/bin/zsh
set -eu
base="${0:A:h}"
root="${base:h:h}"
out="${STACKCHAN_WEBCAM_BUILD_ROOT:-$root/.tools/webcam-source}"
mkdir -p "$out/module-cache"
flags=(-fobjc-arc -fmodules -fmodules-cache-path="$out/module-cache" -mmacosx-version-min=12.3 -Wall -Wextra)
xcrun clang $flags -framework Foundation -framework CoreMediaIO -framework CoreMedia -framework CoreVideo -framework Security "$base/CameraExtension.m" "$base/WebcamGate.m" -o "$out/CameraExtension-source-only"
xcrun clang $flags -c "$base/CompanionSinkBridge.m" -o "$out/CompanionSinkBridge.o"
xcrun swiftc -module-cache-path "$out/swift-cache" -emit-module -parse-as-library -target arm64-apple-macosx12.3 "$base/WebcamControls.swift" -o "$out/WebcamControls.swiftmodule"
# Compile/link only. No extension execution, signing, installation or activation.
