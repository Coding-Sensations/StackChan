import asyncio
import struct
import math
import unittest
from types import SimpleNamespace
from unittest.mock import patch
import test_streaming as streaming
import test_gateway_connection as gc
import test_transport_test as tt
try:
    import native_conversation as native
except ImportError:
    native = None

class NativeTests(unittest.TestCase):
    def test_real_call_wiring_available_without_cue(self):
        self.assertIsNotNone(native, 'native conversation provider wiring is missing')

    @unittest.skipIf(native is None, 'implementation missing')
    def test_route_requires_explicit_call_and_forwarding_confirmation(self):
        value = dict(session_reference='human-confirmed-original-call', call_confirmed=True,
                     route_confirmed=True, forwarding_approved=True, duration_seconds=60)
        route = native.NativeRoute.from_dict(value)
        self.assertEqual(route.session_reference, value['session_reference'])
        for key in ('call_confirmed', 'route_confirmed', 'forwarding_approved'):
            with self.assertRaises(PermissionError):
                native.NativeRoute.from_dict(dict(value, **{key: False}))
        with self.assertRaises(ValueError):
            native.NativeRoute.from_dict(dict(value, duration_seconds=900))

    @unittest.skipIf(native is None, 'implementation missing')
    def test_host_audio_route_needs_endpoints_without_call_attestation(self):
        value=dict(input_uid='Pro Tools Audio Bridge 2-A_UID', output_uid='Pro Tools Audio Bridge 2-B_UID')
        route=native.HostAudioDeviceRoute.from_dict(value)
        self.assertIsNone(route.duration_seconds)
        for changed in (dict(value,input_uid='wrong-device'), dict(value,session_reference='fabricated-call')):
            with self.assertRaises((ValueError,PermissionError)):native.HostAudioDeviceRoute.from_dict(changed)

    def test_persistent_audio_has_no_sixty_second_cutoff_but_keeps_heartbeat(self):
        fixture=streaming.Tests();fixture.setUp();now=[0.]
        session=native.NativeConversationSession(fixture.registry,fixture.session.sessions,fixture.bridge,
            fixture.token,fixture.device.id,fixture.arms.append,clock=lambda:now[0],fixture=True,duration_seconds=None)
        session.begin('persistent-audio');session.control(dict(type='ack',epoch=1))
        session.control(dict(type='consent',epoch=1,physical=True));session._authorized()
        for second in range(1,121):
            now[0]=float(second);session.control(dict(type='heartbeat',epoch=1))
        self.assertTrue(session.armed)
        now[0]=123.
        with self.assertRaisesRegex(TimeoutError,'heartbeat expired'):session._authorized()
        session.close()

    @unittest.skipIf(native is None, 'implementation missing')
    def test_voice_return_drain_and_turn_taking_preserve_real_pcm(self):
        from codec import OpusCodec, Resampler
        class Adapter:
            def __init__(self):
                self.epoch=0; self.muted=True; self.playing=False; self.input=[]; self.output=[]
            def begin(self, epoch):self.epoch=epoch
            def arm_for_approved_test(self, epoch):self.muted=False
            def set_playing(self, epoch, playing):self.playing=playing; return True
            def robot_output_pcm(self, epoch, frames):
                self.output.append(frames)
                return self.input.pop(0) if self.input else bytes(frames*2)
            def robot_pcm(self, epoch, pcm):return not self.playing
            def diagnostics(self):return dict(error=0,input_dropped=0,output_dropped=0)
            def interrupt(self, epoch):self.muted=True; self.playing=False;return True
            def close(self):self.muted=True
        clock=tt.Clock(); adapter=Adapter()
        components=[OpusCodec(16000),OpusCodec(24000),Resampler(16000,48000),Resampler(48000,24000)]
        bridge=native.NativeVoiceBridge(adapter,*components,clock=clock)
        decoder=OpusCodec(24000)
        try:
            bridge.begin(1);adapter.arm_for_approved_test(1)
            active, packets=bridge.poll_voice(1,960)
            self.assertFalse(active);self.assertEqual(packets,[]);self.assertEqual(adapter.output,[960])
            adapter.input=[struct.pack('<960h',*(int(1000*math.sin(2*math.pi*1000*i/48000)) for i in range(960))) for _ in range(6)]
            packets=[]
            for _ in range(6):
                clock.now+=.02; active, chunk=bridge.poll_voice(1,960);packets.extend(chunk)
                self.assertTrue(active)
            self.assertTrue(packets)
            decoded=b''.join(decoder.decode(p) for p in packets)
            self.assertTrue(any(decoded))
            self.assertFalse(bridge.receive_opus(1,b'x'))
            clock.now+=.31;active, packets=bridge.poll_voice(1,960)
            self.assertFalse(active);self.assertFalse(adapter.playing);self.assertEqual(packets,[])
            bridge.interrupt(1)
            before=len(adapter.output);self.assertEqual(bridge.poll_voice(1,960),(False,[]))
            self.assertEqual(len(adapter.output),before)
        finally:
            bridge.close();decoder.close()

    @unittest.skipIf(native is None, 'implementation missing')
    def test_revoked_speaking_turn_stops_before_next_native_pull(self):
        fixture=streaming.Tests();fixture.setUp()
        pulls=[]
        fixture.bridge.poll_voice=lambda epoch,frames:(pulls.append(frames) or True,[b'packet'])
        session=native.NativeConversationSession(fixture.registry,fixture.session.sessions,fixture.bridge,
            fixture.token,fixture.device.id,fixture.arms.append,clock=lambda:0,fixture=True)
        session.begin('native-turn')
        session.control(dict(type='ack',epoch=1))
        session.control(dict(type='consent',epoch=1,physical=True))
        session.playing(True)
        self.assertTrue(session.audio_tick(960)[1])
        session.sessions.interrupt_turn(session.turn.conversation_id,session.turn.id)
        self.assertEqual(session.audio_tick(960),(False,[]))
        self.assertEqual(pulls,[960]);self.assertFalse(session.armed)
        self.assertEqual(fixture.bridge.interrupts,[1])

    @unittest.skipIf(native is None, 'implementation missing')
    def test_native_cleanup_attempts_every_component_after_failure(self):
        closed=[]
        def component(name,fail=False):
            def close():
                closed.append(name)
                if fail:raise RuntimeError(name)
            return SimpleNamespace(close=close)
        adapter=component('adapter',True);adapter.muted=True
        bridge=native.NativeVoiceBridge(adapter,component('decoder',True),component('encoder'),component('up'),component('down'))
        bridge.pending.extend(b'pending')
        with self.assertRaisesRegex(RuntimeError,'adapter'):bridge.close()
        self.assertEqual(closed,['adapter','decoder','encoder','up','down'])
        self.assertEqual(bridge.pending,b'');self.assertFalse(bridge.closed)
        with self.assertRaisesRegex(RuntimeError,'adapter'):bridge.close()
        self.assertEqual(closed,['adapter','decoder','encoder','up','down','adapter','decoder'])

