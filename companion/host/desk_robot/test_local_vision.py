"""Offline tests: generated pixels only; never a camera or real image."""
from concurrent.futures import ThreadPoolExecutor
from dataclasses import FrozenInstanceError
import struct
import unittest
from unittest.mock import patch

from desk_robot.local_vision import LocalVision, RGBThumbnail
from desk_robot.session_contract import Binding


def solid(value, width=20, height=15):
    return RGBThumbnail(width, height, bytes([value, value, value]) * width * height)


def bright_patch(column):
    pixels = bytearray(20 * 15 * 3)
    for y in range(5, 10):
        for x in range(column, column + 5):
            offset = (y * 20 + x) * 3
            pixels[offset:offset + 3] = b'\xff\xff\xff'
    return RGBThumbnail(20, 15, bytes(pixels))


class ThumbnailTests(unittest.TestCase):
    def test_round_trip_size_and_private_repr(self):
        image = solid(255, 160, 120)
        self.assertEqual(len(image.pixels), 57600)
        self.assertEqual(len(image.encode()), 57608)
        self.assertEqual(RGBThumbnail.decode(image.encode()), image)
        self.assertNotIn('pixels=', repr(image))
        with self.assertRaises(FrozenInstanceError):
            image.width = 1

    def test_dimension_type_and_exact_length_bounds(self):
        for width, height, pixels in [(True, 1, b'abc'), (1., 1, b'abc'), (0, 1, b''),
                                      (161, 1, b'abc' * 161), (1, 121, b'abc' * 121),
                                      (1, 1, bytearray(b'abc')), (1, 1, b'ab'),
                                      (1, 1, b'abcd'), ('1', 1, b'abc')]:
            with self.subTest(width=width, height=height):
                with self.assertRaises(ValueError):
                    RGBThumbnail(width, height, pixels)

    def test_decode_rejects_schema_trailing_truncated_and_oversize(self):
        for payload in [b'', b'LV01', struct.pack('>4sHH', b'LV02', 1, 1) + b'abc',
                        struct.pack('>4sHH', b'LV01', 65535, 1) + b'abc',
                        solid(0, 1, 1).encode() + b'\x00', solid(0, 1, 1).encode()[:-1],
                        bytearray(solid(0).encode()), b'x' * 65537]:
            with self.assertRaises(ValueError):
                RGBThumbnail.decode(payload)


