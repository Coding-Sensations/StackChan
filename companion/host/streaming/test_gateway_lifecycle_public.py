"""Public startup/cleanup regressions with inert authority, audio and sockets."""
import asyncio
import ctypes
import threading
import unittest
from types import SimpleNamespace
from unittest.mock import patch

from gateway_connection import OwnedSocketClose, TransportComplete, run_connection
from native_conversation import DeferredNativeBridge, NativeConversationSession
from remote_gateway import QualifiedProvider, RemoteGateway
from test_gateway_connection import Socket


class Sessions:
    def __init__(self):self.turn=None
    def create(self, **kwargs):return SimpleNamespace(id='synthetic-conversation')
    def get_request(self, request):return None
    def begin_turn(self, conversation, **kwargs):
        self.turn=SimpleNamespace(id='synthetic-turn', generation=1, conversation_id=conversation)
        return self.turn
    def accepts_output(self, *args):return self.turn is not None
    def interrupt_turn(self, *args):self.turn=None
    def mark_interrupt_uncertain(self, *args):pass


class LifecycleTests(unittest.IsolatedAsyncioTestCase):
    async def test_batched_queue_misses_require_elapsed_stall_before_interrupt(self):
        from streaming_session import AudioBackpressure, SustainedAudioBackpressure, encode_frame
        now=[0.]
        def pressure(epoch, payload):raise AudioBackpressure('synthetic bounded queue')
        bridge=SimpleNamespace(adapter=SimpleNamespace(playing=False),begin=lambda epoch:None,
            interrupt=lambda epoch:None,receive_opus=pressure)
        registry=SimpleNamespace(authenticate=lambda token:'synthetic-device',
            get=lambda device:SimpleNamespace(status='active',capabilities=('microphone','speaker')))
        session=NativeConversationSession(registry,Sessions(),bridge,'synthetic-token','synthetic-device',lambda epoch:None,fixture=True,clock=lambda:now[0])
        session.begin('synthetic-request');session.control({'type':'ack','epoch':1})
        session.control({'type':'consent','epoch':1,'physical':True})
        for sequence in range(5):session.receive(encode_frame(1,sequence,False,b'x'))
        self.assertTrue(session.armed)
        now[0]=.31
        with self.assertRaises(SustainedAudioBackpressure):session.receive(encode_frame(1,5,False,b'x'))
        self.assertFalse(session.armed)
        session.begin('synthetic-next');session.control({'type':'ack','epoch':2})
        session.control({'type':'consent','epoch':2,'physical':True})
        for sequence in range(5):session.receive(encode_frame(2,sequence,False,b'x'))
        self.assertTrue(session.armed)
        session.close()

    async def test_native_prepare_keeps_heartbeats_and_commits_only_after_ready(self):
        now=[0.];release=threading.Event();calls=[]
        owner=SimpleNamespace(mic_packets=0,speaker_packets=0,return_frames=0,native_audio={})
        owner.adapter=SimpleNamespace(muted=True,playing=False)
        owner.begin=lambda epoch:calls.append(('begin',epoch))
        def arm(epoch):calls.append(('arm',epoch));owner.adapter.muted=False
        owner.adapter.arm_for_approved_test=arm
        owner.adapter.set_playing=lambda epoch,value:True
        owner.interrupt=lambda epoch:setattr(owner.adapter,'muted',True)
        owner.close=lambda:calls.append(('close',))
        owner.poll_voice=lambda epoch,frames:(False,[])
        def build():
            if not release.wait(2):raise RuntimeError('synthetic builder not released')
            return owner
        bridge=DeferredNativeBridge(build)
        registry=SimpleNamespace(authenticate=lambda token:'synthetic-device',
            get=lambda device:SimpleNamespace(status='active',capabilities=('microphone','speaker')))
        session=NativeConversationSession(registry,Sessions(),bridge,'synthetic-token','synthetic-device',bridge.arm,fixture=True,clock=lambda:now[0])
        session.begin('synthetic-request');session.control({'type':'ack','epoch':1})
        socket=Socket(['{"type":"consent","epoch":1,"physical":true}'])
        task=asyncio.create_task(run_connection(socket,session,duration=None))
        try:
            for tick in (.8,1.6,2.4):
                now[0]=tick;socket.queue.put_nowait('{"type":"heartbeat","epoch":1}')
                await asyncio.sleep(.03)
                self.assertFalse(task.done());self.assertFalse(session.armed)
            self.assertGreaterEqual(sum('heartbeat' in value for value in socket.sent),3)
            release.set()
            async with asyncio.timeout(1):
                while not session.armed:await asyncio.sleep(.005)
            self.assertEqual(calls.count(('arm',1)),1)
            self.assertTrue(any('"type":"armed"' in value for value in socket.sent))
        finally:
            release.set();task.cancel()
            with self.assertRaises(asyncio.CancelledError):await task
            async with asyncio.timeout(1):
                while not bridge.startup_done():await asyncio.sleep(.005)
            bridge.close()

    async def test_timeout_and_cancel_join_one_socket_close_task(self):
        release=asyncio.Event();calls=[]
        async def close():calls.append(True);await release.wait()
        owner=OwnedSocketClose(SimpleNamespace(close=close))
        with self.assertRaises(TimeoutError):await owner.close(timeout=.001)
        waiter=asyncio.create_task(owner.close())
        await asyncio.sleep(.001);waiter.cancel()
        with self.assertRaises(asyncio.CancelledError):await waiter
        self.assertFalse(owner.task.cancelled());self.assertEqual(calls,[True])
        release.set();await owner.close();self.assertTrue(owner.confirmed)
        self.assertEqual(calls,[True])

    async def test_admission_retries_retained_owner_before_replacement(self):
        events=[]
        provider=QualifiedProvider(object(),lambda epoch:None,lambda:None,'synthetic-route',True,'synthetic_fixture')
        runtime=RemoteGateway(None,None,lambda device:(events.append('provider') or provider),fixture=True,
            session_factory=lambda *args,**kwargs:SimpleNamespace(armed=False,statistics=lambda:{}))
        runtime._active.add('synthetic-device')
        runtime._retained['synthetic-device']=[('provider',SimpleNamespace(close=lambda:events.append('cleanup')))]
        async def close(**kwargs):pass
        async def complete(*args,**kwargs):raise TransportComplete()
        socket=SimpleNamespace(request=SimpleNamespace(path='/companion/audio',headers=SimpleNamespace(raw_items=lambda:[])),close=close)
        grant=SimpleNamespace(device_id='synthetic-device',credential='synthetic-token')
        with patch('remote_gateway.authorize_audio_request',return_value=grant),patch('remote_gateway.run_connection',complete):
            await runtime.handle(socket)
        self.assertEqual(events,['cleanup','provider']);self.assertFalse(runtime._active)


