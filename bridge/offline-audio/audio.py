"""Offline PCM boundary. No robot or dot transport is implemented.

Single-owner use only. PCM fixture format: signed 16-bit little-endian,
mono, 16 kHz, at most 40 ms per chunk. Actual transport codec negotiation
must happen before integration; these defaults are not a robot claim.
"""
from collections import deque
import math


class AudioBuffer:
    def __init__(self, capacity=8):
        if type(capacity) is not int or not 1 <= capacity <= 256:
            raise ValueError('capacity must be an integer in 1..256')
        self.capacity = capacity
        self._queue = deque()
        self._generation = 0
        self._sequence = 0
        self._started = None
        self._last_time = None
        self.first_output_ms = None

    def _time(self, now):
        if not math.isfinite(now) or (self._last_time is not None and now < self._last_time):
            raise ValueError('timestamp must be finite and monotonic')

    def start_turn(self, now):
        self._time(now)
        self._generation += 1
        self._queue.clear()
        self._sequence = 0
        self._started = now
        self._last_time = now
        self.first_output_ms = None
        return self._generation

    def interrupt(self, now):
        """Clear queued PCM and invalidate any in-flight response chunks.

        A future playback driver must separately stop already-submitted audio;
        clearing this queue cannot silence an actual speaker by itself.
        """
        return self.start_turn(now)

    def push(self, generation, sequence, pcm, now):
        if type(generation) is not int or generation != self._generation or self._started is None:
            return False
        self._time(now)
        if type(sequence) is not int or sequence != self._sequence:
            raise ValueError('chunk sequence is not contiguous')
        if not isinstance(pcm, bytes) or not pcm or len(pcm) % 2 or len(pcm) > 1280:
            raise ValueError('expected 1..640 mono PCM16 samples')
        if len(self._queue) >= self.capacity:
            raise BufferError('playback queue full; producer must apply backpressure')
        self._queue.append(pcm)
        self._sequence += 1
        self._last_time = now
        return True

    def pop(self, now):
        self._time(now)
        self._last_time = now
        if not self._queue:
            return None
        if self.first_output_ms is None:
            self.first_output_ms = (now - self._started) * 1000
        return self._queue.popleft()
