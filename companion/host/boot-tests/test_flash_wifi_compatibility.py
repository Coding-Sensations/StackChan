#!/usr/bin/env python3
from pathlib import Path
import json
import unittest

from flash_wifi_compat import analyze, manifest_preserves_nvs, target_supports_config_blob

REPO = Path(__file__).resolve().parents[3]
MANIFEST_PATH = REPO / "companion/MINIMAL-FLASH-MANIFEST.json"
FLASHED_COMMIT = "c2480f20f55a494dead17be70d74db54c9781d9c"


class FlashWifiCompatibilityTests(unittest.TestCase):
    def test_manifest_preserves_nvs(self):
        manifest = json.loads(MANIFEST_PATH.read_text())
        self.assertTrue(manifest_preserves_nvs(manifest))

    def test_current_head_supports_config_blob(self):
        self.assertTrue(target_supports_config_blob(REPO, "HEAD"))

    def test_flashed_commit_requires_legacy_export(self):
        self.assertFalse(target_supports_config_blob(REPO, FLASHED_COMMIT))

    def test_preserved_nvs_flash_to_legacy_only_target_is_rejected_without_migration(self):
        manifest = json.loads(MANIFEST_PATH.read_text())
        result = analyze(REPO, manifest, FLASHED_COMMIT, migration_performed=False)
        self.assertFalse(result.compatible)
        self.assertTrue(result.requires_legacy_export)
        self.assertIn("legacy ssid/password", result.reason)

    def test_preserved_nvs_flash_to_legacy_only_target_passes_with_migration(self):
        manifest = json.loads(MANIFEST_PATH.read_text())
        result = analyze(REPO, manifest, FLASHED_COMMIT, migration_performed=True)
        self.assertTrue(result.compatible)
        self.assertTrue(result.requires_legacy_export)


if __name__ == "__main__":
    unittest.main()
