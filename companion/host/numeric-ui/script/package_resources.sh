#!/bin/bash
set -euo pipefail
TASK_ROOT="$1"
BUNDLE="$2"
mkdir -p "$BUNDLE/Contents/Resources"
/bin/cp "$TASK_ROOT/Resources/mira-idle-frame.png" "$BUNDLE/Contents/Resources/mira-idle-frame.png"
test "$(/usr/bin/shasum -a 256 "$BUNDLE/Contents/Resources/mira-idle-frame.png" | /usr/bin/awk '{print $1}')" = "93432bf8c86216c9c164d026ab96568cfd966a760a55e12da165af5a35e4ce79"
