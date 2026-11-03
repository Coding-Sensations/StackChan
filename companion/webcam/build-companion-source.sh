#!/bin/zsh
set -eu
base="${0:A:h}"
root="${base:h:h}"
ui="$root/companion/host/numeric-ui"
out="${STACKCHAN_WEBCAM_BUILD_ROOT:-$root/.tools/webcam-source}"
"$base/build-source.sh"
mkdir -p "$out/module-cache" "$out/StackChanPairing.app/Contents/MacOS"
xcrun clang -fobjc-arc -fmodules -fmodules-cache-path="$out/module-cache" -mmacosx-version-min=14.0 -c "$base/WebcamGate.m" -o "$out/WebcamGate.o"
xcrun swiftc -swift-version 5 -module-cache-path "$out/module-cache" -parse-as-library -import-objc-header "$base/Companion-Bridging-Header.h" "$ui"/Models/*.swift "$ui"/Services/*.swift "$ui"/Views/*.swift "$ui"/App/*.swift "$out/CompanionSinkBridge.o" "$out/WebcamGate.o" -framework CoreMediaIO -framework CoreMedia -framework CoreVideo -o "$out/StackChanPairing.app/Contents/MacOS/StackChanPairing"
cp "$root/companion/Companion.Info.plist" "$out/StackChanPairing.app/Contents/Info.plist"
# Unsigned build output only; Team/AppGroup placeholders remain unresolved.
# No installed app replacement, extension request, permission prompt or launch.
plutil -lint "$out/StackChanPairing.app/Contents/Info.plist"
