#!/bin/zsh
set -eu
base="${0:A:h:h}"
ui="$base/source/companion/host/numeric-ui"
out="$base/evidence/one-app-source"
mkdir -p "$out/module-cache"
xcrun swiftc -swift-version 5 -module-cache-path "$out/module-cache" -import-objc-header "$base/source/Companion-Bridging-Header.h" "$ui"/Models/*.swift "$ui"/Services/*.swift "$base/tests/WebcamUIChecks.swift" "$base/evidence/CompanionSinkBridge.o" "$out/WebcamGate.o" -framework CoreMediaIO -framework CoreMedia -framework CoreVideo -o "$out/WebcamUIChecks" > "$base/evidence/webcam-ui-test-build.log" 2>&1
"$out/WebcamUIChecks" > "$base/evidence/webcam-ui-checks.log" 2>&1

# Existing regression fixtures launch only injected synthetic stdout workers.
for fixture in LifecycleChecks CompanionChecks; do
  xcrun swiftc -swift-version 5 -module-cache-path "$out/module-cache" -import-objc-header "$base/source/Companion-Bridging-Header.h" "$ui"/Models/*.swift "$ui"/Services/*.swift "$ui/Tests/$fixture.swift" "$base/evidence/CompanionSinkBridge.o" "$out/WebcamGate.o" -framework CoreMediaIO -framework CoreMedia -framework CoreVideo -o "$out/$fixture" > "$base/evidence/$fixture-build.log" 2>&1
  "$out/$fixture" > "$base/evidence/$fixture-checks.log" 2>&1
done
