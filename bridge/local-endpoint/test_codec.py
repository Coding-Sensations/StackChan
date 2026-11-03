import math
import struct
import unittest
try:
    from codec import OpusCodec, Resampler
except ImportError:
    OpusCodec=Resampler=None


def tone(rate,ms,freq=440):
    return struct.pack('<'+'h'*(rate*ms//1000),*[round(9000*math.sin(2*math.pi*freq*i/rate)) for i in range(rate*ms//1000)])


class CodecTests(unittest.TestCase):
    def setUp(self):self.assertIsNotNone(OpusCodec,'real codec missing')

    def test_actual_opus_roundtrip_16k_and24k60ms(self):
        for rate in (16000,24000):
            with OpusCodec(rate) as c:
                packet=c.encode(tone(rate,60))
                pcm=c.decode(packet)
                self.assertEqual(len(pcm),rate*60//1000*2)
                samples=struct.unpack('<'+'h'*(len(pcm)//2),pcm)
                self.assertGreater(sum(abs(s) for s in samples),100000)
                self.assertGreater(len(packet),2);self.assertLess(len(packet),4096)

    def test_opus_rejects_invalid_lengths_and_use_after_close(self):
        c=OpusCodec(16000)
        for pcm in (b'x',b'\0\0'*159,b'\0\0'*16000):
            with self.assertRaises(ValueError):c.encode(pcm)
        with self.assertRaises(ValueError):c.decode(b'x'*4097)
        c.close();c.close()
        with self.assertRaises(RuntimeError):c.encode(tone(16000,60))

    def test_opus_reset_removes_previous_stream_state(self):
        with OpusCodec(16000) as c:
            c.encode(tone(16000,60));c.reset()
            reset_packet=c.encode(b'\0\0'*960)
        with OpusCodec(16000) as fresh:
            self.assertEqual(reset_packet,fresh.encode(b'\0\0'*960))

    def test_streaming_resampler_preserves_duration_with_flush(self):
        with Resampler(16000,48000) as r:
            output=b''.join(r.process(tone(16000,20)) for _ in range(3))+r.flush()
            self.assertEqual(len(output),48000*60//1000*2)
        with Resampler(48000,24000) as r:
            self.assertEqual(len(r.process(tone(48000,60))+r.flush()),2880)

    def test_resampler_reset_drops_old_filter_tail(self):
        with Resampler(16000,48000) as r:
            r.process(tone(16000,20));r.reset()
            pcm=r.process(b'\0\0'*320)+r.flush()
            self.assertEqual(set(pcm),{0})

    def test_resampler_rejects_unbounded_and_closed_input(self):
        with Resampler(16000,48000) as r:
            for data in (b'x',b'x'*20000):
                with self.assertRaises(ValueError):r.process(data)
        with self.assertRaises(RuntimeError):r.process(b'\0\0')


if __name__=='__main__':unittest.main()
