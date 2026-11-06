#!/usr/bin/env python3
"""Check whether an app-only flash preserves saved Wi-Fi for the target image."""
from __future__ import annotations

from dataclasses import asdict, dataclass
from pathlib import Path
import argparse
import json
import subprocess

REPO = Path(__file__).resolve().parents[3]
FLASHED_COMMIT = "c2480f20f55a494dead17be70d74db54c9781d9c"
MANIFEST = REPO / "companion/MINIMAL-FLASH-MANIFEST.json"


@dataclass
class FlashWifiCompatibility:
    target_commit: str
    preserves_nvs: bool
    target_supports_config_blob: bool
    requires_legacy_export: bool
    migration_performed: bool
    compatible: bool
    reason: str


def _git_grep(repo: Path, revision: str, pattern: str) -> bool:
    result = subprocess.run(
        [
            "git",
            "-C",
            str(repo),
            "--no-pager",
            "grep",
            "-q",
            pattern,
            revision,
            "--",
            "firmware/main",
        ],
        text=True,
        capture_output=True,
    )
    if result.returncode in (0, 1):
        return result.returncode == 0
    raise RuntimeError(result.stderr.strip() or f"git grep failed for {revision}: {pattern}")


def manifest_preserves_nvs(manifest: dict) -> bool:
    untouched = manifest.get("untouched_by_explicit_writes", [])
    return any(str(entry).startswith("NVS") for entry in untouched)


def target_supports_config_blob(repo: Path, revision: str) -> bool:
    return _git_grep(repo, revision, "config_v1")


def analyze(repo: Path, manifest: dict, target_commit: str, migration_performed: bool = False) -> FlashWifiCompatibility:
    preserves_nvs = manifest_preserves_nvs(manifest)
    supports_blob = target_supports_config_blob(repo, target_commit)
    requires_legacy_export = preserves_nvs and not supports_blob
    if not preserves_nvs:
        compatible = True
        reason = "Flash does not preserve NVS, so saved Wi-Fi continuity is outside this check."
    elif not requires_legacy_export:
        compatible = True
        reason = "Target image understands the versioned Wi-Fi blob; preserved saved Wi-Fi remains boot-compatible."
    elif migration_performed:
        compatible = True
        reason = "Target image depends on legacy ssid/password keys, but migration was recorded before the preserved-NVS flash."
    else:
        compatible = False
        reason = "Target image does not show config_v1 support. Export/read back legacy ssid/password keys before any preserved-NVS app-only flash."
    return FlashWifiCompatibility(
        target_commit=target_commit,
        preserves_nvs=preserves_nvs,
        target_supports_config_blob=supports_blob,
        requires_legacy_export=requires_legacy_export,
        migration_performed=migration_performed,
        compatible=compatible,
        reason=reason,
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=REPO)
    parser.add_argument("--manifest", type=Path, default=MANIFEST)
    parser.add_argument("--target-commit", default=FLASHED_COMMIT)
    parser.add_argument("--migration-performed", action="store_true")
    args = parser.parse_args()

    manifest = json.loads(args.manifest.read_text())
    result = analyze(args.repo, manifest, args.target_commit, args.migration_performed)
    print(json.dumps(asdict(result), indent=2))
    return 0 if result.compatible else 1


if __name__ == "__main__":
    raise SystemExit(main())
