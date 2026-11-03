#!/bin/bash
set -euo pipefail
MODE="${1:-run}"
case "$MODE" in run|--compile-only) ;; *) echo "usage: $0 [--compile-only]" >&2; exit 2 ;; esac
UI_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_ROOT="${STACKCHAN_ROOT:-$(cd "$UI_ROOT/../../.." && pwd)}"
BUILD_ROOT="${STACKCHAN_OFFLINE_BUILD_ROOT:-$PROJECT_ROOT/.tools/numeric-ui/offline-tests}"
WEB_ROOT="$PROJECT_ROOT/companion/webcam"
mkdir -p "$BUILD_ROOT/module-cache"
for unit in CompanionSinkBridge WebcamGate; do
  /usr/bin/nice -n 20 /usr/bin/xcrun clang -fobjc-arc -fmodules -fmodules-cache-path="$BUILD_ROOT/module-cache" -mmacosx-version-min=14.0 -c "$WEB_ROOT/$unit.m" -o "$BUILD_ROOT/$unit.o"
done
for fixture in AudioEndpointDirectionChecks StatusChecks LifecycleChecks CompanionChecks RememberedDeviceChecks PersistentChecks MiraLifecycleChecks W1LifecycleChecks; do
  /usr/bin/nice -n 20 /usr/bin/xcrun swiftc -swift-version 5 -module-cache-path "$BUILD_ROOT/module-cache" -import-objc-header "$WEB_ROOT/Companion-Bridging-Header.h" "$UI_ROOT"/Models/*.swift "$UI_ROOT"/Services/*.swift "$UI_ROOT/Tests/FixtureCustody.swift" "$UI_ROOT/Tests/$fixture.swift" "$BUILD_ROOT/CompanionSinkBridge.o" "$BUILD_ROOT/WebcamGate.o" -framework CoreMediaIO -framework CoreMedia -framework CoreVideo -o "$BUILD_ROOT/$fixture"
  if [[ "$MODE" != "--compile-only" ]]; then "$BUILD_ROOT/$fixture"; fi
done

# Pure framing/open-intent fixtures use their own mocked pipeline boundary.
# Compile separately from the real app services to avoid duplicate type names.
/usr/bin/nice -n 20 /usr/bin/xcrun swiftc -swift-version 5 -module-cache-path "$BUILD_ROOT/module-cache" \
  "$UI_ROOT/Services/WebcamWorkerLineDecoder.swift" \
  "$UI_ROOT/Services/WebcamOwnedPipeAdapter.swift" \
  "$UI_ROOT/Services/WebcamOpenIntentLifecycle.swift" \
  "$PROJECT_ROOT/companion/host/streaming/line-decoder-tests/main.swift" \
  -o "$BUILD_ROOT/WebcamDecoderChecks"
if [[ "$MODE" != "--compile-only" ]]; then "$BUILD_ROOT/WebcamDecoderChecks"; fi
