import unittest
import test_streaming
from streaming_session import encode_frame
from desk_robot_session import DeskRobotSession
from desk_robot.device_playback import DevicePlaybackEndpoint,DrainStatus

class Driver:
    def __init__(self):self.packets=[];self.input=False;self.output_frames=0;self.drained=False;self.stopped=False
    def enqueue(self,epoch,sequence,payload):self.packets.append((epoch,sequence,payload));return True
    def drain_status(self,epoch):return DrainStatus(len(self.packets),0,self.output_frames,self.drained)
    def hard_mute(self):self.input=False;self.packets.clear();self.output_frames=0;self.stopped=True

class Tests(unittest.TestCase):
    def setUp(self):
        self.f=test_streaming.Tests();self.f.setUp()
        self.s=DeskRobotSession(self.f.registry,self.f.session.sessions,self.f.bridge,self.f.token,self.f.device.id,self.f.arms.append,clock=lambda:self.f.clock[0],fixture=True)
        self.hello=self.s.begin('desk-request');self.e=self.s.epoch
        self.d=Driver();self.peer=DevicePlaybackEndpoint(self.d,self.hello)
    def arm(self):
        self.s.control({'type':'ack','epoch':self.e});self.s.control(self.peer.contract_ack())
        self.s.control({'type':'consent','epoch':self.e,'physical':True})
    def speaking(self):self.arm();self.peer.control(self.s.playing(True))
    def end(self):
        packets=self.s.outgoing(960)
        for wire in packets:self.peer.receive(wire)
        self.peer.control(self.s.playing(False));return packets
    def test_consent_replay_never_rearms_in_any_active_phase(self):
        for phase in ('listening','speaking','draining'):
            self.setUp();self.arm()
            if phase!='listening':self.peer.control(self.s.playing(True))
            if phase=='draining':self.end()
            before=list(self.f.arms)
            with self.assertRaises(PermissionError):self.s.control({'type':'consent','epoch':self.e,'physical':True})
            self.assertEqual(self.f.arms,before)
    def test_bad_control_closes_and_retains_failed_cleanup_for_retry(self):
        for fault in ('wrong_end','stale_epoch','malformed'):
            self.setUp();self.speaking();self.peer.receive(self.s.outgoing()[0])
            value=self.peer._message('playback_end',final_sequence=99)
            if fault=='stale_epoch':value['epoch']=999
            if fault=='malformed':value['extra']=True
            with self.assertRaises(PermissionError):self.peer.control(value)
            self.assertTrue(self.d.stopped);self.assertEqual(self.d.packets,[])
            self.assertIsNone(self.peer.poll_drained())
        self.setUp();self.speaking();self.peer.receive(self.s.outgoing()[0])
        mute=self.d.hard_mute
        def fail():raise RuntimeError('synthetic close failure')
        self.d.hard_mute=fail
        with self.assertRaises(RuntimeError):self.peer.control({'type':'bad'})
        self.assertIsNone(self.peer.poll_drained())
        self.d.hard_mute=mute;self.peer.disconnect();self.assertTrue(self.d.stopped)
    def test_bridge_stop_failure_cannot_leave_session_armed_or_complete(self):
        self.speaking();self.end();self.d.packets.clear();self.d.drained=True
        receipt=self.peer.poll_drained()
        def fail(epoch):raise RuntimeError('synthetic stop failure')
        self.f.bridge.interrupt=fail
        with self.assertRaises(RuntimeError):self.s.control(receipt)
        self.assertFalse(self.s.armed)
        self.assertNotEqual(self.s.sessions.get_turn(self.s.turn.id).status,'completed')
    def test_device_decoder_queue_bound_is_enforced_before_enqueue(self):
        self.speaking()
        self.peer.receive(self.s.outgoing()[0]);self.peer.receive(self.s.outgoing()[0])
        with self.assertRaises(BufferError):self.peer.receive(self.s.outgoing()[0])
        self.assertTrue(self.d.stopped);self.assertEqual(self.d.packets,[])
    def test_retired_binding_cannot_listen_after_new_contract_or_request_replay(self):
        from desk_robot.session_contract import TurnContract
        self.arm();binding=self.s.contract.binding;self.s.close()
        clone=TurnContract(binding,lambda b:self.s._active(),clock=lambda:0.)
        with self.assertRaises(PermissionError):clone.physical_listen(binding,physical=True)
        with self.assertRaises(PermissionError):self.s.begin('desk-request')
        self.assertFalse(self.s.armed);self.assertEqual(self.f.arms,[1])
    def test_version_ack_required_before_physical_arm(self):
        self.s.control({'type':'ack','epoch':self.e})
        with self.assertRaises(PermissionError):self.s.control({'type':'consent','epoch':self.e,'physical':True})
        self.assertEqual(self.f.arms,[])
    def test_connect_and_negotiation_never_arm_capture(self):
        self.s.control({'type':'ack','epoch':self.e});self.s.control(self.peer.contract_ack())
        self.assertFalse(self.s.armed);self.assertFalse(self.d.input);self.assertEqual(self.f.arms,[])
    def test_actual_driver_drain_required_and_turn_finishes_once(self):
        self.speaking();self.end();self.assertIsNone(self.peer.poll_drained())
        self.d.packets.clear();self.d.output_frames=960;self.assertIsNone(self.peer.poll_drained())
        self.d.output_frames=0;self.assertIsNone(self.peer.poll_drained())
        self.d.drained=True;receipt=self.peer.poll_drained()
        self.assertIsNotNone(receipt);self.assertIsNone(self.peer.poll_drained())
        answer=self.s.control(receipt);self.assertEqual(answer['type'],'hard_mute');self.assertFalse(self.s.armed)
        self.assertEqual(self.s.sessions.get_turn(self.s.turn.id).status,'completed')
        self.peer.control(answer);self.assertTrue(self.d.stopped)
    def test_timer_never_becomes_a_drain_receipt(self):
        self.speaking();self.end();self.f.clock[0]=2.0;self.assertIsNone(self.peer.poll_drained())
        with self.assertRaises(TimeoutError):self.s.outgoing()
        self.assertFalse(self.s.armed)
    def test_stale_receipt_and_media_rejected_on_replacement(self):
        self.speaking();self.end();self.s.begin('replacement')
        with self.assertRaises(PermissionError):self.s.control(self.peer._message('playback_drained',final_sequence=0))
        with self.assertRaises(PermissionError):self.s.receive(encode_frame(self.e,0,False,b'old'))
        self.assertFalse(self.s.armed)
    def test_mic_is_blocked_through_speaking_and_drain(self):
        self.speaking()
        with self.assertRaises(PermissionError):self.s.receive(encode_frame(self.e,0,False,b'mic'))
        self.end()
        with self.assertRaises(PermissionError):self.s.receive(encode_frame(self.e,0,False,b'mic'))
        self.assertEqual(self.f.bridge.input,[])
    def test_cancel_invalidates_both_sides_without_completing(self):
        self.speaking();self.end();self.peer.disconnect();self.s.close();self.d.drained=True
        self.assertIsNone(self.peer.poll_drained());self.assertFalse(self.s.armed);self.assertTrue(self.d.stopped)
        self.assertNotEqual(self.s.sessions.get_turn(self.s.turn.id).status,'completed')
    def test_driver_unavailable_drain_fails_closed(self):
        self.speaking();self.end();self.d.packets.clear();self.d.drain_status=lambda epoch:None
        self.assertIsNone(self.peer.poll_drained())
    def test_queue_rejection_mutes_and_invalidates(self):
        self.speaking();self.d.enqueue=lambda *args:False
        with self.assertRaises(BufferError):self.peer.receive(self.s.outgoing()[0])
        self.assertTrue(self.d.stopped);self.assertIsNone(self.peer.poll_drained())
    def test_duplicate_packets_and_malformed_receipts_fail_closed(self):
        self.speaking();wire=self.s.outgoing()[0];self.peer.receive(wire)
        with self.assertRaises(ValueError):self.peer.receive(wire)
        self.assertTrue(self.d.stopped)
        with self.assertRaises(ValueError):DrainStatus(True,0,0,True)

