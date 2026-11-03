"""No CoreAudio: deterministic blocked builders, fake watchdog clock and fake sockets."""
import asyncio
import json
import threading
import unittest
from types import SimpleNamespace
from home_lab.devices import DeviceRegistry
from home_lab.sessions import ConversationSessions
from native_conversation import DeferredNativeBridge,NativeConversationSession
from gateway_connection import run_connection
from remote_gateway import RemoteGateway,QualifiedProvider,ProviderCleanupError
from test_gateway_connection import Socket
from test_native_consent_boundary import Bridge
from websockets.datastructures import Headers

class AsyncStartupTests(unittest.IsolatedAsyncioTestCase):
    def setUp(self):
        self.assertTrue(callable(getattr(NativeConversationSession,'start_control',None)),'consent currently blocks the heartbeat driver')
        self.now=[0.];self.started=threading.Event();self.release=threading.Event();self.owner=Bridge();self.builds=[]
        def build():
            self.builds.append(True);self.started.set()
            if not self.release.wait(3):raise RuntimeError('test release missing')
            return self.owner
        self.bridge=DeferredNativeBridge(build)
        self.registry=DeviceRegistry();device,token=self.registry.redeem_pairing_code(self.registry.create_pairing_code(),device_id='synthetic-delayed',label='Synthetic',capabilities=('microphone','speaker'))
        self.device=device.id
        self.session=NativeConversationSession(self.registry,ConversationSessions(),self.bridge,token,device.id,self.bridge.arm,fixture=True,clock=lambda:self.now[0])
        self.session.control({'type':'begin','request_id':'synthetic-turn'})
        self.session.control({'type':'ack','epoch':1})
    async def asyncTearDown(self):
        if not hasattr(self,'release'):return
        self.release.set()
        for _ in range(100):
            try:self.session.close();self.bridge.close();break
            except RuntimeError:await asyncio.sleep(.001)
        else:self.fail('startup ownership not cleanly released')
    def consent(self):return self.session.start_control({'type':'consent','epoch':1,'physical':True})
    async def settle(self):
        for _ in range(200):
            if self.bridge.startup_done():return
            await asyncio.sleep(.001)
        self.fail('fake builder did not settle')
    async def test_pending_startup_processes_heartbeats_beyond_existing_watchdog(self):
        self.assertIsNone(self.consent())
        for value in (.8,1.6,2.4):
            self.now[0]=value;self.session.control({'type':'heartbeat','epoch':1})
            self.assertIsNone(self.session.poll_control());self.assertFalse(self.session.armed)
        self.release.set();await self.settle()
        self.assertEqual(self.session.poll_control(),{'type':'armed','epoch':1})
        self.assertEqual(self.owner.calls.count(('arm',1)),1)
    async def test_watchdog_expires_unchanged_and_late_success_never_arms(self):
        self.consent();self.now[0]=2.001
        with self.assertRaises(TimeoutError):self.session.poll_control()
        self.release.set();await self.settle();self.bridge.close()
        self.assertNotIn(('arm',1),self.owner.calls);self.assertIn(('close',),self.owner.calls)
    async def test_close_during_pending_startup_is_bounded_and_retains_owner(self):
        self.consent()
        with self.assertRaises(RuntimeError):self.session.close()
        self.assertEqual(self.session.credential,'');self.assertFalse(self.session.armed)
        self.release.set();await self.settle();self.bridge.close()
        self.assertNotIn(('arm',1),self.owner.calls);self.assertEqual(self.owner.calls,[('close',)])
    async def test_revocation_before_ready_prevents_commit(self):
        self.consent();self.release.set();await self.settle();self.registry.revoke(self.device)
        with self.assertRaises(PermissionError):self.session.poll_control()
        self.bridge.close();self.assertNotIn(('arm',1),self.owner.calls)
    async def test_epoch_change_before_ready_prevents_commit(self):
        self.consent();self.release.set();await self.settle();self.session.epoch=2
        with self.assertRaises(PermissionError):self.session.poll_control()
        self.bridge.close();self.assertNotIn(('arm',1),self.owner.calls)
    async def test_duplicate_consent_does_not_restart_or_renew_gesture(self):
        self.consent()
        for value in (1.,2.,3.,4.,4.9):
            self.now[0]=value;self.session.control({'type':'heartbeat','epoch':1});self.assertIsNone(self.consent())
        self.now[0]=5.001
        with self.assertRaises(TimeoutError):self.session.poll_control()
        self.release.set();await self.settle();self.bridge.close()
        self.assertEqual(len(self.builds),1);self.assertNotIn(('arm',1),self.owner.calls)
    async def test_driver_continues_control_dispatch_while_builder_waits(self):
        socket=Socket(['{"type":"consent","epoch":1,"physical":true}']);retained=[]
        task=asyncio.create_task(run_connection(socket,self.session,duration=None,retain_cleanup=lambda *r:retained.append(r)))
        try:
            for value in (.8,1.6,2.4):
                self.now[0]=value;socket.queue.put_nowait('{"type":"heartbeat","epoch":1}')
                await asyncio.sleep(.03)
                self.assertFalse(task.done(),'blocked or expired while authenticated heartbeats arrive')
            self.assertGreaterEqual(sum('heartbeat' in x for x in socket.sent),3)
        finally:
            task.cancel()
            with self.assertRaises(asyncio.CancelledError):await task
        self.assertTrue(retained,'pending native owner must remain tracked after cancellation')
        self.release.set();await self.settle();self.bridge.close()
        self.assertNotIn(('arm',1),self.owner.calls)
    async def test_cancelled_late_cleanup_failure_retains_exact_owner(self):
        self.consent();self.owner.fail_close=True
        with self.assertRaises(RuntimeError):self.session.close()
        self.release.set();await self.settle()
        with self.assertRaises(RuntimeError):self.bridge.close()
        self.assertNotIn(('arm',1),self.owner.calls)
        self.owner.fail_close=False;self.bridge.close()
        self.assertEqual(len(self.builds),1)
    async def test_turn_invalidation_prevents_ready_commit(self):
        self.consent();self.release.set();await self.settle()
        self.session.sessions.interrupt_turn(self.session.conversation.id,self.session.turn.id)
        with self.assertRaises(PermissionError):self.session.poll_control()
        self.bridge.close();self.assertNotIn(('arm',1),self.owner.calls)
    async def test_interrupt_during_pending_startup_cannot_resurrect(self):
        self.consent()
        with self.assertRaises(RuntimeError):self.session.start_control({'type':'interrupt','epoch':1})
        self.assertFalse(self.session.acknowledged)
        self.release.set();await self.settle();self.bridge.close()
        self.assertIsNone(self.session.poll_control());self.assertNotIn(('arm',1),self.owner.calls)
    async def test_duplicate_consent_completion_arms_once(self):
        self.consent();self.consent();self.release.set();await self.settle()
        self.session.poll_control()
        self.assertEqual(self.consent(),{'type':'armed','epoch':1})
        self.assertEqual(len(self.builds),1);self.assertEqual(self.owner.calls.count(('arm',1)),1)
    async def test_queued_interrupt_wins_over_completed_startup_in_driver(self):
        self.consent();self.release.set();await self.settle()
        socket=Socket(['{"type":"interrupt","epoch":1}'])
        task=asyncio.create_task(run_connection(socket,self.session,duration=None))
        await asyncio.sleep(.03);task.cancel()
        with self.assertRaises(asyncio.CancelledError):await task
        self.assertNotIn(('arm',1),self.owner.calls)

    async def test_driver_commits_ready_startup_after_heartbeats(self):
        self.owner.poll_voice=lambda epoch,frames:(False,[])
        def playing(epoch,value):self.owner.adapter.playing=value;return True
        self.owner.adapter.set_playing=playing
        socket=Socket(['{"type":"consent","epoch":1,"physical":true}'])
        task=asyncio.create_task(run_connection(socket,self.session,duration=None))
        try:
            for value in (.8,1.6,2.4):
                self.now[0]=value;socket.queue.put_nowait('{"type":"heartbeat","epoch":1}');await asyncio.sleep(.025)
            self.release.set()
            for _ in range(100):
                if any(json.loads(x).get('type')=='armed' for x in socket.sent):break
                await asyncio.sleep(.002)
            self.assertTrue(self.session.armed)
            self.assertEqual(sum(json.loads(x).get('type')=='armed' for x in socket.sent),1)
            self.assertFalse(task.done())
        finally:
            task.cancel()
            with self.assertRaises(asyncio.CancelledError):await task
        self.assertNotIn(('arm',2),self.owner.calls)

    async def test_invalid_consent_never_schedules_builder(self):
        for value in ({'type':'consent','epoch':1,'physical':False},{'type':'consent','epoch':2,'physical':True}):
            with self.assertRaises((PermissionError,ValueError)):self.session.start_control(value)
        self.assertEqual(self.builds,[])

