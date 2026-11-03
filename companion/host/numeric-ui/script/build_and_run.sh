#!/bin/bash
set -euo pipefail
MODE="${1:-run}"
TASK_ROOT="$(CDPATH= cd -- "$(dirname "${BASH_SOURCE[0]}")/.." >/dev/null && pwd -P)"
PROJECT_ROOT="${STACKCHAN_ROOT:-$(CDPATH= cd -- "$TASK_ROOT/../../.." >/dev/null && pwd -P)}"
PROJECT_ROOT="$(cd -- "$PROJECT_ROOT" >/dev/null && pwd -P)"
BUILD_ROOT="${STACKCHAN_UI_BUILD_ROOT:-$PROJECT_ROOT/.tools/numeric-ui}"
if [[ "$MODE" == "--compile-only" && -z "${STACKCHAN_UI_BUILD_ROOT:-}" ]]; then
  BUILD_ROOT="$PROJECT_ROOT/.tools/numeric-ui/source-only"
fi
BUNDLE="$BUILD_ROOT/StackChanPairing.app"
if [[ "$MODE" == "--compile-only" && -e "$BUNDLE/Contents/_CodeSignature" ]]; then
  echo "Compile-only requires an output bundle without existing signature metadata; select a fresh STACKCHAN_UI_BUILD_ROOT" >&2
  exit 2
fi
case "$MODE" in
  run|--compile-only|--build-only|--verify|--debug|--logs|--telemetry) ;;
  *) echo "usage: $0 [run|--compile-only|--build-only|--verify|--debug|--logs|--telemetry]" >&2; exit 2 ;;
esac
mkdir -p "$BUNDLE/Contents/MacOS" "$BUNDLE/Contents/Resources" "$BUILD_ROOT/module-cache"
/bin/bash "$TASK_ROOT/script/package_resources.sh" "$TASK_ROOT" "$BUNDLE"
WEB_ROOT="$PROJECT_ROOT/companion/webcam"
for unit in CompanionSinkBridge WebcamGate; do
  /usr/bin/nice -n 20 /usr/bin/xcrun clang -fobjc-arc -fmodules -fmodules-cache-path="$BUILD_ROOT/module-cache" -mmacosx-version-min=14.0 -c "$WEB_ROOT/$unit.m" -o "$BUILD_ROOT/$unit.o"
done
/usr/bin/nice -n 20 /usr/bin/xcrun swiftc -swift-version 5 -module-cache-path "$BUILD_ROOT/module-cache" -parse-as-library -import-objc-header "$WEB_ROOT/Companion-Bridging-Header.h" "$TASK_ROOT"/Models/*.swift "$TASK_ROOT"/Services/*.swift "$TASK_ROOT"/Views/*.swift "$TASK_ROOT"/App/*.swift "$BUILD_ROOT/CompanionSinkBridge.o" "$BUILD_ROOT/WebcamGate.o" -framework CoreMediaIO -framework CoreMedia -framework CoreVideo -o "$BUNDLE/Contents/MacOS/StackChanPairing"
cat > "$BUNDLE/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?><plist version="1.0"><dict>
<key>CFBundleExecutable</key><string>StackChanPairing</string>
<key>CFBundleIdentifier</key><string>local.mrkillbob.StackChanPairing</string>
<key>CFBundleName</key><string>StackChan Companion</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>LSMinimumSystemVersion</key><string>14.0</string>
<key>NSPrincipalClass</key><string>NSApplication</string>
<key>NSMicrophoneUsageDescription</key><string>StackChan Companion reads your selected virtual audio output to play it on StackChan after physical Listen.</string>
</dict></plist>
PLIST
/usr/bin/plutil -insert StackChanRoot -string "$PROJECT_ROOT" "$BUNDLE/Contents/Info.plist"
if [[ "$MODE" == "--compile-only" ]]; then
  /usr/bin/plutil -lint "$BUNDLE/Contents/Info.plist"
  echo "Unsigned source build complete; not launched: $BUNDLE"
  exit 0
fi
# Sign only after the completed bundle exists, so metadata/resources are bound.
/usr/bin/codesign --force --sign - "$BUNDLE"
/bin/bash "$TASK_ROOT/script/verify_bundle.sh" "$BUNDLE"
case "$MODE" in
  --build-only) ;;
  --verify)
    /usr/bin/open "$BUNDLE"
    # Check this exact artifact, rather than any app with the same process name.
    # Window inspection and close/reopen checks remain a separate UI check.
    /bin/sleep 1
    for attempt in {1..20}; do
      if /bin/ps -axo command= | /usr/bin/awk -v binary="$BUNDLE/Contents/MacOS/StackChanPairing" '$0 == binary || index($0, binary " ") == 1 { found=1 } END { exit !found }'; then
        echo "Verified normal launch: $BUNDLE (idle; enrollment remains manual)"
        exit 0
      fi
      /bin/sleep 0.25
    done
    echo "Bundle passed signing checks but normal launch did not remain running: $BUNDLE" >&2
    exit 1 ;;
  run) /usr/bin/open "$BUNDLE" ;;
  --debug) /usr/bin/xcrun lldb -- "$BUNDLE/Contents/MacOS/StackChanPairing" ;;
  --logs|--telemetry) /usr/bin/open "$BUNDLE" ;;
esac