class VisionTests(unittest.TestCase):
    def setUp(self):
        self.now = 0.
        self.current = True
        self.binding = Binding('conversation', 'turn', 1, 1)
        self.vision = LocalVision(lambda b: self.current and b == self.binding, clock=lambda: self.now)

    def enable(self):
        self.vision.enable(self.binding, local_opt_in=True)

    def summary(self, sequence, image, request=None):
        self.vision.push(self.binding, sequence, self.now, image)
        return self.vision.request(self.binding, request or 'request-' + str(sequence), visual_request=True)

    def test_default_off_exact_opt_in_and_request(self):
        self.assertFalse(self.vision.enabled)
        with self.assertRaises(PermissionError):
            self.vision.push(self.binding, 0, self.now, solid(0))
        for opt_in in [False, 1, 'yes']:
            with self.assertRaises(PermissionError):
                self.vision.enable(self.binding, local_opt_in=opt_in)
        self.enable()
        self.vision.push(self.binding, 0, self.now, solid(255))
        with self.assertRaises(PermissionError):
            self.vision.request(self.binding, 'r', visual_request=1)
        self.assertFalse(self.vision.enabled)
        self.assertIsNone(self.vision._history)

    def test_solid_statistics_and_latest_only_single_consumption(self):
        self.enable()
        self.vision.push(self.binding, 0, self.now, solid(0))
        bright = self.summary(1, solid(255))
        self.assertEqual((bright.brightness, bright.contrast, bright.bright_centroid), (1., 0., (.5, .5)))
        self.assertIsNone(bright.motion)
        self.assertIsNone(self.vision.request(self.binding, 'empty', visual_request=True))
        dark = self.summary(2, solid(0))
        self.assertEqual((dark.brightness, dark.contrast, dark.bright_centroid), (0., 0., None))
        self.assertEqual(dark.motion.mean_difference, 1.)
        self.assertNotIn('pixels', repr(bright))

    def test_patch_motion_and_bounded_history(self):
        self.enable()
        first = self.summary(0, bright_patch(1))
        self.assertEqual(first.bright_centroid, (.175, .5))
        self.assertGreater(first.contrast, 0)
        same = self.summary(1, bright_patch(1))
        self.assertEqual(same.motion.changed_fraction, 0.)
        moved = self.summary(2, bright_patch(12))
        self.assertGreater(moved.motion.mean_difference, 0.)
        self.assertGreater(moved.motion.changed_fraction, 0.)
        self.summary(3, solid(100, 160, 120))
        self.assertEqual(len(self.vision._history[3]), 300)
        self.assertTrue(all(type(value) is float for value in self.vision._history[3]))

    def test_disable_new_binding_and_size_change_reset_motion(self):
        self.enable()
        self.summary(0, solid(0))
        self.vision.disable()
        self.assertIsNone(self.vision._history)
        self.binding = Binding('conversation', 'next', 2, 2)
        self.enable()
        self.assertIsNone(self.summary(0, solid(255)).motion)
        self.assertIsNone(self.summary(1, solid(0, 10, 10)).motion)
        self.vision.enable(self.binding, local_opt_in=True)
        self.assertIsNone(self.summary(0, solid(255)).motion)

    def test_stale_history_and_stale_pending_frame(self):
        self.enable()
        self.summary(0, solid(0))
        self.now = .5
        self.assertIsNone(self.summary(1, solid(255)).motion)
        self.vision.push(self.binding, 2, self.now, solid(0))
        self.now = 1.
        with self.assertRaises(TimeoutError):
            self.vision.request(self.binding, 'late', visual_request=True)
        self.assertFalse(self.vision.enabled)
        self.assertIsNone(self.vision._history)

    def test_invalid_owner_foreign_binding_and_owner_exception_clear(self):
        for mode in ['revoked', 'foreign', 'exception']:
            self.setUp()
            self.enable()
            self.summary(0, solid(255))
            binding = self.binding
            if mode == 'revoked':
                self.current = False
            elif mode == 'foreign':
                binding = Binding('other', 'turn', 1, 1)
            else:
                def invalid(_):
                    raise RuntimeError('owner unavailable')
                self.vision._current = invalid
            with self.assertRaises(PermissionError):
                self.vision.request(binding, 'r', visual_request=True)
            self.assertFalse(self.vision.enabled)
            self.assertIsNone(self.vision._history)

    def test_future_timestamp_bad_sequence_type_and_bad_clock(self):
        for sequence, timestamp in [(0, 1.), (True, 0.), (0, float('nan')), (0, True), (0xffffffff, 0.)]:
            self.setUp()
            self.enable()
            with self.assertRaises(ValueError):
                self.vision.push(self.binding, sequence, timestamp, solid(0))
            self.assertFalse(self.vision.enabled)
        for now in [float('inf'), float('nan'), True, -1, '0', 10 ** 1000]:
            self.setUp()
            self.enable()
            self.now = now
            with self.assertRaises(ValueError):
                self.vision.request(self.binding, 'r', visual_request=True)
            self.assertIsNone(self.vision._history)
        self.setUp()
        self.now = 1.
        self.enable()
        self.summary(0, solid(0))
        self.now = .9
        with self.assertRaises(ValueError):
            self.vision.request(self.binding, 'r', visual_request=True)

    def test_malformed_input_and_schema_fail_closed(self):
        self.enable()
        self.summary(0, solid(0))
        malformed = solid(0)
        object.__setattr__(malformed, 'width', True)
        with self.assertRaises(ValueError):
            self.vision.push(self.binding, 1, self.now, malformed)
        self.assertIsNone(self.vision._history)
        self.enable()
        self.vision._adapter.push(self.binding, 0, self.now, b'unknown schema')
        with self.assertRaises(ValueError):
            self.vision.request(self.binding, 'r', visual_request=True)
        self.assertFalse(self.vision.enabled)
        self.enable()
        with self.assertRaises(ValueError):
            self.vision.push(self.binding, 0, self.now, {'pixels': b'abc'})

    def test_request_id_replay_and_budget(self):
        self.enable()
        self.summary(0, solid(0), 'r')
        self.vision.push(self.binding, 1, self.now, solid(0))
        with self.assertRaises(PermissionError):
            self.vision.request(self.binding, 'r', visual_request=True)
        self.enable()
        for sequence in range(16):
            self.summary(sequence, solid(0))
        self.vision.push(self.binding, 16, self.now, solid(0))
        with self.assertRaises(BufferError):
            self.vision.request(self.binding, 'exhausted', visual_request=True)
        self.assertIsNone(self.vision._history)
        for request_id in ['', 'x' * 129, 'bad id', 'é', 1]:
            self.enable()
            with self.assertRaises(ValueError):
                self.vision.request(self.binding, request_id, visual_request=True)

    def test_revalidation_after_statistics(self):
        from desk_robot import local_vision
        original = local_vision._statistics
        for failure in ['owner', 'freshness']:
            self.setUp()
            self.enable()
            self.vision.push(self.binding, 0, self.now, solid(255))
            def delayed(image):
                result = original(image)
                if failure == 'owner':
                    self.current = False
                else:
                    self.now = .5
                return result
            with patch.object(local_vision, '_statistics', delayed):
                with self.assertRaises((PermissionError, TimeoutError)):
                    self.vision.request(self.binding, 'r', visual_request=True)
            self.assertIsNone(self.vision._history)

    def test_concurrent_requests_consume_only_once(self):
        self.enable()
        self.vision.push(self.binding, 0, self.now, solid(255))
        def request(index):
            return self.vision.request(self.binding, 'r-' + str(index), visual_request=True)
        with ThreadPoolExecutor(max_workers=8) as pool:
            results = list(pool.map(request, range(8)))
        self.assertEqual(sum(result is not None for result in results), 1)


if __name__ == '__main__':
    unittest.main()
