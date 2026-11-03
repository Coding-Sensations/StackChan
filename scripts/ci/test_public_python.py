"""Standalone public fixtures with synthetic owner-only trust pins; no network/device IO."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
host = root / 'companion/host'
with tempfile.TemporaryDirectory(prefix='stackchan-synthetic-ci-') as temporary:
    pins = Path(temporary) / 'bindings.json'
    pins.write_text(json.dumps(dict(device='stackchan-001122334455', host='192.168.42.10', port=18443,
        uri='wss://192.168.42.10:18443/companion/audio', ca_sha256='a'*64, expiry=2000000000,
        cert_sha256='b'*64, cert_begin=1900000000, cert_end=2000000000,
        native_library_sha256='c'*64)))
    pins.chmod(0o600)
    env = dict(os.environ, STACKCHAN_BINDING_CONFIG=str(pins), PYTHONDONTWRITEBYTECODE='1',
               PYTHONPATH=str(host))
    result = subprocess.run([sys.executable, '-m', 'unittest', 'test_numeric_pairing_worker',
                            'test_capture_timing_contract', 'test_joined_webcam_delivery', 'test_webcam_worker_emitter', '-v'],
                            cwd=host / 'streaming', env=env, timeout=120)
    raise SystemExit(result.returncode)
