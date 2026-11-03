"""Inert provider lifecycle checks. No CoreAudio, sockets, or live registry."""
import unittest
from types import SimpleNamespace
from unittest.mock import patch
from home_lab.devices import DeviceRegistry
from home_lab.sessions import ConversationSessions
from streaming_session import AudioBackpressure
import native_conversation as native


class Bridge:
    def __init__(self):
        self.calls=[];self.adapter=SimpleNamespace(muted=True,playing=False,arm_for_approved_test=self.arm)
        self.mic_packets=self.speaker_packets=self.return_frames=0
        self.native_audio={};self.fail_close=False
    def begin(self,epoch):self.calls.append(('begin',epoch))
    def arm(self,epoch):self.calls.append(('arm',epoch));self.adapter.muted=False
    def interrupt(self,epoch):self.calls.append(('mute',epoch));self.adapter.muted=True
    def close(self):
        self.calls.append(('close',))
        if self.fail_close:raise RuntimeError('synthetic close failed')
    def poll_voice(self,epoch,frames):raise RuntimeError('synthetic audio fault')
    def outgoing_opus(self,epoch,frames):return [b'synthetic-packet']


class ConsentBoundaryTests(unittest.TestCase):
    def setUp(self):
        self.bridge=Bridge();self.opens=[]
        self.library=patch.object(native,'LIBRARY',SimpleNamespace(read_bytes=lambda: (_ for _ in ()).throw(AssertionError('library read before Listen'))))
        self.library.start();self.addCleanup(self.library.stop)
        builder=patch.object(native,'_build_native_bridge',side_effect=self.build,create=True)
        builder.start();self.addCleanup(builder.stop)
        self.provider=native.make_provider(native.NativeRoute('synthetic-call',60))
        registry=DeviceRegistry()
        device,token=registry.redeem_pairing_code(registry.create_pairing_code(),device_id='synthetic',label='Synthetic',capabilities=('microphone','speaker'))
        self.session=native.NativeConversationSession(registry,ConversationSessions(),self.provider.bridge,token,device.id,self.provider.arm,clock=lambda:0,fixture=True)
    def build(self):self.opens.append('open');return self.bridge
    def begin(self):
        self.session.begin('synthetic-turn')
        self.session.control({'type':'ack','epoch':1})
    def listen(self):self.session.control({'type':'consent','epoch':1,'physical':True})
    def test_connect_begin_ack_and_invalid_consent_do_not_open_capture(self):
        self.begin()
        for value in ({'type':'consent','epoch':1,'physical':False},{'type':'consent','epoch':2,'physical':True}):
            with self.assertRaises((ValueError,PermissionError)):self.session.control(value)
        self.assertEqual(self.opens,[]);self.assertEqual(self.bridge.calls,[])
        self.session.close();self.provider.close()
        self.assertEqual(self.opens,[])

    def test_standard_audio_devices_are_not_misreported_as_call_identity(self):
        route=native.HostAudioDeviceRoute('Pro Tools Audio Bridge 2-A_UID','Pro Tools Audio Bridge 2-B_UID')
        provider=native.make_provider(route)
        self.assertTrue(provider.route_verified)
        self.assertEqual(provider.kind,'host_audio_devices')
        self.assertEqual(provider.route_reference,'host-audio-device-route')
        self.assertFalse(hasattr(provider,'session_reference'))
        self.assertEqual(self.opens,[])
        provider.close()
    def test_physical_listen_opens_once_and_interrupt_releases_capture(self):
        self.begin();self.listen()
        self.assertEqual(self.opens,['open'])
        self.assertEqual(self.bridge.calls,[('begin',1),('arm',1)])
        self.session.control({'type':'interrupt','epoch':1})
        self.assertIn(('mute',1),self.bridge.calls);self.assertIn(('close',),self.bridge.calls)
        self.assertFalse(self.session.armed)
        self.provider.close();self.assertEqual(self.bridge.calls.count(('close',)),1)
    def test_failed_close_keeps_exact_owner_and_retries_without_reopening(self):
        self.begin();self.listen();self.bridge.fail_close=True
        with self.assertRaises(RuntimeError):self.session.close()
        self.assertFalse(self.session.armed)
        with self.assertRaises(RuntimeError):self.provider.close()
        self.bridge.fail_close=False;self.provider.close()
        self.assertEqual(self.opens,['open'])
        self.assertEqual(self.bridge.calls.count(('close',)),3)
    def test_audio_fault_closes_capture(self):
        self.begin();self.listen()
        with self.assertRaisesRegex(RuntimeError,'audio fault'):self.provider.bridge.poll_voice(1,960)
        self.assertIn(('close',),self.bridge.calls)
    def test_transient_backpressure_keeps_capture_until_session_streak_limit(self):
        self.begin();self.listen()
        def pressure(epoch,packet):raise AudioBackpressure('synthetic bounded queue miss')
        self.bridge.receive_opus=pressure
        with self.assertRaises(AudioBackpressure):self.provider.bridge.receive_opus(1,b'synthetic')
        self.assertNotIn(('close',),self.bridge.calls)
        self.session.close();self.assertIn(('close',),self.bridge.calls)
    def test_setup_failure_retains_partial_owner_until_checked_close(self):
        self.begin()
        owner=Bridge();owner.fail_close=True
        self.provider.bridge.builder=lambda:(_ for _ in ()).throw(native.ProviderCleanupError(native.QualifiedProvider(owner,None,owner.close,'synthetic',False)))
        with self.assertRaises(native.ProviderCleanupError):self.listen()
        self.assertFalse(self.session.armed)
        with self.assertRaises(RuntimeError):self.provider.close()
        owner.fail_close=False;self.provider.close()
        self.assertEqual(owner.calls,[('close',),('close',)])
    def test_failed_arm_closes_capture_without_granting_media(self):
        self.begin()
        def fail(epoch):raise RuntimeError('synthetic arm failure')
        self.bridge.adapter.arm_for_approved_test=fail
        with self.assertRaisesRegex(RuntimeError,'arm failure'):self.listen()
        self.assertFalse(self.session.armed)
        self.assertIn(('close',),self.bridge.calls)
        self.provider.close();self.assertEqual(self.bridge.calls.count(('close',)),1)
    def test_new_turn_requires_new_physical_consent_after_interrupt(self):
        self.begin();self.listen();self.session.interrupt()
        self.session.begin('new-synthetic-turn')
        self.session.control({'type':'ack','epoch':2})
        self.assertEqual(self.opens,['open'])
        with self.assertRaises(PermissionError):self.listen()
        self.assertEqual(self.opens,['open'])
        self.session.control({'type':'consent','epoch':2,'physical':True})
        self.assertEqual(self.opens,['open','open'])
        self.session.close()
    def test_direct_session_output_remains_inert_until_listen(self):
        self.begin()
        self.assertEqual(self.session.outgoing(),[]);self.assertEqual(self.opens,[])
        self.listen()
        self.assertEqual(len(self.session.outgoing()),1)
        self.session.close()


if __name__=='__main__':unittest.main()
