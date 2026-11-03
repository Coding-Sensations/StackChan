"""Trusted backend selection and deferred open using only inert mock resources."""
import hashlib,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
import native_conversation as native

LEGACY=('Pro Tools Audio Bridge 2-A_UID','Pro Tools Audio Bridge 2-B_UID')
DIRECTIONAL=('local.mrkillbob.StackChan.hal.microphone.v2','local.mrkillbob.StackChan.hal.speaker.v2')
class Resource:
    def __init__(self):self.closed=False
    def close(self):self.closed=True
class Adapter(Resource):
    def __init__(self,fail=False):super().__init__();self.calls=[];self.fail=fail;self.muted=True;self.playing=False
    def open_for_approved_test(self):self.calls.append('legacy')
    def open_directional_for_approved_test(self):
        self.calls.append('directional')
        if self.fail:raise RuntimeError('synthetic directional failure')
class DirectionalAdoptionTests(unittest.TestCase):
    def test_route_pair_must_match_trusted_backend(self):
        for backend,allowed,rejected in [('legacy_bridge_v1',LEGACY,DIRECTIONAL),('directional_hal_v2',DIRECTIONAL,LEGACY)]:
            with self.subTest(backend=backend),patch.object(native,'NATIVE_AUDIO_BACKEND',backend):
                route=native.HostAudioDeviceRoute.from_dict(dict(input_uid=allowed[0],output_uid=allowed[1]));self.assertIsNone(route.duration_seconds)
                for body in [dict(input_uid=rejected[0],output_uid=rejected[1]),dict(input_uid=allowed[0],output_uid=allowed[1],native_audio_backend=backend),dict(input_uid=allowed[1],output_uid=allowed[0])]:
                    with self.assertRaises(PermissionError):native.HostAudioDeviceRoute.from_dict(body)
    def setup_bridge(self,backend,adapter,hash_ok=True):
        resources=[]
        def create(*unused):r=Resource();resources.append(r);return r
        with tempfile.TemporaryDirectory() as tmp:
            lib=Path(tmp)/'synthetic.bin';lib.write_bytes(b'not-a-library')
            with patch.object(native,'NATIVE_AUDIO_BACKEND',backend),patch.object(native,'LIBRARY',lib),patch.object(native,'LIBRARY_HASH',hashlib.sha256(lib.read_bytes()).hexdigest() if hash_ok else '0'*64),patch.object(native.config,'require_native'),patch('native_adapter.NativeHostAdapter',return_value=adapter) as load,patch('codec.OpusCodec',side_effect=create),patch('codec.Resampler',side_effect=create):
                try:return native._build_native_bridge(),resources,load.call_count
                except BaseException:
                    self.resources=resources;self.loads=load.call_count;raise
    def test_only_explicit_directional_binding_selects_directional_open(self):
        a=Adapter();bridge,resources,loads=self.setup_bridge('directional_hal_v2',a)
        self.assertEqual(a.calls,['directional']);self.assertEqual(loads,1);bridge.close();self.assertTrue(a.closed);self.assertTrue(all(r.closed for r in resources))
    def test_legacy_binding_preserves_legacy_open(self):
        a=Adapter();bridge,_,_=self.setup_bridge('legacy_bridge_v1',a);self.assertEqual(a.calls,['legacy']);bridge.close()
    def test_directional_failure_closes_without_legacy_fallback(self):
        a=Adapter(fail=True)
        with self.assertRaisesRegex(RuntimeError,'directional failure'):self.setup_bridge('directional_hal_v2',a)
        self.assertEqual(a.calls,['directional']);self.assertTrue(a.closed);self.assertTrue(all(r.closed for r in self.resources))
    def test_hash_failure_rejects_before_native_or_codec_allocation(self):
        a=Adapter()
        with self.assertRaisesRegex(RuntimeError,'library changed'):self.setup_bridge('directional_hal_v2',a,False)
        self.assertEqual(self.loads,0);self.assertEqual(self.resources,[]);self.assertEqual(a.calls,[])
    def test_unknown_backend_rejects_before_native_load(self):
        with self.assertRaises(PermissionError):self.setup_bridge('unapproved',Adapter())
        self.assertEqual(self.loads,0);self.assertEqual(self.resources,[])
    def test_connect_stays_inert_for_directional_route(self):
        with patch.object(native,'NATIVE_AUDIO_BACKEND','directional_hal_v2'),patch.object(native,'_build_native_bridge',side_effect=AssertionError('opened before physical Listen')):
            route=native.HostAudioDeviceRoute.from_dict(dict(input_uid=DIRECTIONAL[0],output_uid=DIRECTIONAL[1]));provider=native.make_provider(route);provider.close()

    def test_trusted_ten_second_limit_ends_even_without_ui_stop(self):
        import test_streaming as streaming
        f=streaming.Tests();f.setUp();now=[0.];completed=[]
        with patch.object(native,'NATIVE_AUDIO_BACKEND','directional_hal_v2'),patch.object(native,'NATIVE_AUDIO_TEST_SECONDS',10):
            route=native.HostAudioDeviceRoute.from_dict(dict(input_uid=DIRECTIONAL[0],output_uid=DIRECTIONAL[1]));self.assertEqual(route.duration_seconds,10)
            session=native.NativeConversationSession(f.registry,f.session.sessions,f.bridge,f.token,f.device.id,f.arms.append,clock=lambda:now[0],fixture=True,duration_seconds=None)
            session.on_complete=lambda:completed.append(True)
            session.begin('bounded-fixture');session.control(dict(type='ack',epoch=1));session.control(dict(type='consent',epoch=1,physical=True));session._authorized()
            for second in range(1,10):
                now[0]=float(second);session.control(dict(type='heartbeat',epoch=1));session._authorized()
            now[0]=10.
            with self.assertRaises(native.TransportComplete):session._authorized()
            self.assertEqual(completed,[True]);session.close();self.assertFalse(session.armed)
