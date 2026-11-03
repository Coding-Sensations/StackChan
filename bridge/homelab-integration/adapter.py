"""Offline HomeLab integration. No listeners, robot drivers or provider calls.

Import the real HomeLab modules and existing PCM fixture buffer via PYTHONPATH.
The JSON audio envelope is a local test format, not the factory wire protocol.
Single owner; externally supplied sessions/journal must be in-memory fixtures.
"""
from dataclasses import dataclass
from datetime import UTC, datetime
import base64
import json
import math
from uuid import uuid4

from audio import AudioBuffer
from home_lab.contracts import Approval, Device
from home_lab.events import EventJournal
from home_lab.sessions import ConversationSessions

FIRMWARE_REVISION = '1b5765599fba8aaad1811d9a79358ccc7051f5f3'
TOOL_SCHEMAS = {
    'camera.snapshot': {
        'capability': 'camera',
        'inputSchema': {'type': 'object', 'additionalProperties': False,
            'required': ['width', 'height', 'max_bytes'],
            'properties': {
                'width': {'type': 'integer', 'minimum': 1, 'maximum': 320},
                'height': {'type': 'integer', 'minimum': 1, 'maximum': 240},
                'max_bytes': {'type': 'integer', 'minimum': 1024, 'maximum': 262144},
            }},
        'description': 'Plan one snapshot only; no capture, upload or streaming implementation.',
    },
    'motion.relative': {
        'capability': 'motor',
        'inputSchema': {'type': 'object', 'additionalProperties': False,
            'required': ['yaw_delta_deg', 'pitch_delta_deg', 'duration_ms', 'ttl_ms'],
            'properties': {
                'yaw_delta_deg': {'type': 'number', 'minimum': -15, 'maximum': 15},
                'pitch_delta_deg': {'type': 'number', 'minimum': -15, 'maximum': 15},
                'duration_ms': {'type': 'integer', 'minimum': 100, 'maximum': 2000},
                'ttl_ms': {'type': 'integer', 'minimum': 100, 'maximum': 5000},
            }},
        'description': 'Fixture relative motion bounds; not qualified factory coordinates or limits.',
    },
}


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('duplicate frame field')
        result[key] = value
    return result


@dataclass(frozen=True)
class AudioFrame:
    device_id: str
    conversation_id: str
    turn_id: str
    generation: int
    sequence: int
    pcm: bytes
    codec: str = 'pcm16le'
    sample_rate: int = 16000
    channels: int = 1

    def __post_init__(self):
        for value in (self.device_id, self.conversation_id, self.turn_id):
            if not isinstance(value, str) or not value.strip() or len(value) > 128:
                raise ValueError('frame identity must be a bounded nonempty string')
        if type(self.generation) is not int or self.generation < 1:
            raise ValueError('invalid generation')
        if type(self.sequence) is not int or not 0 <= self.sequence <= 2147483647:
            raise ValueError('invalid sequence')
        if (self.codec != 'pcm16le' or type(self.sample_rate) is not int or self.sample_rate != 16000
                or type(self.channels) is not int or self.channels != 1):
            raise ValueError('only PCM16LE mono 16kHz fixture format is supported')
        if type(self.pcm) is not bytes or not self.pcm or len(self.pcm) % 2 or len(self.pcm) > 1280:
            raise ValueError('PCM fixture chunk must contain 1..640 samples (at most 40ms)')

    def to_wire(self):
        values = dict(self.__dict__)
        values['pcm_base64'] = base64.b64encode(values.pop('pcm')).decode('ascii')
        return json.dumps(values, separators=(',', ':'), allow_nan=False).encode('utf-8')

    @classmethod
    def from_wire(cls, payload):
        if type(payload) is not bytes or not 1 <= len(payload) <= 4096:
            raise ValueError('frame payload must be bounded to 4096 bytes')
        try:
            values = json.loads(payload, object_pairs_hook=_unique_object)
            fields = {'device_id','conversation_id','turn_id','generation','sequence',
                      'pcm_base64','codec','sample_rate','channels'}
            if type(values) is not dict or set(values) != fields:
                raise ValueError('unexpected frame fields')
            encoded = values.pop('pcm_base64')
            if type(encoded) is not str or len(encoded) > 1708:
                raise ValueError('oversized PCM encoding')
            return cls(pcm=base64.b64decode(encoded, validate=True), **values)
        except (TypeError, UnicodeError, ValueError) as error:
            raise ValueError('invalid fixture audio frame') from error


