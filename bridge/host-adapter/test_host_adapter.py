import struct,unittest
try:
    from host_adapter import SyntheticHostAdapter,RoutePlan
except ImportError:
    SyntheticHostAdapter=RoutePlan=None

def samples(n,value=1000):return struct.pack('<'+'h'*n,*([value]*n))
class HostAdapterTests(unittest.TestCase):
    def setUp(self):self.assertIsNotNone(SyntheticHostAdapter,'host adapter missing')
    def adapter(self):return SyntheticHostAdapter(RoutePlan('A_UID','B_UID'))
    def test_default_muted_and_epoch_required(self):
        a=self.adapter();a.begin(1)
        self.assertFalse(a.robot_pcm(1,samples(960)))
        self.assertEqual(a.dot_input_callback(1,512),samples(512,0))
        with self.assertRaises(ValueError):a.begin(1)
    def test_variable512_callbacks_preserve_audio_order_and_pad_underrun(self):
        a=self.adapter();a.begin(1);a.unmute_synthetic(1)
        pcm=struct.pack('<'+'h'*960,*range(960));self.assertTrue(a.robot_pcm(1,pcm))
        self.assertEqual(a.dot_input_callback(1,512),pcm[:1024])
        self.assertEqual(a.dot_input_callback(1,512),pcm[1024:]+samples(64,0))
        self.assertEqual(a.metrics['underrun_samples'],64)
    def test_two_buses_are_directionally_isolated(self):
        a=self.adapter();a.begin(1);a.unmute_synthetic(1)
        a.robot_pcm(1,samples(512,123));a.dot_output_callback(1,samples(512,456))
        self.assertEqual(a.dot_input_callback(1,512),samples(512,123))
        self.assertEqual(a.robot_output_pcm(1,512),samples(512,456))
    def test_interrupt_and_new_epoch_reject_stale_callbacks(self):
        a=self.adapter();a.begin(1);a.unmute_synthetic(1)
        a.robot_pcm(1,samples(512));a.dot_output_callback(1,samples(512));a.interrupt(1)
        self.assertEqual(a.robot_output_pcm(1,512),samples(512,0))
        a.begin(2);a.unmute_synthetic(2)
        self.assertFalse(a.robot_pcm(1,samples(512)));self.assertFalse(a.dot_output_callback(1,samples(512)))
        self.assertEqual(a.dot_input_callback(1,512),samples(512,0))
    def test_overflow_discards_oldest_samples_with_explicit_counter(self):
        a=self.adapter();a.begin(1);a.unmute_synthetic(1)
        a.robot_pcm(1,samples(5760,123));a.robot_pcm(1,samples(5760,456))
        self.assertEqual(a.buffered_samples,{'robot_to_dot':7680,'dot_to_robot':0})
        self.assertEqual(a.metrics['discarded_samples'],3840)
        self.assertEqual(a.dot_input_callback(1,1920),samples(1920,123))
        self.assertEqual(a.dot_input_callback(1,512),samples(512,456))
    def test_halfduplex_suppresses_robot_capture_while_playing(self):
        a=self.adapter();a.begin(1);a.unmute_synthetic(1);a.set_playing(1,True)
        self.assertFalse(a.robot_pcm(1,samples(512)))
        self.assertTrue(a.dot_output_callback(1,samples(512)))
        a.set_playing(1,False);self.assertTrue(a.robot_pcm(1,samples(512)))
    def test_invalid_format_capacity_callback_and_samebus_fail(self):
        with self.assertRaises(ValueError):RoutePlan('A','A')
        with self.assertRaises(ValueError):RoutePlan('A','B',sample_rate=16000)
        a=self.adapter();a.begin(1);a.unmute_synthetic(1)
        for pcm in (b'x',b'\0'*12000):
            with self.assertRaises(ValueError):a.robot_pcm(1,pcm)
        for n in (True,0,2049):
            with self.assertRaises(ValueError):a.dot_input_callback(1,n)
    def test_device_io_is_explicitly_unavailable(self):
        with self.assertRaises(NotImplementedError):self.adapter().open_device_io()

class CodecWiringTests(unittest.TestCase):
    def test_stale_muted_decode_is_not_called_and_valid_pcm_reaches_bus(self):
        from host_adapter import CodecHostBridge
        class Decoder:
            calls=0
            def reset(self):pass
            def decode(self,packet):self.calls+=1;return samples(960,23)
        class Up:
            def reset(self):pass
            def process(self,pcm):return samples(len(pcm)//2*3,23)
        d=Decoder();a=SyntheticHostAdapter(RoutePlan('A','B'))
        bridge=CodecHostBridge(a,d,d,Up(),Up());bridge.begin(1)
        self.assertFalse(bridge.receive_opus(1,b'fixture'));self.assertEqual(d.calls,0)
        a.unmute_synthetic(1);self.assertFalse(bridge.receive_opus(2,b'fixture'));self.assertEqual(d.calls,0)
        self.assertTrue(bridge.receive_opus(1,b'fixture'));self.assertEqual(a.dot_input_callback(1,512),samples(512,23))
    def test_downlink_frames_form60ms_packets_and_interrupt_drops_partial(self):
        from host_adapter import CodecHostBridge
        class Encoder:
            resets=0
            def reset(self):self.resets+=1
            def encode(self,pcm):return pcm
        class Down:
            def reset(self):pass
            def process(self,pcm):return samples(len(pcm)//4,45)
        e=Encoder();a=SyntheticHostAdapter(RoutePlan('A','B'))
        bridge=CodecHostBridge(a,e,e,Down(),Down());bridge.begin(1);a.unmute_synthetic(1)
        packets=[]
        for _ in range(6):
            a.dot_output_callback(1,samples(512,45));packets+=bridge.outgoing_opus(1)
        self.assertEqual(packets,[samples(1440,45)])
        self.assertEqual(len(bridge.pending),192)
        bridge.interrupt(1);self.assertEqual(bridge.pending,b'');self.assertEqual(bridge.outgoing_opus(1),[])

if __name__=='__main__':unittest.main()
