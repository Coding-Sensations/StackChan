"""Production admission boundary; authenticates existing grants, never issues one.
No listener, deployment, persistence, provider call or firmware provisioning.
"""
from dataclasses import dataclass, field


@dataclass(frozen=True)
class AuthorizedDevice:
    device_id: str = field(repr=False)
    credential: str = field(repr=False)


def authorize_audio_request(registry, path, headers):
    """Use raw header pairs to reject duplicates before constructing a session.
    TLS trust and a qualified provider binding are separate caller requirements.
    Return values must not be logged or exposed back to an unauthenticated peer.
    """
    denied = 'audio admission denied'
    if path != '/companion/audio' or type(headers) not in (list, tuple) or not 1 <= len(headers) <= 64:
        raise PermissionError(denied)
    selected = {}
    total = 0
    for pair in headers:
        if type(pair) not in (tuple, list) or len(pair) != 2:
            raise PermissionError(denied)
        name, value = pair
        if type(name) is not str or type(value) is not str:
            raise PermissionError(denied)
        total += len(name.encode()) + len(value.encode())
        if total > 8192 or any(ord(c) < 32 or ord(c) == 127 for c in name + value):
            raise PermissionError(denied)
        name = name.lower()
        if name == 'x-stackchan-fixture':
            raise PermissionError(denied)
        if name in ('authorization', 'x-companion-device'):
            if name in selected:
                raise PermissionError(denied)
            selected[name] = value
    authorization = selected.get('authorization', '')
    device_id = selected.get('x-companion-device', '')
    if not authorization.startswith('Bearer '):
        raise PermissionError(denied)
    credential = authorization[7:]
    if not 1 <= len(credential) <= 256 or not 1 <= len(device_id) <= 100:
        raise PermissionError(denied)
    if any(not 33 <= ord(c) <= 126 for c in credential + device_id):
        raise PermissionError(denied)
    if registry.authenticate(credential) != device_id:
        raise PermissionError(denied)
    device = registry.get(device_id)
    if not device or device.status != 'active' or not {'microphone', 'speaker'} <= set(device.capabilities):
        raise PermissionError(denied)
    return AuthorizedDevice(device_id, credential)
