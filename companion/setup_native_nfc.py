#!/usr/bin/env python3
"""Fetch immutable official M5 NFC components; verify archives and existing files.
No installers execute. Existing mismatches are refused rather than overwritten.
"""
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import tarfile
import tempfile
import urllib.request


def digest(data):
    return hashlib.sha256(data).hexdigest()


def setup(root):
    pins = json.loads((root / 'companion/NFC-VENDOR-PINS.json').read_text())
    components = root / 'firmware/components'
    components.mkdir(parents=True, exist_ok=True)
    for name, pin in pins.items():
        assert pin['repo'] == 'm5stack/' + name
        expected_url = 'https://codeload.github.com/' + pin['repo'] + '/tar.gz/' + pin['sha']
        assert pin['archive_url'] == expected_url
        with urllib.request.urlopen(expected_url, timeout=30) as response:
            archive = response.read(pin['archive_bytes'] + 1)
        if len(archive) != pin['archive_bytes'] or digest(archive) != pin['archive_sha256']:
            raise ValueError('Archive verification failed: ' + name)
        files = {}
        total = 0
        with tarfile.open(fileobj=io.BytesIO(archive), mode='r:gz') as tar:
            for member in tar:
                parts = PurePosixPath(member.name).parts
                if not parts or parts[0] != name + '-' + pin['sha'] or '..' in parts or member.name.startswith('/'):
                    raise ValueError('Unsafe archive path')
                if member.isdir():
                    continue
                if not member.isfile() or len(parts) < 2:
                    raise ValueError('Unsupported archive member')
                total += member.size
                if total > 20_000_000 or member.size > 5_000_000:
                    raise ValueError('Archive exceeds extraction bound')
                path = Path(*parts[1:])
                if path in files:
                    raise ValueError('Duplicate archive member')
                files[path] = tar.extractfile(member).read()
        license_data = files.get(Path('LICENSE'))
        if license_data is None or digest(license_data) != pin['license_sha256']:
            raise ValueError('License verification failed: ' + name)
        destination = components / name
        if destination.exists():
            if destination.is_symlink():
                raise ValueError('Existing symlink refused: ' + str(destination))
            actual = set()
            for entry in destination.rglob('*'):
                if entry.is_symlink():
                    raise ValueError('Existing symlink refused: ' + str(entry))
                if entry.is_file():
                    actual.add(entry.relative_to(destination))
                elif not entry.is_dir():
                    raise ValueError('Unsupported existing entry: ' + str(entry))
            if actual != set(files):
                raise ValueError('Existing component file set mismatch: ' + name)
            for path, data in files.items():
                existing = destination / path
                if existing.is_symlink() or not existing.is_file() or digest(existing.read_bytes()) != digest(data):
                    raise ValueError('Existing component mismatch: ' + str(existing))
            print(name + ': verified existing ' + pin['sha'])
            continue
        with tempfile.TemporaryDirectory(prefix='nfc-stage-', dir=components) as stage:
            staged = Path(stage) / name
            staged.mkdir()
            for path, data in files.items():
                target = staged / path
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
            staged.rename(destination)
        print(name + ': installed verified ' + pin['sha'])


if __name__ == '__main__':
    setup(Path(__file__).resolve().parents[1])
