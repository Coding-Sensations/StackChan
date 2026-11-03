"""Synthetic bounded-ring backpressure coverage; no devices, calls, or audio capture."""
import ctypes
import unittest
from types import SimpleNamespace

from native_adapter import NativeHostAdapter
from native_conversation import NativeConversationSession, NativeVoiceBridge
from streaming_session import AudioBackpressure, SustainedAudioBackpressure, encode_frame


class NativeRing:
    """SPSC ring stand-in with the production 160 ms capacity."""
    capacity = 7680

    def __init__(self, queued=4864):
        self.queued = queued
        self.input_queued = 0
        self.push_calls = 0
        self.dropped = 0

    def sc_diagnostic(self, handle, metric):
        return {
            0: 0, 1: 0, 2: 0, 3: 0, 4: 0,
            5: self.queued, 6: self.input_queued,
        }.get(metric, 0)

    def sc_counter(self, handle, direction, metric):
        if direction == 0 and metric == 0:
            return self.dropped
        return 0

    def sc_error(self, handle):
        return 0

    def sc_begin(self, handle, epoch):
        return 0

    def sc_arm(self, handle, epoch):
        return 0

    def sc_push(self, handle, epoch, samples, frames):
        self.push_calls += 1
        accepted = min(frames, self.capacity - self.queued)
        self.queued += accepted
        self.dropped += frames - accepted
        return accepted

    def sc_pull(self, handle, epoch, samples, frames):
        return 0

    def sc_playing(self, handle, epoch, playing):
        return 0

    def sc_mute(self, handle, epoch):
        return 0

    def sc_close_checked(self, handle, retained):
        ctypes.cast(handle,ctypes.POINTER(ctypes.c_void_p))[0]=None
        ctypes.cast(retained,ctypes.POINTER(ctypes.c_uint32))[0]=0
        return 0


class Codec:
    def reset(self):
        pass

    def close(self):
        pass

    def decode(self, packet):
        return bytes(1920)  # one synthetic 60 ms mono16k frame

    def process(self, pcm):
        if len(pcm) == 1920:
            return bytes(5760)  # 60 ms mono48k mic frame
        return b''

    def encode(self, pcm):
        return b'synthetic-opus'


class Registry:
    def __init__(self):
        self.device = SimpleNamespace(id='synthetic', status='active', capabilities=('microphone', 'speaker'))

    def authenticate(self, token):
        return 'synthetic' if token == 'synthetic-token' else None

    def get(self, device_id):
        return self.device if device_id == 'synthetic' else None


class Sessions:
    def create(self, **kwargs):
        return SimpleNamespace(id='synthetic-conversation')

    def get_request(self, request_id):
        return None

    def begin_turn(self, conversation_id, request_id):
        return SimpleNamespace(conversation_id=conversation_id, id=request_id, generation=1)

    def accepts_output(self, *args):
        return True

    def interrupt_turn(self, *args):
        pass

    def mark_interrupt_uncertain(self, *args):
        pass


