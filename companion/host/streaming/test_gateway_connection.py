import asyncio
import unittest
from types import SimpleNamespace
from gateway_connection import run_connection

class Socket:
    def __init__(self, messages=(), block_send=False):
        self.queue=asyncio.Queue()
        for m in messages:self.queue.put_nowait(m)
        self.sent=[];self.closed=False;self.pending=0;self.peak=0;self.block_send=block_send
    async def recv(self):
        self.pending+=1;self.peak=max(self.peak,self.pending)
        try:return await self.queue.get()
        finally:self.pending-=1
    async def send(self,m):
        if self.block_send:await asyncio.Event().wait()
        self.sent.append(m)
    async def close(self):self.closed=True
class Session:
    def __init__(self):self.armed=False;self.closed=False;self.controls=[];self.audio=[];self.packets=[];self.revoked=False;self.frames=[];self.epoch=1;self.bridge=SimpleNamespace(adapter=SimpleNamespace(playing=False))
    def _authorized(self):
        if self.revoked:raise PermissionError('revoked')
    def control(self,value):self.controls.append(value);return {'type':'acknowledged','epoch':1}
    def receive(self,wire):self.audio.append(wire)
    def outgoing(self,frames):self.frames.append(frames);return self.packets
    def playing(self,value):return {"type":"playing","epoch":self.epoch,"value":value}
    def close(self):self.closed=True;self.armed=False
class Tests(unittest.IsolatedAsyncioTestCase):
    async def check_closed(self,socket,session):
        self.assertTrue(socket.closed);self.assertTrue(session.closed);self.assertEqual(socket.pending,0);self.assertLessEqual(socket.peak,1)
    async def test_persistent_connection_keeps_revocation_and_cancel_cleanup(self):
        w=Socket();s=Session();t=asyncio.create_task(run_connection(w,s,duration=None))
        await asyncio.sleep(.03)
        self.assertFalse(t.done())
        s.revoked=True
        with self.assertRaises(PermissionError):await t
        await self.check_closed(w,s)
    async def test_controls_and_binary_dispatch_until_deadline(self):
        w=Socket(['{"type":"ack","epoch":1}',b'x'*17]);s=Session()
        with self.assertRaises(TimeoutError):await run_connection(w,s,duration=.04)
        self.assertEqual(len(s.controls),1);self.assertEqual(s.audio,[b'x'*17]);self.assertEqual(len(w.sent),1);await self.check_closed(w,s)
    async def test_duplicate_json_closes_without_dispatch(self):
        w=Socket(['{"epoch":1,"epoch":2}']);s=Session()
        with self.assertRaises(ValueError):await run_connection(w,s,duration=.1)
        self.assertEqual(s.controls,[]);await self.check_closed(w,s)
    async def test_oversize_wire_rejected_before_session(self):
        w=Socket([b'x'*4113]);s=Session()
        with self.assertRaises(ValueError):await run_connection(w,s,duration=.1)
        self.assertEqual(s.audio,[]);await self.check_closed(w,s)
    async def test_send_timeout_invalidates_audio(self):
        w=Socket(['{"type":"ack","epoch":1}'],block_send=True);s=Session()
        with self.assertRaises(TimeoutError):await run_connection(w,s,duration=.1,send_timeout=.01)
        await self.check_closed(w,s)
    async def test_idle_peer_revocation_checked_without_receive(self):
        w=Socket();s=Session();t=asyncio.create_task(run_connection(w,s,duration=.3))
        await asyncio.sleep(.03);s.revoked=True
        with self.assertRaises(PermissionError):await t
        await self.check_closed(w,s)
    async def test_cancel_cleans_receive_and_audio(self):
        w=Socket();s=Session();t=asyncio.create_task(run_connection(w,s,duration=.3))
        await asyncio.sleep(.01);t.cancel()
        with self.assertRaises(asyncio.CancelledError):await t
        await self.check_closed(w,s)
    async def test_return_packets_bounded_and_no_drain_when_muted(self):
        w=Socket();s=Session();s.packets=[b'x'*17]*3;s.armed=True;s.bridge.adapter.playing=True
        with self.assertRaises(BufferError):await run_connection(w,s,duration=.1)
        self.assertEqual(len(w.sent),1);self.assertIsInstance(w.sent[0],str);self.assertEqual(s.frames,[960]);await self.check_closed(w,s)
        w=Socket();s=Session();s.packets=[b'x'*17]
        with self.assertRaises(TimeoutError):await run_connection(w,s,duration=.03)
        self.assertEqual(s.frames,[]);self.assertEqual(w.sent,[]);await self.check_closed(w,s)
    async def test_listening_does_not_stream_encoded_silence(self):
        w=Socket();s=Session();s.armed=True;s.packets=[b'x'*17]
        with self.assertRaises(TimeoutError):await run_connection(w,s,duration=.04)
        self.assertEqual(s.frames,[]);self.assertEqual(len(w.sent),1)
        self.assertIn('"value":false',w.sent[0]);await self.check_closed(w,s)
    async def test_provider_failure_closes_connection(self):
        w=Socket([b'x'*17]);s=Session()
        def fail(_):raise RuntimeError('provider fixture failure')
        s.receive=fail
        with self.assertRaises(RuntimeError):await run_connection(w,s,duration=.1)
        await self.check_closed(w,s)
class ActualSessionTests(unittest.IsolatedAsyncioTestCase):
    async def test_existing_session_requires_real_protocol_consent(self):
        from test_streaming import Tests as SessionFixture
        from streaming_session import encode_frame
        f=SessionFixture();f.setUp()
        w=Socket([encode_frame(1,0,False,b'x')])
        with self.assertRaises(PermissionError):await run_connection(w,f.session,duration=.1)
        self.assertEqual(f.bridge.input,[]);self.assertEqual(f.arms,[]);self.assertTrue(w.closed)
    async def test_existing_session_controls_and_cleanup(self):
        from test_streaming import Tests as SessionFixture
        from streaming_session import encode_frame
        f=SessionFixture();f.setUp()
        w=Socket(['{"type":"begin","request_id":"synthetic-gateway"}', '{"type":"ack","epoch":1}', '{"type":"consent","epoch":1,"physical":true}', encode_frame(1,0,False,b'x')])
        # Expire the synthetic watchdog only after protocol dispatch succeeds;
        # a 40ms wall-clock race could end before the queued microphone frame.
        original_receive=f.session.receive
        def receive_then_expire(frame):
            original_receive(frame)
            f.clock[0]=2.
        f.session.receive=receive_then_expire
        async with asyncio.timeout(2):
            with self.assertRaises(TimeoutError):await run_connection(w,f.session,duration=1)
        self.assertEqual(f.arms,[1]);self.assertEqual(f.session.mic_seq,1)
        self.assertEqual(f.bridge.interrupts,[1]);self.assertEqual(f.session.credential,'');self.assertTrue(w.closed)
if __name__=='__main__':unittest.main()
