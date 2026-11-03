import unittest
from codec import OpusCodec
from test_codec import tone
try:
    from endpoint import Pipeline, parse_hello
except ImportError:
    Pipeline=parse_hello=None

class PipelineTests(unittest.TestCase):
    def setUp(self):self.assertIsNotNone(Pipeline,'endpoint pipeline missing')
    def test_streaming_pipeline_duration_and_reset(self):
        with Pipeline() as p,OpusCodec(16000) as source,OpusCodec(24000) as sink:
            packets=[]
            for _ in range(4):packets+=p.feed(source.encode(tone(16000,60)))
            packets+=p.finish()
            self.assertEqual(sum(len(sink.decode(x)) for x in packets),24000*240//1000*2)
            p.feed(source.encode(tone(16000,60)));p.reset()
            self.assertEqual(p.finish(),[])
    def test_strict_factory_hello(self):
        good={'type':'hello','version':2,'transport':'websocket','audio_params':{'format':'opus','sample_rate':16000,'channels':1,'frame_duration':60}}
        self.assertEqual(parse_hello(good),2)
        for key,val in [('version',True),('transport','mqtt')]:
            bad=dict(good);bad[key]=val
            with self.assertRaises(ValueError):parse_hello(bad)
        good['audio_params']['sample_rate']=48000
        with self.assertRaises(ValueError):parse_hello(good)
if __name__=='__main__':unittest.main()