class NativeBackpressureTests(unittest.TestCase):
    def test_full_mic_packet_is_counted_and_session_survives_queue_pressure(self):
        ring = NativeRing(queued=4864)
        ring.dropped = 7  # native sc_counter units are also mono48k sample frames
        adapter = NativeHostAdapter.__new__(NativeHostAdapter)
        adapter.lib = ring
        adapter.handle = ctypes.c_void_p(1)
        adapter.epoch = 0
        adapter.muted = True
        adapter.playing = False
        adapter.uplink_backpressure_dropped = 0

        codec = Codec()
        bridge = NativeVoiceBridge(adapter, codec, codec, codec, codec, clock=lambda: 0)
        session = NativeConversationSession(
            Registry(), Sessions(), bridge, 'synthetic-token', 'synthetic',
            adapter.arm_for_approved_test, fixture=True, clock=lambda: 0,
        )
        try:
            session.begin('synthetic-turn')
            session.control({'type': 'ack', 'epoch': 1})
            session.control({'type': 'consent', 'epoch': 1, 'physical': True})

            # 4,864 queued + 2,880 packet samples exceeds the 7,680 bound by 64.
            session.receive(encode_frame(1, 0, False, b'synthetic-packet'))

            self.assertTrue(session.armed)
            self.assertEqual(session.mic_seq, 1)
            self.assertEqual(session.backpressure_mic_packets, 1)
            self.assertEqual(ring.push_calls, 0, 'reject the complete packet before partial enqueue')
            self.assertEqual(ring.queued, 4864)
            self.assertEqual(adapter.diagnostics()['output_dropped'], 2887)

            self.assertEqual(session.audio_tick(960), (False, []))
            self.assertTrue(session.armed, 'one bounded uplink drop must not close the session')
            stats = session.statistics()
            self.assertEqual(stats['backpressure_mic_packets'], 1)
            self.assertEqual(stats['native_audio']['output_dropped'], 2887)
        finally:
            session.close()
            bridge.close()

    def _session(self, ring):
        adapter = NativeHostAdapter.__new__(NativeHostAdapter)
        adapter.lib = ring
        adapter.handle = ctypes.c_void_p(1)
        adapter.epoch = 0
        adapter.muted = True
        adapter.playing = False
        adapter.uplink_backpressure_dropped = 0
        codec = Codec()
        bridge = NativeVoiceBridge(adapter, codec, codec, codec, codec, clock=lambda: 0)
        session = NativeConversationSession(
            Registry(), Sessions(), bridge, 'synthetic-token', 'synthetic',
            adapter.arm_for_approved_test, fixture=True, clock=lambda: 0,
        )
        session.begin('synthetic-turn')
        session.control({'type': 'ack', 'epoch': 1})
        session.control({'type': 'consent', 'epoch': 1, 'physical': True})
        return adapter, bridge, session

    def test_only_typed_queue_full_is_recoverable(self):
        adapter, bridge, session = self._session(NativeRing(queued=0))
        try:
            adapter.muted = True  # revoked native authority produces a generic False
            with self.assertRaises(BufferError) as failure:
                session.receive(encode_frame(1, 0, False, b'synthetic-packet'))
            self.assertNotIsInstance(failure.exception, AudioBackpressure)
            self.assertEqual(session.backpressure_mic_packets, 0)
        finally:
            session.close()
            bridge.close()

    def test_elapsed_full_queue_stall_interrupts_the_session(self):
        ring = NativeRing(queued=NativeRing.capacity)
        adapter, bridge, session = self._session(ring)
        clock=[0.];session.clock=lambda:clock[0]
        try:
            for sequence in range(6):
                clock[0]=sequence*.05
                session.receive(encode_frame(1, sequence, False, b'synthetic-packet'))
            self.assertTrue(session.armed)
            clock[0]=.301
            with self.assertRaises(SustainedAudioBackpressure):
                session.receive(encode_frame(1, session.mic_seq, False, b'synthetic-packet'))
            self.assertFalse(session.armed)
            self.assertEqual(session.backpressure_mic_packets, 7)
            self.assertEqual(session.mic_seq, 7)
            self.assertEqual(adapter.diagnostics()['output_dropped'], 7 * 2880)
            self.assertEqual(ring.push_calls, 0)
        finally:
            session.close()
            bridge.close()

    def test_successful_packet_breaks_consecutive_loss_streak(self):
        ring = NativeRing(queued=NativeRing.capacity)
        adapter, bridge, session = self._session(ring)
        try:
            session.receive(encode_frame(1, 0, False, b'synthetic-packet'))
            session.receive(encode_frame(1, 1, False, b'synthetic-packet'))
            self.assertEqual(session.backpressure_mic_streak, 2)

            ring.queued = 0
            session.receive(encode_frame(1, 2, False, b'synthetic-packet'))
            self.assertEqual(session.backpressure_mic_streak, 0)

            ring.queued = NativeRing.capacity
            session.receive(encode_frame(1, 3, False, b'synthetic-packet'))
            session.receive(encode_frame(1, 4, False, b'synthetic-packet'))
            self.assertTrue(session.armed)
            self.assertEqual(session.backpressure_mic_packets, 4)
            self.assertEqual(session.backpressure_mic_streak, 2)
        finally:
            session.close()
            bridge.close()

    def test_playback_suppression_resets_backpressure_streak(self):
        adapter, bridge, session = self._session(NativeRing(queued=NativeRing.capacity))
        try:
            session.receive(encode_frame(1, 0, False, b'synthetic-packet'))
            self.assertEqual(session.backpressure_mic_streak, 1)

            adapter.playing = True
            session.receive(encode_frame(1, 1, False, b'synthetic-packet'))
            adapter.playing = False
            self.assertEqual(session.suppressed_mic_packets, 1)
            self.assertEqual(session.backpressure_mic_streak, 0)

            for sequence in range(2, 6):
                session.receive(encode_frame(1, sequence, False, b'synthetic-packet'))
            self.assertTrue(session.armed)
            self.assertEqual(session.backpressure_mic_packets, 5)
            self.assertEqual(session.backpressure_mic_streak, 4)
        finally:
            session.close()
            bridge.close()


if __name__ == '__main__':
    unittest.main()
