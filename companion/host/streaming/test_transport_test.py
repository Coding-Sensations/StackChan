"""Synthetic transport-only authority/codec tests; no listener or live grant."""
import importlib.util
import ssl
import struct
import unittest
from home_lab.devices import DeviceRegistry
from home_lab.sessions import ConversationSessions
from codec import OpusCodec
from streaming_session import decode_frame, encode_frame
if importlib.util.find_spec('transport_test'):
    import transport_test
else:
    transport_test=None

DEVICE='stackchan-001122334455'
class Clock:
    def __init__(self):self.now=0.
    def __call__(self):return self.now

class TransportTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(transport_test,'dedicated transport-only session is not implemented')
        self.registry=DeviceRegistry()
        code=self.registry.create_pairing_code()
        _,self.token=self.registry.redeem_pairing_code(code,device_id=DEVICE,label='Synthetic transport test',capabilities=('microphone','speaker'))
        self.sessions=ConversationSessions();self.clock=Clock();self.opened=[]
        self.cue=struct.pack('<1440h',*([900]*1440))
    def tearDown(self):
        for session in getattr(self,'opened',[]):session.close()
    def session(self,**kwargs):
        result=transport_test.TransportSession(self.registry,self.sessions,self.token,DEVICE,self.cue,
            transport_approved=True,clock=self.clock,mic_duration=.25,**kwargs)
        self.opened.append(result);return result
    def arm(self,session):
        session.begin('synthetic-transport-request')
        session.control({'type':'ack','epoch':1})
        session.control({'type':'consent','epoch':1,'physical':True})
    def packet(self):
        with OpusCodec(16000) as codec:return codec.encode(struct.pack('<960h',*([3000]*960)))

    def test_session_hello_is_firmware_compatible_without_mira_reference(self):
        session=self.session();hello=session.begin('synthetic-transport-request')
        self.assertFalse(hello['fixture']);self.assertTrue(hello['transport_only'])
        self.assertFalse(hello['identity_verified'])
        self.assertEqual(hello['audio'],{'uplink_rate':16000,'downlink_rate':24000,'channels':1,'frame_ms':60})
        self.assertEqual(session.conversation.connection_id,'stackchan.transport_test')
        self.assertIsNone(session.conversation.external_session_id)
        self.assertNotIn('mira',str(hello).lower())

    def test_approval_and_existing_device_authority_are_required(self):
        with self.assertRaises(PermissionError):
            transport_test.TransportSession(self.registry,self.sessions,self.token,DEVICE,self.cue,clock=self.clock)
        self.registry.revoke(DEVICE)
        with self.assertRaises(PermissionError):self.session()

    def test_no_cue_or_mic_before_ack_and_physical_consent(self):
        session=self.session();session.begin('synthetic-transport-request')
        self.assertEqual(session.outgoing(),[])
        with self.assertRaises(PermissionError):session.receive(encode_frame(1,0,False,self.packet()))
        with self.assertRaises(PermissionError):session.control({'type':'consent','epoch':1,'physical':True})
        self.assertEqual(session.statistics()['speaker_packets'],0)

    def test_finite_cue_has_playing_state_and_real_24k_opus(self):
        session=self.session();self.arm(session)
        self.assertTrue(session.bridge.adapter.playing)
        self.assertEqual(session.playing(True),{'type':'playing','epoch':1,'value':True})
        packets=session.outgoing(960);self.assertEqual(len(packets),1)
        epoch,seq,speaker,payload=decode_frame(packets[0]);self.assertEqual((epoch,seq,speaker),(1,0,True))
        with OpusCodec(24000) as codec:self.assertEqual(len(codec.decode(payload)),2880)
        self.assertEqual(session.outgoing(960),[])
        self.clock.now=.32
        self.assertFalse(session.bridge.adapter.playing)
        self.assertEqual(session.playing(False)['value'],False)
        self.assertEqual(session.outgoing(960),[])

    def test_speaking_mic_is_discarded_then_listening_mic_is_counted(self):
        session=self.session();self.arm(session)
        session.receive(encode_frame(1,0,False,self.packet()))
        self.assertEqual(session.statistics()['suppressed_mic_packets'],1)
        self.assertEqual(session.statistics()['decoded_mic_frames'],0)
        session.outgoing(960);self.clock.now=.32;session.playing(False)
        session.receive(encode_frame(1,1,False,self.packet()))
        stats=session.statistics();self.assertEqual(stats['decoded_mic_frames'],960)
        self.assertEqual(stats['decoded_mic_packets'],1)
        self.assertFalse(stats['identity_verified']);self.assertTrue(stats['transport_only'])
        self.assertFalse(any(isinstance(value,(bytes,bytearray)) for value in stats.values()))

    def test_duplicate_consent_does_not_replay_cue_or_allow_second_turn(self):
        session=self.session();self.arm(session);self.assertEqual(len(session.outgoing(960)),1)
        session.control({'type':'consent','epoch':1,'physical':True})
        self.assertEqual(session.outgoing(960),[])
        with self.assertRaises(PermissionError):session.begin('second-request')
        self.assertEqual(session.statistics()['speaker_packets'],1)

    def test_mic_phase_completes_with_finite_deadline(self):
        session=self.session();self.arm(session);session.outgoing(960)
        self.clock.now=.32;session.playing(False);self.clock.now=.58
        with self.assertRaises(transport_test.TransportComplete):session.outgoing()
        self.assertTrue(session.statistics()['mic_complete'])

    def test_cue_and_listener_limits_are_enforced_without_binding(self):
        with self.assertRaises(ValueError):
            transport_test.TransportGateway(self.registry,self.sessions,b'\0'*(24000*10*2+2),transport_approved=True)
        gateway=transport_test.TransportGateway(self.registry,self.sessions,self.cue,transport_approved=True)
        tls=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        for kwargs in [{'ssl_context':None},{'ssl_context':tls,'host':'0.0.0.0'},
                       {'ssl_context':tls,'port':18444},{'ssl_context':tls,'duration':120.01}]:
            with self.subTest(kwargs=kwargs),self.assertRaises(ValueError):gateway.listener(**kwargs)
        context=gateway.listener(ssl_context=tls)
        self.assertTrue(hasattr(context,'__aenter__'))
        self.assertIsInstance(gateway.last_result,dict)
        self.assertEqual(gateway.last_result['speaker_packets'],0)

    def test_revocation_and_sequence_replay_still_fail_closed(self):
        session=self.session();self.arm(session);session.outgoing(960);self.clock.now=.32;session.playing(False)
        frame=encode_frame(1,0,False,self.packet());session.receive(frame)
        with self.assertRaises(ValueError):session.receive(frame)
        self.assertFalse(session.armed)
        self.registry.revoke(DEVICE)
        with self.assertRaises(PermissionError):session.outgoing()

    def test_remote_mira_gateway_still_rejects_unqualified_transport_provider(self):
        from remote_gateway import RemoteGateway
        import asyncio
        class Headers:
            def raw_items(inner):return [('Authorization','Bearer '+self.token),('X-Companion-Device',DEVICE)]
        class Socket:
            request=type('Request',(),{'path':'/companion/audio','headers':Headers()})()
            def __init__(inner):inner.closed=[]
            async def close(inner,**kwargs):inner.closed.append(kwargs)
        socket=Socket();gateway=RemoteGateway(self.registry,self.sessions,lambda _:object())
        asyncio.run(gateway.handle(socket))
        self.assertEqual(socket.closed[0]['code'],1008)

    def test_decoded_microphone_energy_is_numeric_without_recording(self):
        session=self.session();self.arm(session);session.outgoing(960);self.clock.now=.32;session.playing(False)
        session.receive(encode_frame(1,0,False,self.packet()))
        stats=session.statistics()
        self.assertGreater(stats.get('decoded_mic_peak',0),0)
        self.assertGreater(stats.get('decoded_mic_energy',0),0)
        self.assertEqual(stats['decoded_mic_frames'],960)
        self.assertFalse(any(isinstance(value,(bytes,bytearray,memoryview)) for value in stats.values()))

    def test_zero_connection_receipt_never_claims_consent_or_voice(self):
        gateway=transport_test.TransportGateway(self.registry,self.sessions,self.cue,transport_approved=True)
        self.assertIsInstance(gateway.last_result,dict)
        self.assertEqual(gateway.last_result['outcome'],'no_connection')
        self.assertFalse(gateway.last_result['consent_received'])
        self.assertEqual(gateway.last_result['decoded_mic_frames'],0)
        self.assertFalse(gateway.last_result['identity_verified'])

    def test_interrupted_microphone_phase_does_not_later_become_complete(self):
        session=self.session();self.arm(session);session.outgoing(960);self.clock.now=.32;session.playing(False)
        session.control({'type':'interrupt','epoch':1});self.clock.now=.58
        self.assertFalse(session.statistics()['mic_complete'])

