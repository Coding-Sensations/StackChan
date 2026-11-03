"""Offline state mirror of an existing turn. No IO, dispatch, credential or capture.

The integration must validate physical events and device drain receipts before
calling this policy. `is_current` is supplied by the existing conversation owner;
this module never creates, renews or interrupts that owner's conversation.
"""
from collections import deque
from dataclasses import dataclass
from enum import Enum
import math
import time
from threading import RLock
from functools import wraps


def _locked(method):
    @wraps(method)
    def call(self, *args, **kwargs):
        with self._lock:
            return method(self, *args, **kwargs)
    return call


def _identifier(value):
    return (type(value) is str and 1 <= len(value) <= 128
            and all(c.isascii() and (c.isalnum() or c in '-_.:') for c in value))


def _uint(value, minimum=0):
    return type(value) is int and minimum <= value <= 0xffffffff


@dataclass(frozen=True)
class Binding:
    conversation_id: str
    turn_id: str
    generation: int
    epoch: int

    def __post_init__(self):
        if not (_identifier(self.conversation_id) and _identifier(self.turn_id)
                and _uint(self.generation, 1) and _uint(self.epoch, 1)):
            raise ValueError('bounded existing turn binding required')


class State(str, Enum):
    HARD_MUTED = 'hard_muted'
    LISTENING = 'listening'
    THINKING = 'thinking'
    SPEAKING = 'speaking'
    DRAINING = 'draining'
    CANCELLED = 'cancelled'
    DISCONNECTED = 'disconnected'


