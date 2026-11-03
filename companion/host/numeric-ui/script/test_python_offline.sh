#!/bin/bash
# Offline Python suites for the Companion host (streaming/). Excludes test_remote_gateway.py,
# whose listener fixtures bind localhost; run it separately only with approval.
set -euo pipefail
UI_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HOST_ROOT="$(cd "$UI_ROOT/.." && pwd)"
PROJECT_ROOT="$(cd "$UI_ROOT/../../.." && pwd)"
PYTHON="${STACKCHAN_PYTHON:-python3}"
: "${HOME_LAB_ROOT:?Set HOME_LAB_ROOT to the approved private source checkout}"
HOME_LAB_ROOT="$(cd "$HOME_LAB_ROOT" && pwd)"
if [[ ! -d "$HOME_LAB_ROOT/src/home_lab" ]]; then
  echo "HOME_LAB_ROOT must contain src/home_lab" >&2
  exit 2
fi
export PYTHONDONTWRITEBYTECODE=1
export PYTHONPATH="$HOME_LAB_ROOT/src:$PROJECT_ROOT/bridge/host-adapter:$PROJECT_ROOT/bridge/local-endpoint:$HOST_ROOT/native-audio:$HOST_ROOT/streaming:$HOST_ROOT"
cd "$HOST_ROOT/streaming"
modules=()
# STACKCHAN_SKIP_MODULES: space-separated module names to skip when their third-party
# dependency (pyserial, the HomeLab desk_robot package) is not installed. Skips are printed.
skip=" test_remote_gateway ${STACKCHAN_SKIP_MODULES:-} "
for f in test_*.py; do
  if [[ "$skip" == *" ${f%.py} "* ]]; then echo "SKIPPED ${f%.py}" >&2; continue; fi
  modules+=("${f%.py}")
done
if [[ ${#modules[@]} -eq 0 ]]; then
  echo "No Python test modules selected; refusing implicit discovery" >&2
  exit 2
fi
exec "$PYTHON" -B -m unittest "${modules[@]}"
