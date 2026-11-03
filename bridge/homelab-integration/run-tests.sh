#!/bin/sh
set -eu
bridge_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
home_lab_root=${HOME_LAB_ROOT:?Set HOME_LAB_ROOT to the reviewed compatible checkout}
expected=c0d1181b235bf99fd03f71ec49c9dbae76007ef4
actual=$(git -C "$home_lab_root" rev-parse HEAD)
[ "$actual" = "$expected" ] || { echo 'HomeLab revision differs; review compatibility before testing.' >&2; exit 1; }
git -C "$home_lab_root" diff --quiet HEAD -- src/home_lab/contracts.py src/home_lab/sessions.py src/home_lab/events.py src/home_lab/storage.py
export PYTHONDONTWRITEBYTECODE=1
export PYTHONPATH="$home_lab_root/src:$bridge_root/offline-audio:$bridge_root/homelab-integration"
python3 -m unittest discover -s "$bridge_root/offline-audio" -v
python3 -m unittest discover -s "$bridge_root/homelab-integration" -v
python3 -m unittest discover -s "$bridge_root/transport" -v
python3 "$bridge_root/homelab-integration/demo.py"