class TurnContract:
    """One fixed turn; cancellation/mute cannot resume it or mint a new one."""
    def __init__(self, binding, is_current, *, clock=time.monotonic):
        if type(binding) is not Binding or not callable(is_current) or not callable(clock):
            raise ValueError('existing owner and clock required')
        self._lock = RLock()
        self._binding = binding
        self._is_current = is_current
        self._clock = clock
        self._last_time = None
        self._state = State.HARD_MUTED
        self._invalid = False
        self._completed = False
        self._queue = deque()
        self._bytes = 0
        self._next_sequence = 0
        self._sent_sequence = -1
        self._final_sequence = None
        self._drain_until = None
        self._queue_until = None
        self._now()

    @property
    def binding(self):
        return self._binding

    @property
    @_locked
    def state(self):
        return self._state

    @property
    @_locked
    def queued_bytes(self):
        return self._bytes

    @property
    @_locked
    def capture_allowed(self):
        try:
            self._check(self.binding)
        except (PermissionError, ValueError, TimeoutError):
            return False
        return self._state == State.LISTENING

    def _now(self):
        try:
            now = self._clock()
            valid = (type(now) in (int, float) and math.isfinite(now)
                     and now >= 0 and (self._last_time is None or now >= self._last_time))
        except Exception:
            valid = False
        if not valid:
            self._invalidate(State.CANCELLED)
            raise ValueError('valid monotonic clock required')
        self._last_time = now
        return now

    def _invalidate(self, state):
        if self._invalid:
            return
        self._queue.clear()
        self._bytes = 0
        self._state = state
        self._invalid = True
        self._drain_until = None
        self._queue_until = None

    def _check(self, binding):
        if type(binding) is not Binding or binding != self.binding:
            raise PermissionError('foreign turn')
        if self._invalid:
            raise PermissionError('turn closed')
        try:
            current = self._is_current(binding) is True
        except Exception:
            current = False
        if not current:
            self._invalidate(State.HARD_MUTED)
            raise PermissionError('existing turn unavailable')
        now = self._now()
        if ((self._queue_until is not None and now >= self._queue_until)
                or (self._drain_until is not None and now >= self._drain_until)):
            self._invalidate(State.CANCELLED)
            raise TimeoutError('playback drain unconfirmed')
        return now

    @_locked
    def validate(self, binding):
        """Revalidate the existing owner atomically; never renew authority."""
        self._check(binding)
        return self._state

    @_locked
    def physical_listen(self, binding, *, physical):
        self._check(binding)
        if physical is not True or self._state != State.HARD_MUTED or self._completed:
            raise PermissionError('new physical Listen required')
        self._state = State.LISTENING

    @_locked
    def thinking(self, binding):
        self._check(binding)
        if self._state != State.LISTENING:
            raise PermissionError('listening turn required')
        self._state = State.THINKING

    @_locked
    def start_playback(self, binding):
        self._check(binding)
        if self._state != State.THINKING:
            raise PermissionError('thinking turn required')
        self._state = State.SPEAKING

    @_locked
    def enqueue(self, binding, sequence, payload):
        self._check(binding)
        if self._state != State.SPEAKING:
            raise PermissionError('playback closed')
        if not _uint(sequence) or sequence == 0xffffffff or type(payload) is not bytes or not 1 <= len(payload) <= 4096:
            raise ValueError('bounded playback packet required')
        if sequence != self._next_sequence:
            self._invalidate(State.CANCELLED)
            raise ValueError('playback sequence gap/replay')
        if len(self._queue) >= 8 or self._bytes + len(payload) > 32768:
            raise BufferError('playback backpressure')
        self._queue.append((sequence, payload))
        self._bytes += len(payload)
        self._next_sequence += 1

    @_locked
    def take(self, binding):
        self._check(binding)
        if self._state not in (State.SPEAKING, State.DRAINING):
            raise PermissionError('playback inactive')
        if not self._queue:
            return None
        sequence, payload = self._queue.popleft()
        self._bytes -= len(payload)
        self._sent_sequence = sequence
        if self._final_sequence == sequence:
            self._queue_until = None
            self._drain_until = self._last_time + 2.0
        return sequence, payload

    @_locked
    def end_playback(self, binding, final_sequence):
        now = self._check(binding)
        if self._state != State.SPEAKING or type(final_sequence) is not int or final_sequence != self._next_sequence - 1:
            raise PermissionError('correlated playback end required')
        self._final_sequence = final_sequence
        if self._queue:
            self._queue_until = now + 4.0
        else:
            self._drain_until = now + 2.0
        self._state = State.DRAINING

    @_locked
    def drained(self, binding, final_sequence):
        self._check(binding)
        if (type(final_sequence) is not int or self._final_sequence is None
                or final_sequence != self._final_sequence):
            raise PermissionError('foreign drain receipt')
        if self._completed:
            return False
        if (self._state != State.DRAINING or self._queue
                or self._sent_sequence != final_sequence):
            raise PermissionError('playback still pending')
        self._completed = True
        self._drain_until = None
        self._state = State.HARD_MUTED
        return True

    @_locked
    def hard_mute(self):
        self._invalidate(State.HARD_MUTED)

    @_locked
    def cancel(self):
        self._invalidate(State.CANCELLED)

    @_locked
    def disconnect(self):
        self._invalidate(State.DISCONNECTED)


class MemoryContext:
    """Optional RAM-only view for an existing owner; never a durable memory store."""
    def __init__(self, *, persona='Be concise, curious and clear. Use your own original voice.'):
        if type(persona) is not str or not persona.strip() or len(persona.encode('utf-8')) > 2048:
            raise ValueError('bounded original persona required')
        self._lock = RLock()
        self._persona = persona
        self._turns = deque()
        self._bytes = 0

    @property
    def persona(self):
        return self._persona

    @property
    @_locked
    def size_bytes(self):
        return self._bytes

    @_locked
    def append(self, role, text):
        if type(role) is not str or role not in ('user', 'assistant') or type(text) is not str or not text.strip():
            raise ValueError('bounded text turn required')
        size = len(text.encode('utf-8'))
        if size > 4096:
            raise ValueError('bounded text turn required')
        self._turns.append((role, text, size))
        self._bytes += size
        while len(self._turns) > 20 or self._bytes > 32768:
            self._bytes -= self._turns.popleft()[2]

    @_locked
    def snapshot(self):
        return tuple((role, text) for role, text, _ in self._turns)

    @_locked
    def clear(self):
        self._turns.clear()
        self._bytes = 0
