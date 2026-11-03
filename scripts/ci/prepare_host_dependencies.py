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
    ('78/xiaozhi-esp32', 'e77dedb1309153bb63fed285772962c920c97dd4', 'xiaozhi-esp32',
     {'main/' + name: backend.PINNED[name] for name in (
         'device_state_machine.cc', 'protocols/websocket_protocol.cc',
         'protocols/mqtt_protocol.cc', 'application.cc', 'ota.cc', 'boards/common/wifi_board.cc')}),
    ('78/esp-wifi-connect', 'a7b34167750f0188486b489240268d0f7c306caf', 'managed_components/78__esp-wifi-connect',
     {name: backend.PINNED[name] for name in (
         'dns_server.cc', 'wifi_manager.cc', 'wifi_configuration_ap.cc', 'ssid_manager.cc')}),
    ('78/esp-ml307', 'ab4de7c28c8b8f809eba2f56f38090d57fce984d', 'managed_components/78__esp-ml307',
     {('src/web_socket.cc' if name == 'web_socket.cc' else 'src/esp/' + name): digest
      for name, digest in teardown.items() if name != 'espnow.c'}),
    ('espressif/esp-now', 'c33383de97f3ed2fc52117f5ef04f71990432e5d', 'components/esp-now',
     {'src/espnow/src/espnow.c': teardown['espnow.c']}),
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
                                str(FIRMWARE / 'patches/xiaozhi-e77dedb.patch')],
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
