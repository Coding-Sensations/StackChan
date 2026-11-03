import dataclasses
import json
import unittest

try:
    from adapter import AudioFrame, OfflineAdapter, TOOL_SCHEMAS
except ImportError:
    AudioFrame = OfflineAdapter = TOOL_SCHEMAS = None

from home_lab.contracts import Approval, Device, Event, Turn
from home_lab.events import EventJournal
from home_lab.sessions import ConversationSessions


class IntegrationTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(OfflineAdapter, 'offline HomeLab adapter not implemented')
        self.sessions = ConversationSessions()
        self.events = EventJournal()
        self.device = Device(id='robot-fixture', label='Offline robot',
                             capabilities=('speaker', 'camera', 'motor'), status='offline_fixture')
        self.bridge = OfflineAdapter(self.device, self.sessions, self.events, capacity=2)
        self.turn = self.bridge.begin('request-1', 1.0)

    def frame(self, **kwargs):
        values = dict(device_id=self.device.id, conversation_id=self.turn.conversation_id,
                      turn_id=self.turn.id, generation=self.turn.generation, sequence=0, pcm=b'\0\0'*320)
        return AudioFrame(**(values | kwargs))

    def test_reuses_session_request_dedupe_without_resetting_audio(self):
        self.assertIsInstance(self.turn, Turn)
        self.assertTrue(self.bridge.push(self.frame(), 1.1))
        self.assertEqual(self.bridge.begin('request-1', 1.2), self.turn)
        self.assertEqual(self.bridge.pop(1.3), self.frame().pcm)

    def test_cross_device_and_cross_conversation_frames_are_rejected(self):
        self.assertFalse(self.bridge.push(self.frame(device_id='other'), 1.1))
        self.assertFalse(self.bridge.push(self.frame(conversation_id='other'), 1.1))
        self.assertIsNone(self.bridge.pop(1.2))

    def test_superseded_turn_is_rejected_even_when_generation_matches(self):
        old = self.frame()
        new = self.bridge.begin('request-2', 1.1)
        self.assertEqual(new.generation, self.turn.generation)
        self.assertFalse(self.bridge.push(old, 1.2))

    def test_interrupt_clears_queue_and_reports_unconfirmed_physical_stop(self):
        self.bridge.push(self.frame(), 1.1)
        result = self.bridge.interrupt(1.2)
        self.assertTrue(result['queue_cleared'])
        self.assertEqual(result['speaker_stop'], 'unsupported')
        self.assertEqual(result['agent_cancellation'], 'not_requested')
        self.assertFalse(self.bridge.push(self.frame(), 1.3))
        self.assertIsNone(self.bridge.pop(1.4))
        cursor, events = self.events.after_cursor(0)
        self.assertGreater(cursor, 0)
        self.assertIsInstance(events[-1], Event)
        self.assertEqual(events[-1].turn_id, self.turn.id)
        self.assertEqual(self.sessions.get_turn(self.turn.id).status, 'uncertain')

    def test_invalid_interrupt_time_does_not_invalidate_home_lab_turn(self):
        with self.assertRaises(ValueError):
            self.bridge.interrupt(float('nan'))
        self.assertTrue(self.sessions.accepts_output(self.turn.conversation_id, self.turn.id, self.turn.generation))

    def test_audio_backpressure_and_invalid_sequence(self):
        self.bridge.push(self.frame(), 1.1)
        with self.assertRaises(ValueError):
            self.bridge.push(self.frame(sequence=2), 1.2)
        self.bridge.push(self.frame(sequence=1), 1.2)
        with self.assertRaises(BufferError):
            self.bridge.push(self.frame(sequence=2), 1.3)
        self.assertEqual(self.bridge.pop(1.4), self.frame().pcm)

    def test_terminal_session_output_is_dropped_before_playback(self):
        self.bridge.push(self.frame(), 1.1)
        self.sessions.finish_turn(self.turn.conversation_id, self.turn.id, 'completed')
        self.assertIsNone(self.bridge.pop(1.2))
        self.assertFalse(self.bridge.push(self.frame(), 1.3))

    def test_fixture_audio_wire_roundtrip_and_decode_bounds(self):
        frame = self.frame()
        self.assertEqual(AudioFrame.from_wire(frame.to_wire()), frame)
        for payload in (b' ' * 4097, b'{"unexpected":1}', b'{"x":1,"x":2}'):
            with self.assertRaises(ValueError):
                AudioFrame.from_wire(payload)
        for changes in ({'pcm':b'x'}, {'pcm':b'x'*1282}, {'sample_rate':48000}, {'generation':True}):
            with self.assertRaises(ValueError):
                self.frame(**changes)

    def test_camera_plan_is_single_bounded_request_and_pending_approval(self):
        plan = self.bridge.plan_tool('camera.snapshot', {'width':320, 'height':240, 'max_bytes':262144})
        self.assertFalse(plan['executable'])
        self.assertIsInstance(plan['approval'], Approval)
        self.assertEqual(plan['approval'].status, 'pending')
        self.assertEqual(plan['approval'].turn_id, self.turn.id)
        with self.assertRaises(ValueError):
            self.bridge.plan_tool('camera.snapshot', {'width':640, 'height':240, 'max_bytes':262144})
        with self.assertRaises(ValueError):
            self.bridge.plan_tool('camera.snapshot', {'width':320, 'height':240, 'max_bytes':1024, 'stream':True})

    def test_motion_plan_refuses_nonfinite_out_of_bounds_and_extra_authority(self):
        args = {'yaw_delta_deg':5.0, 'pitch_delta_deg':-3.0, 'duration_ms':500, 'ttl_ms':1000}
        self.assertFalse(self.bridge.plan_tool('motion.relative', args)['executable'])
        for changes in ({'yaw_delta_deg':15.1}, {'pitch_delta_deg':float('nan')}, {'ttl_ms':5001}, {'duration_ms':True}, {'laser':True}):
            with self.assertRaises(ValueError):
                self.bridge.plan_tool('motion.relative', args | changes)

    def test_tools_require_active_turn_and_declared_fixture_capability(self):
        b = OfflineAdapter(dataclasses.replace(self.device, capabilities=()), ConversationSessions(), EventJournal())
        b.begin('new', 1.0)
        with self.assertRaises(PermissionError):
            b.plan_tool('camera.snapshot', {'width':320,'height':240,'max_bytes':1024})
        self.bridge.interrupt(1.2)
        with self.assertRaises(PermissionError):
            self.bridge.plan_tool('camera.snapshot', {'width':320,'height':240,'max_bytes':1024})

    def test_live_boundary_refuses_activation_and_real_device_claims(self):
        with self.assertRaises(NotImplementedError):
            self.bridge.connect_same_dot()
        with self.assertRaises(ValueError):
            OfflineAdapter(dataclasses.replace(self.device, status='active'), ConversationSessions(), EventJournal())
        self.assertEqual(set(TOOL_SCHEMAS), {'camera.snapshot','motion.relative'})


