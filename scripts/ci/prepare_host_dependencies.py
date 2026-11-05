#!/usr/bin/env python3
"""Fetch only pinned source inputs for inert host fixtures, not an ESP-IDF SDK."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
from urllib.request import urlopen

ROOT = Path(__file__).resolve().parents[2]
FIRMWARE = ROOT / 'firmware'


def module(path):
    spec = importlib.util.spec_from_file_location(path.stem, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


backend = module(FIRMWARE / 'main/apps/stock_agent/generate_backend.py')
teardown = json.loads((FIRMWARE / 'main/apps/runtime_lifecycle/teardown_pins.json').read_text())
# Exact revisions from firmware/repos.json and IDF component registry provenance.
SOURCES = [
    ('78/xiaozhi-esp32', '49b4380b3faa0ceadd28a3e78aaf0f311c46c57e', 'xiaozhi-esp32',
     {'main/' + name: backend.PINNED[name] for name in (
         'device_state_machine.cc', 'protocols/websocket_protocol.cc',
         'protocols/mqtt_protocol.cc', 'application.cc', 'ota.cc', 'boards/common/wifi_board.cc')}),
    ('78/esp-wifi-connect', 'a7b34167750f0188486b489240268d0f7c306caf', 'managed_components/78__esp-wifi-connect',
     {
      'dns_server.cc': '8e6f03dc39f7386a149e227c756a53e3f715454b1d024c7ce38b75cdf00045d1',
      'wifi_manager.cc': '3346377f3595774d8a8d07b259f4860a9aabaf323e39fba6c1cac598e44d5d20',
      'wifi_configuration_ap.cc': '05e2d1fe46ce57de433078f287accfc06df771c326a23b9eac894d6657a465c3',
      'ssid_manager.cc': 'e3441e0fc84012b539bd08b680cdfbfdaca5eb55af55423400268d0a18d5daca',
      'include/ssid_manager.h': 'f45a0687170a30ba5592a9b3fde7c8f5da768dac7de185b256b2be56359d9e3f'}),
    ('78/esp-ml307', '39638f2a4e514e12ef5d8afc7241db5b9b394086', 'managed_components/78__esp-ml307',
     {('src/web_socket.cc' if name == 'web_socket.cc' else 'src/esp/' + name): digest
      for name, digest in teardown.items() if name != 'espnow.c'}),
    ('espressif/esp-now', 'c33383de97f3ed2fc52117f5ef04f71990432e5d', 'components/esp-now',
     {'src/espnow/src/espnow.c': '4db54a4cd22731a62009475eb8102eb460a247268e6791b0813178d7c808711c'}),
]


def main():
    with tempfile.TemporaryDirectory(prefix='stackchan-pinned-host-') as temporary:
        stage = Path(temporary)
        verified = []
        for repository, revision, destination, files in SOURCES:
            for relative, expected in files.items():
                output = stage / destination / relative
                output.parent.mkdir(parents=True, exist_ok=True)
                url = f'https://raw.githubusercontent.com/{repository}/{revision}/{relative}'
                with urlopen(url, timeout=30) as response:
                    output.write_bytes(response.read())
            if destination == 'xiaozhi-esp32':
                # Only these patched units are consumed by these fixtures. Other
                # firmware patch units are deliberately not fetched or built.
                subprocess.run(['git', 'apply', '--include=main/application.cc', '--include=main/ota.cc',
                                str(FIRMWARE / 'patches/xiaozhi-esp32.patch')],
                               cwd=stage / destination, check=True)
            for relative, expected in files.items():
                source = stage / destination / relative
                if hashlib.sha256(source.read_bytes()).hexdigest() != expected:
                    raise SystemExit(f'Unreviewed host dependency: {repository}/{relative}')
                target = FIRMWARE / destination / relative
                if any(part.is_symlink() for part in (target, *target.parents)) or (target.exists() and target.read_bytes() != source.read_bytes()):
                    raise SystemExit(f'Refusing to replace different local dependency: {target}')
                verified.append((source, target))
        # Never install a partially verified source set.
        for source, target in verified:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(source.read_bytes())
        # Newer lifecycle fixtures compile the real Mooncake sources too.
        mooncake = FIRMWARE / 'components/mooncake'
        expected = '572a7e488641f1c3f22e85d65c27f095ecf27d12'
        if mooncake.exists():
            raise SystemExit(f'Refusing to replace local Mooncake source: {mooncake}')
        mooncake.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(['git', 'init', str(mooncake)], check=True)
        subprocess.run(['git', '-C', str(mooncake), 'fetch', '--depth=1',
                        'https://github.com/Forairaaaaa/mooncake.git', expected], check=True)
        subprocess.run(['git', '-C', str(mooncake), 'checkout', '--detach', 'FETCH_HEAD'], check=True)
        actual = subprocess.check_output(['git', '-C', str(mooncake), 'rev-parse', 'HEAD'], text=True).strip()
        if actual != expected:
            raise SystemExit('Mooncake source revision mismatch')
        print(f'Prepared {len(verified)} pinned source files and Mooncake {actual}; no SDK or device IO')


if __name__ == '__main__':
    main()