class RetainedCleanupTests(unittest.IsolatedAsyncioTestCase):
    def fixture(self):
        registry=DeviceRegistry();device,token=registry.redeem_pairing_code(registry.create_pairing_code(),device_id='synthetic-cleanup',label='Synthetic',capabilities=('microphone','speaker'))
        opened=[]
        def provider(_):opened.append(True);return QualifiedProvider(Bridge(),lambda _:None,lambda:None,'synthetic',True,'synthetic_fixture')
        runtime=RemoteGateway(registry,ConversationSessions(),provider,fixture=True)
        async def close(**kw):pass
        socket=SimpleNamespace(request=SimpleNamespace(path='/companion/audio',headers=Headers([('Authorization','Bearer '+token),('X-Companion-Device',device.id)])),close=close)
        return runtime,device.id,socket,opened
    async def test_next_admission_retries_exact_cleanup_before_new_provider(self):
        runtime,device,socket,opened=self.fixture();closed=[]
        owner=SimpleNamespace(close=lambda:closed.append(True))
        runtime._active.add(device);runtime._retained[device]=[('provider',owner)]
        await runtime.handle(socket)
        self.assertEqual(closed,[True],'retained cleanup was never retried on next admission')
        self.assertEqual(opened,[True])
    async def test_cleanup_failure_keeps_slot_and_does_not_open_provider(self):
        runtime,device,socket,opened=self.fixture()
        def fail():raise RuntimeError('synthetic cleanup failure')
        owner=SimpleNamespace(close=fail);runtime._active.add(device);runtime._retained[device]=[('provider',owner)]
        await runtime.handle(socket)
        self.assertEqual(opened,[]);self.assertEqual(runtime._retained[device],[('provider',owner)]);self.assertIn(device,runtime._active)
    async def test_failed_rejected_socket_is_owned_without_releasing_live_slot(self):
        runtime,device,socket,opened=self.fixture();release=asyncio.Event()
        async def close(**kw):await release.wait()
        socket.close=close;runtime._active.add(device)
        await runtime.handle(socket)
        self.assertEqual(opened,[]);self.assertIn(device,runtime._active)
        self.assertEqual(len(runtime._unclaimed_closers),1)
        release.set();await runtime.close()
        self.assertFalse(runtime._unclaimed_closers);self.assertIn(device,runtime._active)
    async def test_cancelled_socket_wait_keeps_exact_close_task(self):
        from gateway_connection import OwnedSocketClose
        release=asyncio.Event();calls=[]
        async def close(**kw):calls.append(True);await release.wait()
        owner=OwnedSocketClose(SimpleNamespace(close=close));waiter=asyncio.create_task(owner.close())
        await asyncio.sleep(.001);waiter.cancel()
        with self.assertRaises(asyncio.CancelledError):await waiter
        self.assertEqual(calls,[True]);self.assertFalse(owner.task.cancelled())
        release.set();await owner.close();self.assertTrue(owner.confirmed)

    async def test_cancellation_during_admission_cleanup_owns_both_sockets(self):
        from gateway_connection import OwnedSocketClose
        runtime,device,incoming,opened=self.fixture()
        old_release=asyncio.Event();new_release=asyncio.Event()
        async def old_close(**kw):await old_release.wait()
        async def new_close(**kw):await new_release.wait()
        old_owner=OwnedSocketClose(SimpleNamespace(close=old_close));incoming.close=new_close
        runtime._active.add(device);runtime._retained[device]=[('socket',old_owner)]
        task=asyncio.create_task(runtime.handle(incoming))
        await asyncio.sleep(.001);task.cancel()
        with self.assertRaises(asyncio.CancelledError):await asyncio.wait_for(task,1)
        self.assertEqual(opened,[]);self.assertIn(device,runtime._active)
        self.assertEqual(runtime._retained[device],[('socket',old_owner)])
        self.assertFalse(old_owner.task.cancelled());self.assertEqual(len(runtime._unclaimed_closers),1)
        old_release.set();new_release.set();await runtime.close()
        self.assertNotIn(device,runtime._active);self.assertFalse(runtime._unclaimed_closers)

    async def test_parallel_admission_cannot_duplicate_retained_cleanup(self):
        from gateway_connection import OwnedSocketClose
        runtime,device,socket,opened=self.fixture();release=asyncio.Event();calls=[]
        async def close(**kw):calls.append(True);await release.wait()
        owner=OwnedSocketClose(SimpleNamespace(close=close))
        runtime._active.add(device);runtime._retained[device]=[('socket',owner)]
        first=asyncio.create_task(runtime.handle(socket));await asyncio.sleep(.001)
        await runtime.handle(socket)
        self.assertEqual(opened,[]);self.assertEqual(calls,[True]);self.assertIn(device,runtime._active)
        release.set();await first
        self.assertEqual(opened,[True]);self.assertEqual(calls,[True])

    async def test_socket_close_timeout_keeps_one_inflight_task_on_retry(self):
        import gateway_connection
        owner_type=getattr(gateway_connection,'OwnedSocketClose',None)
        self.assertIsNotNone(owner_type,'socket cleanup loses the timed-out close operation')
        release=asyncio.Event();calls=[]
        async def close(**kw):calls.append(True);await release.wait()
        owner=owner_type(SimpleNamespace(close=close))
        with self.assertRaises(TimeoutError):await owner.close(timeout=.005)
        with self.assertRaises(TimeoutError):await owner.close(timeout=.005)
        self.assertEqual(calls,[True]);release.set();await owner.close(timeout=.05)
        self.assertTrue(owner.confirmed)