class DirectionalCancellationTests(unittest.TestCase):
    def test_cancel_before_during_open_and_after_start(self):
        from native_adapter import NativeHostAdapter
        for point in ('before','open','start'):
            with self.subTest(point=point):
                cancelled=threading.Event();calls=[]
                if point=='before':cancelled.set()
                class Function:
                    def __call__(self, handle):
                        calls.append('open')
                        ctypes.cast(handle,ctypes.POINTER(ctypes.c_void_p))[0]=ctypes.c_void_p(1)
                        if point=='open':cancelled.set()
                        return 0
                def start(handle):
                    calls.append('start')
                    if point=='start':cancelled.set()
                    return 0
                adapter=NativeHostAdapter.__new__(NativeHostAdapter);adapter.handle=ctypes.c_void_p()
                adapter.lib=SimpleNamespace(sc_open_directional=Function(),sc_start=start)
                def close():calls.append('close');adapter.handle=ctypes.c_void_p()
                adapter.close=close
                with self.assertRaisesRegex(RuntimeError,'cancelled'):
                    adapter.open_directional_for_approved_test(cancelled=cancelled.is_set)
                self.assertEqual(calls,[] if point=='before' else ['open','close'] if point=='open' else ['open','start','close'])
                self.assertFalse(adapter.handle.value)


if __name__=='__main__':unittest.main()
