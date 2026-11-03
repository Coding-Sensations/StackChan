"""Test the production resource packaging step without compiling or launching an app."""
from pathlib import Path
import hashlib
import subprocess
import tempfile
import unittest

UI = Path(__file__).resolve().parents[1] / "numeric-ui"

class BundleResources(unittest.TestCase):
    def test_reviewed_image_is_packaged_and_changed_bytes_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            bundle = root / "Companion.app"
            subprocess.run(["/bin/bash", str(UI / "script/package_resources.sh"), str(UI), str(bundle)], check=True)
            data = (bundle / "Contents/Resources/mira-idle-frame.png").read_bytes()
            self.assertEqual(hashlib.sha256(data).hexdigest(), "93432bf8c86216c9c164d026ab96568cfd966a760a55e12da165af5a35e4ce79")
            source = root / "altered"
            (source / "Resources").mkdir(parents=True)
            (source / "Resources/mira-idle-frame.png").write_bytes(b"synthetic changed image")
            rejected = subprocess.run(["/bin/bash", str(UI / "script/package_resources.sh"), str(source), str(bundle)], check=False)
            self.assertNotEqual(rejected.returncode, 0)
