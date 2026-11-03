"""Local approved pins, never supplied by a pairing request.

No config means unavailable. A private owner-only JSON file is selected with
STACKCHAN_BINDING_CONFIG; changes require restarting this process. It contains
public trust pins and paths only, never keys, certificates or credentials.
"""
import ipaddress
import hashlib
import json
import os
from pathlib import Path
import re
from urllib.parse import urlsplit

ROOT = Path(os.environ.get('STACKCHAN_ROOT', str(Path(__file__).resolve().parents[3]))).resolve()
HOME_LAB_ROOT = Path(os.environ.get('HOME_LAB_ROOT', '/nonexistent/stackchan-home-lab'))
PROFILE = ROOT / '.tools/persistent-companion/profile.json'
LIBRARY = ROOT / 'companion/host/native-audio/build/libstackchan_audio_checked.dylib'
NATIVE_AUDIO_BACKEND = 'legacy_bridge_v1'
NATIVE_AUDIO_TEST_SECONDS = None
DEVICE = HOST = URI = CA_HASH = CERT_HASH = LIBRARY_HASH = ''
PORT = EXPIRY = CERT_BEGIN = CERT_END = 0
_ERROR = None


def _hash(value):
    return type(value) is str and re.fullmatch('[a-f0-9]{64}', value) is not None


def _load(path):
    path = Path(path)
    if path.is_symlink() or not path.is_file() or path.stat().st_size > 8192 or path.stat().st_uid != os.getuid() or path.stat().st_mode & 0o077:
        raise ValueError('private binding file required')
    def pairs(values):
        result = {}
        for key, value in values:
            if key in result: raise ValueError('duplicate binding')
            result[key] = value
        return result
    value = json.loads(path.read_text(), object_pairs_hook=pairs)
    required = {'device', 'host', 'port', 'uri', 'ca_sha256', 'expiry', 'cert_sha256', 'cert_begin', 'cert_end', 'native_library_sha256'}
    directional = {'native_audio_backend', 'native_library_path'}
    if type(value) is not dict or set(value) not in (required, required | directional, required | directional | {'native_audio_test_seconds'}): raise ValueError('invalid binding fields')
    if directional <= set(value):
        if value['native_audio_backend'] != 'directional_hal_v2': raise ValueError('invalid native backend')
        if 'native_audio_test_seconds' in value and (type(value['native_audio_test_seconds']) is not int or not 1 <= value['native_audio_test_seconds'] <= 15): raise ValueError('bounded native test duration required')
        text = value['native_library_path']
        if type(text) is not str or not 1 <= len(text) <= 1024 or any(ord(c) < 32 for c in text): raise ValueError('invalid native library path')
        library = Path(text)
        if not library.is_absolute() or '..' in library.parts or str(library) != text: raise ValueError('absolute native library path required')
    if type(value['device']) is not str or not re.fullmatch('stackchan-[a-f0-9]{12}', value['device']): raise ValueError('invalid device pin')
    host = ipaddress.IPv4Address(value['host'])
    if str(host) != value['host'] or not any(host in ipaddress.IPv4Network(n) for n in ('10.0.0.0/8', '172.16.0.0/12', '192.168.0.0/16')): raise ValueError('invalid host pin')
    if type(value['port']) is not int or value['port'] != 18443: raise ValueError('invalid port pin')
    if value['uri'] != f"wss://{host}:{value['port']}/companion/audio": raise ValueError('invalid endpoint pin')
    if not all(_hash(value[k]) for k in ('ca_sha256', 'cert_sha256', 'native_library_sha256')): raise ValueError('invalid hash pin')
    if any(type(value[k]) is not int for k in ('expiry', 'cert_begin', 'cert_end')) or not 0 < value['expiry'] <= 0xffffffff or not 0 < value['cert_begin'] < value['cert_end'] <= 0xffffffff: raise ValueError('invalid expiry pin')
    return value


if os.environ.get('STACKCHAN_BINDING_CONFIG'):
    try:
        _pins = _load(os.environ['STACKCHAN_BINDING_CONFIG'])
        DEVICE, HOST, PORT, URI = (_pins[k] for k in ('device', 'host', 'port', 'uri'))
        CA_HASH, EXPIRY = _pins['ca_sha256'], _pins['expiry']
        CERT_HASH, CERT_BEGIN, CERT_END = (_pins[k] for k in ('cert_sha256', 'cert_begin', 'cert_end'))
        LIBRARY_HASH = _pins['native_library_sha256']
        if _pins.get('native_audio_backend') == 'directional_hal_v2':
            NATIVE_AUDIO_BACKEND = 'directional_hal_v2'
            NATIVE_AUDIO_TEST_SECONDS = _pins.get('native_audio_test_seconds')
            LIBRARY = Path(_pins['native_library_path'])
    except (ValueError, OSError, TypeError):
        _ERROR = 'approved binding unavailable'


def require():
    if _ERROR or not DEVICE or not HOST or not URI or not _hash(CA_HASH) or not EXPIRY:
        raise ValueError('approved binding unavailable')


def require_native():
    require()
    if not _hash(LIBRARY_HASH) or not os.environ.get('HOME_LAB_ROOT'):
        raise ValueError('approved native dependency unavailable')


def require_server_certificate(data, begin, end):
    """Require the owner-bound exact server certificate before loading its key."""
    require()
    if (type(data) is not bytes or not 1 <= len(data) <= 8192
            or not _hash(CERT_HASH) or hashlib.sha256(data).hexdigest() != CERT_HASH
            or type(begin) is not int or type(end) is not int
            or begin != CERT_BEGIN or end != CERT_END):
        raise ValueError('unpinned server certificate')