class AcceptedSocketTests(unittest.IsolatedAsyncioTestCase):
    async def test_actual_driver_orders_playing_cue_and_counted_mic_without_listener(self):
        import asyncio,json
        self.assertIsNotNone(transport_test)
        registry=DeviceRegistry();_,token=registry.redeem_pairing_code(registry.create_pairing_code(),
            device_id=DEVICE,label='Synthetic accepted socket',capabilities=('microphone','speaker'))
        clock=Clock();cue=struct.pack('<1440h',*([900]*1440))
        gateway=transport_test.TransportGateway(registry,ConversationSessions(),cue,
            transport_approved=True,mic_duration=.25,clock=clock)
        class Headers:
            def raw_items(self):return [('Authorization','Bearer '+token),('X-Companion-Device',DEVICE)]
        class Socket:
            request=type('Request',(),{'path':'/companion/audio','headers':Headers()})()
            def __init__(self):
                self.queue=asyncio.Queue();self.sent=[];self.closed=False
                self.cue_sent=asyncio.Event();self.listening=asyncio.Event()
                for control in [{'type':'begin','request_id':'synthetic-accepted-socket'},
                                {'type':'ack','epoch':1},{'type':'consent','epoch':1,'physical':True}]:
                    self.queue.put_nowait(json.dumps(control))
            async def recv(self):return await self.queue.get()
            async def send(self,message):
                self.sent.append(message)
                if type(message) is bytes:self.cue_sent.set()
                elif json.loads(message).get('type')=='playing' and json.loads(message)['value'] is False:
                    self.listening.set()
            async def close(self,**kwargs):self.closed=True
        socket=Socket()
        async def physical_fixture():
            await socket.cue_sent.wait();clock.now=.32
            await socket.listening.wait()
            with OpusCodec(16000) as codec:packet=codec.encode(struct.pack('<960h',*([3000]*960)))
            await socket.queue.put(encode_frame(1,0,False,packet))
            await asyncio.sleep(.05);clock.now=.58
        async with asyncio.timeout(2):
            await asyncio.gather(gateway.handle(socket),physical_fixture())
        self.assertTrue(socket.closed)
        result=gateway.last_result
        self.assertEqual(result['outcome'],'complete')
        self.assertEqual(result['speaker_packets'],1)
        self.assertEqual(result['decoded_mic_frames'],960)
        self.assertGreater(result['decoded_mic_energy'],0)
        self.assertFalse(result['identity_verified']);self.assertFalse(result['robot_playback_verified'])
        wire_order=[]
        for message in socket.sent:
            if type(message) is bytes:wire_order.append('cue')
            else:
                control=json.loads(message)
                if control['type']=='playing':wire_order.append('playing_true' if control['value'] else 'playing_false')
                if control['type']=='begin':self.assertFalse(control['fixture']);self.assertTrue(control['transport_only'])
        self.assertEqual(wire_order,['playing_true','cue','playing_false'])

