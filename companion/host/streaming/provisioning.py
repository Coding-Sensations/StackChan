"""Volatile USB provisioning envelope using existing HomeLab pairing authority.
No serial IO or real credential issuance occurs unless an authorized caller invokes
redeem. No provider/persona is selected here; route qualification is a caller gate.
"""
import struct
from dataclasses import dataclass, field
from streaming_session import require_production_tls

HEADER = struct.Struct('>4sBH')
LIMITS = (256, 100, 256, 128)
MAX_FRAME = 755


def pack_grant(uri, device, credential, request):
    require_production_tls(uri)
    values = (uri, device, credential, request)
    chunks = []
    for value, limit in zip(values, LIMITS):
        if type(value) is not str or not 1 <= len(value) <= limit or any(not 33 <= ord(c) <= 126 for c in value):
            raise ValueError('bounded provisioning field required')
        raw = value.encode('ascii')
        chunks.append(struct.pack('>H', len(raw)) + raw)
    body = b''.join(chunks)
    return HEADER.pack(b'SCPG', 1, len(body)) + body


def unpack_grant(frame):
    if type(frame) is not bytes or not 19 <= len(frame) <= MAX_FRAME:
        raise ValueError('bounded provisioning frame required')
    magic, version, length = HEADER.unpack(frame[:7])
    if magic != b'SCPG' or version != 1 or length != len(frame)-7:
        raise ValueError('provisioning header rejected')
    values = []; position = 7
    for limit in LIMITS:
        if position + 2 > len(frame):
            raise ValueError('provisioning length rejected')
        size = struct.unpack_from('>H', frame, position)[0]; position += 2
        if not 1 <= size <= limit or position + size > len(frame):
            raise ValueError('provisioning length rejected')
        try:
            values.append(frame[position:position+size].decode('ascii'))
        except UnicodeDecodeError:
            raise ValueError('provisioning encoding rejected') from None
        position += size
    if position != len(frame) or pack_grant(*values) != frame:
        raise ValueError('provisioning fields rejected')
    return tuple(values)


@dataclass(frozen=True)
class ProvisioningResult:
    frame: bytes = field(repr=False)
    device_id: str = field(repr=False)


class ProvisioningBroker:
    """Reuses DeviceRegistry; no separate pairing/token database or owner listener.
    Live calls require explicit owner authorization and independently qualified
    route. Neither flag may be taken from an untrusted request or grant payload.
    """
    def __init__(self, registry, endpoint):
        require_production_tls(endpoint)
        self.registry = registry; self.endpoint = endpoint

    def redeem(self, code, *, device_id, label, request_id, owner_authorized=False, route_qualified=False):
        if owner_authorized is not True or route_qualified is not True:
            raise PermissionError('owner approval and qualified route required')
        if type(code) is not str or not 1 <= len(code) <= 128:
            raise ValueError('bounded pairing code required')
        # Validate all framing fields before consuming the one-use pairing code.
        pack_grant(self.endpoint, device_id, 'validation-only', request_id)
        device, credential = self.registry.redeem_pairing_code(
            code, device_id=device_id, label=label, capabilities=('microphone', 'speaker'))
        return ProvisioningResult(pack_grant(self.endpoint, device.id, credential, request_id), device.id)