class AdditionalBoundaries(unittest.TestCase):
    def test_tool_huge_number_is_rejected_as_invalid_input(self):
        self.assertIsNotNone(OfflineAdapter, 'adapter required')
        b = OfflineAdapter(Device(id='fixture',label='Fixture',status='offline_fixture',capabilities=('motor',)), ConversationSessions(), EventJournal())
        b.begin('r', 0.0)
        with self.assertRaises(ValueError):
            b.plan_tool('motion.relative', {'yaw_delta_deg':10**1000,'pitch_delta_deg':0,'duration_ms':100,'ttl_ms':100})

    def test_playback_drops_turn_superseded_outside_adapter(self):
        self.assertIsNotNone(OfflineAdapter, 'adapter required')
        sessions=ConversationSessions()
        b=OfflineAdapter(Device(id='fixture',label='Fixture',status='offline_fixture',capabilities=('speaker',)), sessions, EventJournal())
        t=b.begin('r',0.0)
        b.push(AudioFrame(device_id='fixture',conversation_id=t.conversation_id,turn_id=t.id,generation=t.generation,sequence=0,pcm=b'\0\0'),0.1)
        sessions.begin_turn(t.conversation_id,request_id='external')
        self.assertIsNone(b.pop(0.2))


class InputFramingTests(unittest.TestCase):
    def test_microphone_fixture_is_bounded_and_cleared_on_interrupt(self):
        self.assertTrue(hasattr(OfflineAdapter,'push_input'), 'mic framing missing')
        b=OfflineAdapter(Device(id='mic-fixture',label='Mic fixture',status='offline_fixture',capabilities=('microphone',)), ConversationSessions(), EventJournal(),capacity=1)
        t=b.begin('mic-request',0.0)
        f=AudioFrame(device_id=b.device.id,conversation_id=t.conversation_id,turn_id=t.id,generation=t.generation,sequence=0,pcm=b'\0\0')
        self.assertTrue(b.push_input(f,0.1))
        with self.assertRaises(BufferError):
            b.push_input(dataclasses.replace(f,sequence=1),0.2)
        self.assertEqual(b.pop_input(0.3), f.pcm)
        self.assertFalse(b.push(f,0.4))  # Device has no speaker.
        self.assertTrue(b.push_input(dataclasses.replace(f,sequence=1),0.4))
        b.interrupt(0.5)
        self.assertIsNone(b.pop_input(0.6))
        self.assertFalse(b.push_input(dataclasses.replace(f,sequence=2),0.7))

    def test_missing_microphone_capability_refuses_capture_fixture(self):
        self.assertTrue(hasattr(OfflineAdapter,'push_input'), 'mic framing missing')
        b=OfflineAdapter(Device(id='speaker-fixture',label='Fixture',status='offline_fixture',capabilities=('speaker',)), ConversationSessions(), EventJournal())
        t=b.begin('request',0.0)
        f=AudioFrame(device_id=b.device.id,conversation_id=t.conversation_id,turn_id=t.id,generation=t.generation,sequence=0,pcm=b'\0\0')
        self.assertFalse(b.push_input(f,0.1))


if __name__ == '__main__':
    unittest.main()
