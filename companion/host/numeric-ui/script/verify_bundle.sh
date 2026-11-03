#!/bin/bash
set -euo pipefail
BUNDLE="${1:?usage: verify_bundle.sh path/to/StackChanPairing.app}"
/usr/bin/plutil -lint "$BUNDLE/Contents/Info.plist"
test -x "$BUNDLE/Contents/MacOS/StackChanPairing"
# The linker signature alone does not bind the completed app's Info.plist.
/usr/bin/codesign --verify --deep --strict --verbose=2 "$BUNDLE"
