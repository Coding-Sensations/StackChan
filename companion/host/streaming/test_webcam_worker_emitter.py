"""Pure synthetic contracts only; no runtime, IO, subprocess or native imports."""
import base64
import json
import math
import unittest
from types import SimpleNamespace as NS

from webcam_transport import (
    FRAME_BYTES, HEADER, MAX_U64, MappedCapture, VideoFrame, WebcamAssembler,
    WebcamPurpose, encode_fragments,
)
from webcam_worker_emitter import BoundedWebcamWorkerEmitter


class Tests(unittest.TestCase):
    def setUp(self):
        self.now = 10.
        self.current = True
        self.mapped = MappedCapture(9.99, .001)
        self.p = WebcamPurpose('synthetic-device', 'synthetic-conversation', 'synthetic-turn', 7, 9)
        self.e = self.make()

    def make(self, p=None, **overrides):
        args = dict(current=lambda p: self.current,
                    map_capture_clock=lambda p, ns: self.mapped,
                    clock=lambda: self.now)
        args.update(overrides)
        return BoundedWebcamWorkerEmitter(p or self.p, **args)

    def frame(self, sequence=1, **overrides):
        args = dict(purpose=self.p, sequence=sequence, host_captured=9.99,
                    bgra=bytes(range(256)) * 1200, producer_capture_ns=123456789)
        args.update(overrides)
        return VideoFrame(**args)

    def offer(self, frame=None, **kwargs):
        if 'ns' in kwargs:
            self.e.offer(frame or self.frame(), producer_capture_ns=kwargs['ns'])
        else:
            self.e.offer(frame or self.frame())

    def pull(self, **kwargs):
        return self.e.next_event(controls_drained=kwargs.get('control', True),
                                 audio_drained=kwargs.get('audio', True))

    def decode(self, event):
        self.assertLessEqual(len(event) + 1, 8192)
        obj = json.loads(event)
        self.assertEqual(set(obj), {'type', 'wire'})
        self.assertEqual(obj['type'], 'webcam_fragment')
        self.assertLessEqual(len(obj['wire']), 5484)
        wire = base64.b64decode(obj['wire'], validate=True)
        self.assertLessEqual(len(wire), 4112)
        return wire

    def consumer(self, purpose=None):
        p = purpose or self.p
        session = NS(_authorized=lambda: None, _active=lambda: True,
                     device_id=p.device, conversation=NS(id=p.conversation),
                     turn=NS(id=p.turn, generation=p.generation))
        assembler = WebcamAssembler(session, lambda s, p: self.current,
                                    lambda s, p, ns: self.mapped, clock=lambda: self.now)
        assembler.explicit_on(p, local_intent=True)
        return assembler

    def test_schema_offsets_complete_roundtrip_synthetic_fixed_clock(self):
        self.offer()
        consumer = self.consumer()
        offset = 0
        count = 0
        while (event := self.pull()) is not None:
            wire = self.decode(event)
            h = HEADER.unpack_from(wire)
            self.assertEqual(HEADER.size, 56)
            self.assertEqual(h[:10], (b'WCV1', 1, 1, 320, 240, 7, 9, 1, 123456789, FRAME_BYTES))
            self.assertEqual(h[10], offset)
            end = 56 + sum(h[11:14])
            self.assertEqual(wire[56:end], (self.p.device+self.p.conversation+self.p.turn).encode())
            offset += len(wire)-end
            consumer.push(wire)
            count += 1
        self.assertEqual(offset, FRAME_BYTES)
        self.assertGreater(count, 70)
        self.assertEqual(consumer.take_latest(), self.frame())
        self.assertIsNone(self.pull())

    def test_controls_audio_have_priority_and_do_not_consume_fragments(self):
        self.offer()
        for flags in ({'control': False}, {'audio': False}, {'control': 1}, {'audio': 'yes'}):
            self.assertIsNone(self.pull(**flags))
        self.assertEqual(HEADER.unpack_from(self.decode(self.pull()))[10], 0)

    def test_latest_replaces_partial_old_frame_and_only_latest_completes(self):
        self.offer()
        c = self.consumer()
        c.push(self.decode(self.pull()))
        newer = self.frame(2, bgra=b'Z'*FRAME_BYTES)
        self.offer(newer)
        while (event := self.pull()) is not None:
            c.push(self.decode(event))
        self.assertEqual(c.take_latest(), newer)
        self.assertIsNone(c.take_latest())

    def test_revoke_drops_pending_and_is_irreversible(self):
        self.offer()
        self.e.revoke()
        self.assertIsNone(self.pull())
        with self.assertRaises(PermissionError):
            self.offer(self.frame(2))

    def test_expiry_after_first_fragment_revokes_without_completing(self):
        self.offer()
        self.assertIsNotNone(self.pull())
        self.now = 10.3
        with self.assertRaises(PermissionError):
            self.pull()
        self.assertIsNone(self.pull())

    def test_nominal_20ms_tick_cannot_qualify_full_frame(self):
        self.offer()
        emitted = 0
        with self.assertRaises(PermissionError):
            while True:
                self.assertIsNotNone(self.pull())
                emitted += 1
                self.now += .02
        self.assertLess(emitted * 4112, FRAME_BYTES)

    def test_replay_out_of_order_and_boolean_sequence_fail_closed(self):
        for sequence in (1, 0, True, MAX_U64+1):
            with self.subTest(sequence=sequence):
                self.e = self.make()
                self.offer()
                with self.assertRaises(ValueError):
                    self.offer(self.frame(sequence))
                self.assertIsNone(self.pull())

    def test_foreign_purpose_and_pixel_bounds_reject_before_mapping(self):
        def forbidden(*args):
            raise AssertionError('invalid frame reached capture mapper')
        for frame in (self.frame(purpose=WebcamPurpose('foreign','c','t',7,9)),
                      self.frame(bgra=b'x'), self.frame(bgra=bytearray(FRAME_BYTES)),
                      self.frame(host_captured=math.nan)):
            self.e = self.make(map_capture_clock=forbidden)
            with self.assertRaises(ValueError):
                self.offer(frame)

    def test_capture_integer_bounds_and_mapping_faults(self):
        for ns in (0, True, -1, MAX_U64+1):
            self.e = self.make()
            with self.assertRaises(ValueError):
                self.offer(ns=ns)
        for mapped in (None, MappedCapture(10.1, 0), MappedCapture(9.98, 0),
                       MappedCapture(9.99, .026), MappedCapture(9.99, math.nan),
                       MappedCapture(9.99, True)):
            self.e = self.make()
            self.mapped = mapped
            with self.assertRaises(PermissionError):
                self.offer()

    def test_current_loss_and_callback_exceptions_revoke_pending(self):
        self.offer()
        self.current = False
        with self.assertRaises(PermissionError):
            self.pull(control=False)
        self.assertIsNone(self.pull())
        self.current = True
        def fault(p, ns):
            raise RuntimeError('synthetic mapper fault')
        self.e = self.make(map_capture_clock=fault)
        with self.assertRaises(RuntimeError):
            self.offer()
        self.assertIsNone(self.pull())

    def test_reentrant_revoke_during_mapping_cannot_publish(self):
        def cancel(p, ns):
            self.e.revoke()
            return self.mapped
        self.e = self.make(map_capture_clock=cancel)
        with self.assertRaises(PermissionError):
            self.offer()
        self.assertIsNone(self.pull())

    def test_clock_regression_nonfinite_and_boolean_reject(self):
        for now in (9., math.nan, math.inf, True):
            self.now = 10.
            self.e = self.make()
            self.offer()
            self.now = now
            with self.assertRaises(PermissionError):
                self.pull()
            self.assertIsNone(self.pull())

    def test_maximum_identifiers_and_uint64_preserve_exact_wire_values(self):
        self.p = WebcamPurpose('d'*64, 'c'*128, 't'*128, MAX_U64, MAX_U64)
        self.e = self.make()
        self.offer(self.frame(MAX_U64, producer_capture_ns=MAX_U64), ns=MAX_U64)
        h = HEADER.unpack_from(self.decode(self.pull()))
        self.assertEqual(h[5:9], (MAX_U64, MAX_U64, MAX_U64, MAX_U64))
        self.assertEqual(h[11:14], (64,128,128))

    def test_assembler_preserves_original_capture_timestamp_for_emitter(self):
        captured = MAX_U64 - 7
        consumer = self.consumer()
        original = self.frame(producer_capture_ns=captured)
        for fragment in encode_fragments(self.p, 1, captured, original.bgra):
            consumer.push(fragment)
        frame = consumer.take_latest()
        self.assertEqual(frame.producer_capture_ns, captured)
        observed = []
        def mapping(purpose, ns):
            observed.append(ns)
            return self.mapped
        self.e = self.make(map_capture_clock=mapping)
        self.offer(frame)
        while (event := self.pull()) is not None:
            self.assertEqual(HEADER.unpack_from(self.decode(event))[8], captured)
        self.assertGreater(len(observed), 70)
        self.assertEqual(set(observed), {captured})

    def test_missing_frame_timestamp_cannot_be_filled_by_legacy_argument(self):
        legacy = VideoFrame(self.p, 1, 9.99, b'X'*FRAME_BYTES)
        self.assertIsNone(legacy.producer_capture_ns)
        for explicit in (False, True):
            self.e = self.make()
            with self.assertRaises(ValueError):
                if explicit:
                    self.offer(legacy, ns=123456789)
                else:
                    self.offer(legacy)
            self.assertIsNone(self.pull())

    def test_frame_timestamp_mismatch_and_invalid_metadata_reject_before_mapping(self):
        def forbidden(*args):
            raise AssertionError('invalid timestamp reached capture mapper')
        for captured in (0, True, -1, MAX_U64+1, None):
            self.e = self.make(map_capture_clock=forbidden)
            with self.assertRaises(ValueError):
                self.offer(self.frame(producer_capture_ns=captured))
        self.e = self.make(map_capture_clock=forbidden)
        with self.assertRaises(ValueError):
            self.offer(ns=123456788)
        self.assertIsNone(self.pull())

    def test_original_timestamp_host_mapping_mismatch_rejects(self):
        self.mapped = MappedCapture(9.98, .001)
        with self.assertRaises(PermissionError):
            self.offer(self.frame(producer_capture_ns=MAX_U64))
        self.assertIsNone(self.pull())

    def test_injected_contracts_required_no_default_owner(self):
        with self.assertRaises(TypeError):
            BoundedWebcamWorkerEmitter(self.p)
        with self.assertRaises(ValueError):
            self.make(current=None)


if __name__ == '__main__':
    unittest.main()