class AdapterCancellationTests(unittest.TestCase):
    def test_cancel_before_open_never_opens(self):
        from native_adapter import NativeHostAdapter
        import ctypes
        adapter=NativeHostAdapter.__new__(NativeHostAdapter);adapter.handle=ctypes.c_void_p()
        with self.assertRaisesRegex(RuntimeError,'cancelled'):adapter.open_for_approved_test(cancelled=lambda:True)
        self.assertIsNone(adapter.handle.value)
    def check_native_cancel(self,where):
        from native_adapter import NativeHostAdapter
        import ctypes
        cancelled=threading.Event();calls=[]
        def opened(*args):
            calls.append('open');ctypes.cast(args[-1],ctypes.POINTER(ctypes.c_void_p))[0]=ctypes.c_void_p(1)
            if where=='open':cancelled.set()
            return 0
        def started(*args):
            calls.append('start')
            if where=='start':cancelled.set()
            return 0
        def closed(handle,retained):
            calls.append('close');ctypes.cast(handle,ctypes.POINTER(ctypes.c_void_p))[0]=ctypes.c_void_p()
            return 0
        adapter=NativeHostAdapter.__new__(NativeHostAdapter);adapter.handle=ctypes.c_void_p();adapter.lib=SimpleNamespace(sc_open=opened,sc_start=started,sc_close_checked=closed)
        with self.assertRaisesRegex(RuntimeError,'cancelled'):adapter.open_for_approved_test(cancelled=cancelled.is_set)
        self.assertEqual(calls,['open','close'] if where=='open' else ['open','start','close'])
        self.assertIsNone(adapter.handle.value)
    def test_cancel_between_open_and_start_closes_without_start(self):self.check_native_cancel('open')
    def test_cancel_after_start_closes_without_arm(self):self.check_native_cancel('start')

if __name__=='__main__':unittest.main()
