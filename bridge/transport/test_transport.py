import struct
import unittest
from dataclasses import replace
try:
    from transport import OpusEnvelope, UsbPcmFrame, UsbPcmParser, DuplexGate, latency_budget
except ImportError:
    OpusEnvelope = UsbPcmFrame = UsbPcmParser = DuplexGate = latency_budget = None
from adapter import AudioFrame, OfflineAdapter
from home_lab.contracts import Device
from home_lab.sessions import ConversationSessions
from home_lab.events import EventJournal


class TransportTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(OpusEnvelope,'transport fixture implementation missing')

    def test_official_v2_network_header_exact_bytes(self):
        p=OpusEnvelope(version=2,payload=b'fake-opus',timestamp=1234)
        wire=p.encode()
        self.assertEqual(wire,struct.pack('>HHIII',2,0,0,1234,9)+b'fake-opus')
        self.assertEqual(OpusEnvelope.decode(2,wire),p)

    def test_official_v1_raw_and_v3_length_header(self):
        for v in (1,3):
            p=OpusEnvelope(version=v,payload=b'fixture',timestamp=0)
            self.assertEqual(OpusEnvelope.decode(v,p.encode()),p)
        self.assertEqual(OpusEnvelope(version=3,payload=b'x').encode(),b'\0\0\0\1x')

    def test_network_rejects_truncation_trailing_bytes_and_non_audio(self):
        wire=OpusEnvelope(version=2,payload=b'x').encode()
        for bad in (wire[:4],wire[:-1],wire+b'x',struct.pack('>HHIII',2,1,0,0,1)+b'x'):
            with self.assertRaises(ValueError):OpusEnvelope.decode(2,bad)
        with self.assertRaises(ValueError):OpusEnvelope(version=1,payload=b'x'*4097)
        with self.assertRaises(ValueError):OpusEnvelope(version=3,payload=b'x',timestamp=1)

    def test_proposed_usb_pcm_roundtrip_fragmented_and_coalesced(self):
        f=UsbPcmFrame(epoch=3,sequence=1,sample_rate=16000,direction='mic',pcm=b'\0\0'*320)
        parser=UsbPcmParser();found=[]
        for byte in f.encode():found+=parser.feed(bytes([byte]))
        self.assertEqual(found,[f])
        self.assertEqual(parser.feed(f.encode()+f.encode()),[f,f])

    def test_usb_corruption_and_debug_log_mix_fail_closed(self):
        f=UsbPcmFrame(epoch=3,sequence=0,sample_rate=24000,direction='speaker',pcm=b'\0\0'*480)
        wire=bytearray(f.encode());wire[-1]^=1
        for bad in (bytes(wire),b'DEBUG logs'+f.encode(),b'x'*8193):
            parser=UsbPcmParser()
            with self.assertRaises(ValueError):parser.feed(bad)
            self.assertEqual(parser.buffered_bytes,0)
        with self.assertRaises(ValueError):replace(f,pcm=b'x'*1922)
        with self.assertRaises(ValueError):replace(f,sample_rate=48000)

    def test_latency_budget_exposes_stock_frame_floor(self):
        stock=latency_budget('network-opus')
        proposed=latency_budget('usb-pcm')
        self.assertEqual(stock['capture_frame_ms'],60)
        self.assertLess(proposed['roundtrip_adapter_target_ms'],stock['roundtrip_adapter_target_ms'])
        self.assertFalse(stock['measured'])
        self.assertEqual(stock['model_response_ms'],'unknown')


class DuplexTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(DuplexGate,'duplex gate missing')
        self.b=OfflineAdapter(Device(id='r',label='Fixture',status='offline_fixture',capabilities=('microphone','speaker')),ConversationSessions(),EventJournal())
        self.t=self.b.begin('turn',0.0)
        self.f=AudioFrame(device_id='r',conversation_id=self.t.conversation_id,turn_id=self.t.id,generation=self.t.generation,sequence=0,pcm=b'\0\0')
        self.g=DuplexGate(self.b)

    def test_unverified_physical_mute_state_blocks_input(self):
        self.assertFalse(self.g.capture(self.f,0.1))
        with self.assertRaises(PermissionError):self.g.set_physical_mute(False,verified=False,now=0.2)

    def test_physical_mute_clears_inflight_and_cannot_replay_after_unmute(self):
        self.g.set_physical_mute(False,verified=True,now=0.1)
        self.assertTrue(self.g.capture(self.f,0.2))
        self.g.set_physical_mute(True,verified=True,now=0.3)
        self.assertIsNone(self.b.pop_input(0.4))
        self.g.set_physical_mute(False,verified=True,now=0.5)
        self.assertFalse(self.g.capture(self.f,0.6))

    def test_half_duplex_without_aec_blocks_echo_and_preserves_new_speech_after_stop(self):
        self.g.set_physical_mute(False,verified=True,now=0.1)
        self.g.set_playback(True)
        self.assertFalse(self.g.capture(self.f,0.2))
        self.g.set_playback(False)
        self.assertTrue(self.g.capture(self.f,0.3))

    def test_full_duplex_requires_reference_qualification_and_reports_no_speaker_ack(self):
        with self.assertRaises(PermissionError):self.g.enable_full_duplex(aec_verified=False,reference_verified=True)
        self.g.enable_full_duplex(aec_verified=True,reference_verified=True)
        self.g.set_physical_mute(False,verified=True,now=0.1);self.g.set_playback(True)
        self.assertTrue(self.g.capture(self.f,0.2))
        result=self.g.interrupt(0.3)
        self.assertEqual(result['speaker_stop'],'unsupported')
        self.assertFalse(self.g.capture(self.f,0.4))


class FurtherBoundaries(unittest.TestCase):
    def test_mute_blocks_playback_and_interrupt_clears_render_queue(self):
        self.assertIsNotNone(DuplexGate,'gate required')
        b=OfflineAdapter(Device(id='r',label='Fixture',status='offline_fixture',capabilities=('speaker',)),ConversationSessions(),EventJournal())
        t=b.begin('r1',0.0)
        f=AudioFrame(device_id='r',conversation_id=t.conversation_id,turn_id=t.id,generation=t.generation,sequence=0,pcm=b'\0\0')
        g=DuplexGate(b);self.assertFalse(g.render(f,0.1))
        g.set_physical_mute(False,verified=True,now=0.2);self.assertTrue(g.render(f,0.3))
        g.interrupt(0.4);self.assertIsNone(b.pop(0.5))

    def test_network_payload_cannot_exceed_declared_version_or_timestamp_limits(self):
        self.assertIsNotNone(OpusEnvelope,'envelope required')
        for changes in ({'version':True},{'timestamp':-1},{'timestamp':2**32},{'payload':b''}):
            with self.assertRaises(ValueError):OpusEnvelope(**({'version':2,'payload':b'x'}|changes))

    def test_pcm_endpoint_requires_new_unique_binding_for_each_turn(self):
        import transport
        self.assertTrue(hasattr(transport,'PcmSessionBinding'),'session wire binding missing')
        b=OfflineAdapter(Device(id='r',label='Fixture',status='offline_fixture',capabilities=('microphone',)),ConversationSessions(),EventJournal())
        b.begin('r1',0.0);g=DuplexGate(b);g.set_physical_mute(False,verified=True,now=0.0)
        binding=transport.PcmSessionBinding(g);epoch=binding.bind_current_turn()
        old=UsbPcmFrame(epoch,0,16000,'mic',b'\0\0')
        self.assertTrue(binding.accept(old,0.1));self.assertEqual(b.pop_input(0.2),old.pcm)
        b.begin('r2',0.3);new_epoch=binding.bind_current_turn()
        self.assertNotEqual(epoch,new_epoch);self.assertFalse(binding.accept(old,0.4))
        with self.assertRaises(NotImplementedError):binding.accept(UsbPcmFrame(new_epoch,0,24000,'mic',b'\0\0'),0.5)


class HostRouteTests(unittest.TestCase):
    def test_host_route_is_directional_and_non_executable(self):
        import transport
        self.assertTrue(hasattr(transport,'host_route_plan'),'public host route plan missing')
        p=transport.host_route_plan('Pro Tools Audio Bridge 2-A','Pro Tools Audio Bridge 2-B')
        self.assertFalse(p['executable'])
        self.assertEqual(p['mic_path']['native_voice'],'input')
        self.assertEqual(p['playback_path']['native_voice'],'output')
        self.assertEqual(p['session'],'existing dot in native Voice; user-operated')
        with self.assertRaises(ValueError):transport.host_route_plan('same','same')


if __name__=='__main__':unittest.main()