class ConnectionTests(unittest.IsolatedAsyncioTestCase):
    async def test_existing_connection_loop_completes_only_after_fake_driver_drain(self):
        import asyncio,json
        from gateway_connection import run_connection,TransportComplete
        f=Tests();f.setUp()
        f.f.bridge.adapter.playing=True
        class PeerSocket:
            def __init__(self):
                self.queue=asyncio.Queue();self.closed=False;self.sent=[]
                for message in ({'type':'ack','epoch':f.e},f.peer.contract_ack(),{'type':'consent','epoch':f.e,'physical':True}):
                    self.queue.put_nowait(json.dumps(message))
            async def recv(self):return await self.queue.get()
            async def send(self,value):
                self.sent.append(value)
                if type(value) is bytes:
                    f.peer.receive(value);f.f.bridge.adapter.playing=False
                else:
                    message=json.loads(value)
                    if message['type'] in ('playing','playback_end','hard_mute'):f.peer.control(message)
                    else:assert message['type'] in ('acknowledged','state_contract_ready','armed')
                    if message['type']=='playback_end':
                        assert f.peer.poll_drained() is None
                        # Fake driver completion is explicit, not a timed sleep.
                        f.d.packets.clear();f.d.output_frames=0;f.d.drained=True
                        self.queue.put_nowait(json.dumps(f.peer.poll_drained()))
            async def close(self):self.closed=True;f.peer.disconnect()
        socket=PeerSocket()
        with self.assertRaises(TransportComplete):await run_connection(socket,f.s,duration=.5)
        self.assertTrue(socket.closed);self.assertFalse(f.s.armed)
        self.assertEqual(f.s.sessions.get_turn(f.s.turn.id).status,'completed')
        self.assertTrue(any(type(x) is str and 'playback_end' in x for x in socket.sent))

if __name__=='__main__':unittest.main()
