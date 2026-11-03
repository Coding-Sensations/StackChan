import unittest
from audio import AudioBuffer


class AudioBufferTests(unittest.TestCase):
    def test_bounded_buffer_rejects_overflow_without_losing_existing_chunk(self):
        buffer = AudioBuffer(capacity=1)
        turn = buffer.start_turn(1.0)
        buffer.push(turn, 0, b'\x00\x00', 1.1)
        with self.assertRaises(BufferError):
            buffer.push(turn, 1, b'\x01\x00', 1.2)
        self.assertEqual(buffer.pop(1.3), b'\x00\x00')

    def test_interrupt_clears_playback_and_rejects_old_generation(self):
        buffer = AudioBuffer()
        old = buffer.start_turn(1.0)
        buffer.push(old, 0, b'\x01\x00', 1.1)
        current = buffer.interrupt(1.2)
        self.assertIsNone(buffer.pop(1.3))
        self.assertFalse(buffer.push(old, 1, b'\x02\x00', 1.4))
        self.assertTrue(buffer.push(current, 0, b'\x03\x00', 1.5))
        self.assertEqual(buffer.pop(1.6), b'\x03\x00')

    def test_first_output_latency_is_capture_to_pop_not_receive_time(self):
        buffer = AudioBuffer()
        turn = buffer.start_turn(2.0)
        buffer.push(turn, 0, b'\x00\x00', 2.2)
        buffer.pop(2.3)
        buffer.push(turn, 1, b'\x00\x00', 2.4)
        buffer.pop(2.5)
        self.assertAlmostEqual(buffer.first_output_ms, 300)

    def test_reordered_chunk_is_rejected_without_advancing_sequence(self):
        buffer = AudioBuffer()
        turn = buffer.start_turn(1.0)
        with self.assertRaises(ValueError):
            buffer.push(turn, 1, b'\x00\x00', 1.1)
        self.assertTrue(buffer.push(turn, 0, b'\x00\x00', 1.2))

    def test_invalid_pcm_or_timestamp_is_rejected(self):
        buffer = AudioBuffer()
        turn = buffer.start_turn(1.0)
        for value in (b'', b'x', b'\x00\x00' * 641):
            with self.assertRaises(ValueError):
                buffer.push(turn, 0, value, 1.1)
        for time in (float('nan'), float('inf'), 0.9):
            with self.assertRaises(ValueError):
                buffer.push(turn, 0, b'\x00\x00', time)


if __name__ == '__main__':
    unittest.main()