class DriverTests(unittest.IsolatedAsyncioTestCase):
    async def test_native_return_is_pumped_even_while_listening(self):
        socket=gc.Socket();session=gc.Session();session.armed=True;called=[]
        def tick(frames):called.append(frames);return False,[]
        session.audio_tick=tick
        with self.assertRaises(TimeoutError):await gc.run_connection(socket,session,duration=.04)
        self.assertTrue(called, 'native return must drain while listening')
        self.assertTrue(socket.closed);self.assertTrue(session.closed)

    async def test_timed_completion_preserves_cleanup_warning_without_audio_failure(self):
        result=await tt.FaultEvidenceTests().run_fixture(heartbeats=True,close_failure=True)
        self.assertTrue(result['cue_complete']);self.assertTrue(result['mic_complete'])
        self.assertEqual(result['outcome'],'complete_with_cleanup_fault')
        self.assertEqual(result['diagnostics']['phase'],'complete')
        self.assertNotIn('fault',result['diagnostics'])
        self.assertEqual(result['diagnostics']['cleanup_fault']['phase'],'socket_cleanup')

    async def test_provider_close_failure_preserves_completed_conversation(self):
        from home_lab.devices import DeviceRegistry
        from home_lab.sessions import ConversationSessions
        from websockets.datastructures import Headers
        from remote_gateway import RemoteGateway,QualifiedProvider
        registry=DeviceRegistry()
        device,token=registry.redeem_pairing_code(registry.create_pairing_code(),device_id='native-cleanup',label='Synthetic',capabilities=('microphone','speaker'))
        def fail_close():raise RuntimeError('synthetic provider close failure')
        async def close(**kwargs):pass
        socket=SimpleNamespace(request=SimpleNamespace(path='/companion/audio',headers=Headers([('Authorization','Bearer '+token),('X-Companion-Device',device.id)])),close=close)
        runtime=RemoteGateway(registry,ConversationSessions(),lambda _:QualifiedProvider(streaming.Bridge(),lambda _:None,fail_close,'confirmed-call',True,'existing_mira'))
        async def completed(socket,session,**kwargs):
            session.on_complete();raise native.TransportComplete('synthetic completion')
        with patch('remote_gateway.run_connection',completed):await runtime.handle(socket)
        result=runtime.last_result
        self.assertEqual(result['outcome'],'complete_with_cleanup_fault')
        self.assertTrue(result['conversation_complete']);self.assertIn(device.id,runtime._active)
        self.assertEqual(result['diagnostics']['cleanup_fault_count'],1)
        self.assertEqual(result['diagnostics']['cleanup_fault']['phase'],'session_cleanup')
        self.assertNotIn('fault',result['diagnostics'])

if __name__=='__main__':unittest.main()