class FaultEvidenceTests(unittest.IsolatedAsyncioTestCase):
    async def run_fixture(self,heartbeats=False,send_failure=None,close_failure=False):
        import asyncio,json
        registry=DeviceRegistry();_,token=registry.redeem_pairing_code(registry.create_pairing_code(),device_id=DEVICE,label='Synthetic diagnostic socket',capabilities=('microphone','speaker'))
        clock=Clock();cue=struct.pack('<81600h',*([900]*81600))
        gateway=transport_test.TransportGateway(registry,ConversationSessions(),cue,transport_approved=True,mic_duration=.25,clock=clock)
        class Headers:
            def raw_items(self):return [('Authorization','Bearer '+token),('X-Companion-Device',DEVICE)]
        class Socket:
            request=type('Request',(),{'path':'/companion/audio','headers':Headers()})()
            def __init__(self):
                self.queue=asyncio.Queue();self.attempted=0;self.closed=False
                for value in ({'type':'begin','request_id':'synthetic-diagnostic'}, {'type':'ack','epoch':1}, {'type':'consent','epoch':1,'physical':True}):self.queue.put_nowait(json.dumps(value))
            async def recv(self):return await self.queue.get()
            async def send(self,value):
                if type(value) is bytes:
                    self.attempted+=1
                    if self.attempted==send_failure:raise OSError('synthetic-secret-send-fault')
                    clock.now+=.31 if self.attempted==57 else .06
                    if heartbeats and self.attempted%5==0:self.queue.put_nowait(json.dumps({'type':'heartbeat','epoch':1}))
                else:
                    control=json.loads(value)
                    clock.now+=.30 if control.get('type')=='playing' and control.get('value') is False else .01
            async def close(self,**kwargs):
                self.closed=True
                if close_failure:raise RuntimeError('synthetic-secret-close-fault')
        socket=Socket()
        async with asyncio.timeout(4):await gateway.handle(socket)
        self.assertTrue(socket.closed)
        self.assertNotIn('synthetic-secret',json.dumps(gateway.last_result))
        return gateway.last_result
    async def test_no_heartbeat_synthetic_watchdog_around_33_packets(self):
        result=await self.run_fixture();diag=result['diagnostics']
        self.assertEqual(result['outcome'],'fault');self.assertEqual(diag['fault']['type'],'TimeoutError');self.assertEqual(diag['fault']['phase'],'authority')
        self.assertEqual(diag['heartbeat_count'],0);self.assertEqual(diag['heartbeat_remaining_ms'],0)
        self.assertTrue(diag['ack_received']);self.assertTrue(diag['consent_received']);self.assertEqual(diag['last_control'],'consent')
        self.assertTrue(32<=result['speaker_packets']<=34);self.assertEqual(diag['sent_speaker_packets'],result['speaker_packets'])
        self.assertTrue(any(frame['file']=='streaming_session.py' and frame['function']=='_authorized' for frame in diag['fault']['stack']))
    async def test_valid_heartbeats_complete_same_cue_without_relaxing_watchdog(self):
        result=await self.run_fixture(heartbeats=True);diag=result['diagnostics']
        self.assertEqual(result['outcome'],'complete');self.assertEqual(result['speaker_packets'],57);self.assertEqual(diag['sent_speaker_packets'],57)
        self.assertGreater(diag['heartbeat_count'],0);self.assertNotIn('fault',diag);self.assertEqual(diag['cleanup_fault_count'],0)
    async def test_send_fault_before_33rd_send_is_not_cleanup_fault(self):
        result=await self.run_fixture(send_failure=33,close_failure=True);diag=result['diagnostics']
        self.assertEqual(result['speaker_packets'],33);self.assertEqual(diag['sent_speaker_packets'],32)
        self.assertEqual(diag['fault']['type'],'OSError');self.assertEqual(diag['fault']['phase'],'audio_send')
        self.assertEqual(diag['cleanup_fault']['type'],'RuntimeError');self.assertEqual(diag['cleanup_fault']['phase'],'socket_cleanup')
    async def test_pre_session_admission_fault_survives_close_fault(self):
        import json
        registry=DeviceRegistry();gateway=transport_test.TransportGateway(registry,ConversationSessions(),b'\0'*2880,transport_approved=True)
        class Headers:
            def raw_items(self):return []
        class Socket:
            request=type('Request',(),{'path':'/companion/audio','headers':Headers()})()
            async def close(self,**kwargs):raise OSError('synthetic-secret-close')
        await gateway.handle(Socket());diag=gateway.last_result['diagnostics']
        self.assertEqual(diag['fault']['type'],'PermissionError');self.assertEqual(diag['fault']['phase'],'admission');self.assertEqual(diag['cleanup_fault']['type'],'OSError')
        self.assertEqual(gateway.last_result['speaker_packets'],0);self.assertNotIn('synthetic-secret',json.dumps(gateway.last_result))
if __name__=='__main__':unittest.main()