class OfflineAdapter:
    def __init__(self, device: Device, sessions: ConversationSessions,
                 journal: EventJournal, capacity=8):
        if device.status != 'offline_fixture':
            raise ValueError('explicit offline fixture device required')
        # Refuse accidental use of a persisted/live HomeLab object here.
        if getattr(sessions, '_store', None) is not None or getattr(journal, '_store', None) is not None:
            raise ValueError('offline integration requires in-memory HomeLab state')
        self.device, self.sessions, self.journal = device, sessions, journal
        self.conversation = sessions.create(connection_id='same-dot.unsupported', device_id=device.id)
        self.audio = AudioBuffer(capacity)
        self.input_audio = AudioBuffer(capacity)
        self.turn = None
        self._audio_generation = None

    def _event(self, kind, **data):
        return self.journal.append(source_id='stackchan.offline', event_type=kind,
            correlation_id=self.turn.request_id, occurred_at=datetime.now(UTC),
            conversation_id=self.turn.conversation_id, turn_id=self.turn.id, data=data)

    def _active(self):
        return self.turn is not None and self.sessions.accepts_output(
            self.turn.conversation_id, self.turn.id, self.turn.generation)

    def begin(self, request_id, now):
        self.audio._time(now)
        self.input_audio._time(now)
        old = self.sessions.get_request(request_id)
        if old is not None:
            if old != self.turn or not self._active():
                raise ValueError('request cannot reopen a stale or foreign turn')
            return old
        turn = self.sessions.begin_turn(self.conversation.id, request_id=request_id)
        self._audio_generation = self.audio.start_turn(now)
        self._input_generation = self.input_audio.start_turn(now)
        self.turn = turn
        self._event('bridge.turn_started', generation=turn.generation, offline=True)
        return turn

    def push(self, frame: AudioFrame, now):
        if ('speaker' not in self.device.capabilities or not self._active()
                or frame.device_id != self.device.id
                or frame.conversation_id != self.turn.conversation_id
                or frame.turn_id != self.turn.id or frame.generation != self.turn.generation):
            return False
        return self.audio.push(self._audio_generation, frame.sequence, frame.pcm, now)

    def push_input(self, frame: AudioFrame, now):
        if ('microphone' not in self.device.capabilities or not self._active()
                or frame.device_id != self.device.id
                or frame.conversation_id != self.turn.conversation_id
                or frame.turn_id != self.turn.id or frame.generation != self.turn.generation):
            return False
        return self.input_audio.push(self._input_generation, frame.sequence, frame.pcm, now)

    def pop_input(self, now):
        """Return fixture bytes only; never capture a mic or submit to an agent."""
        if not self._active():
            self.input_audio.interrupt(now)
            return None
        return self.input_audio.pop(now)

    def pop(self, now):
        if not self._active():
            self.audio.interrupt(now)
            return None
        return self.audio.pop(now)

    def interrupt(self, now):
        self.audio._time(now)
        self.input_audio._time(now)  # Validate both before any state mutation.
        self.audio.interrupt(now)
        self.input_audio.interrupt(now)
        if self.turn is not None and self._active():
            self.sessions.interrupt_turn(self.turn.conversation_id, self.turn.id)
            self.sessions.mark_interrupt_uncertain(self.turn.conversation_id, self.turn.id)
            self._event('bridge.interrupted', speaker_stop='unsupported', agent_cancellation='not_requested')
        return {'queue_cleared': True, 'speaker_stop': 'unsupported', 'agent_cancellation': 'not_requested'}

    def plan_tool(self, name, arguments):
        if not self._active():
            raise PermissionError('active fixture turn required')
        if name not in TOOL_SCHEMAS:
            raise ValueError('unknown fixture tool')
        tool = TOOL_SCHEMAS[name]
        if tool['capability'] not in self.device.capabilities:
            raise PermissionError('fixture capability unavailable')
        schema = tool['inputSchema']
        if type(arguments) is not dict or set(arguments) != set(schema['required']):
            raise ValueError('tool fields must exactly match schema')
        for key, value in arguments.items():
            rule = schema['properties'][key]
            numeric = type(value) is int if rule['type'] == 'integer' else type(value) in (int, float)
            if not numeric or not rule['minimum'] <= value <= rule['maximum'] or not math.isfinite(value):
                raise ValueError('tool argument outside offline fixture bounds')
        if name == 'motion.relative' and arguments['ttl_ms'] < arguments['duration_ms']:
            raise ValueError('motion expires before its requested duration')
        approval = Approval(id=str(uuid4()), operation=name, target=self.device.id,
            revision=FIRMWARE_REVISION, conversation_id=self.turn.conversation_id, turn_id=self.turn.id)
        return {'tool': name, 'arguments': dict(arguments), 'approval': approval,
                'executable': False, 'reason': 'offline schema only; hardware and live route unqualified'}

    def connect_same_dot(self):
        raise NotImplementedError('No supported same-dot streaming transport is implemented; no fallback assistant')
