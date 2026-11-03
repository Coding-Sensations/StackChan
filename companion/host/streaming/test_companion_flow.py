"""Offline Companion regression fixtures; only synthetic ports and state."""
import asyncio
import struct
import unittest
from contextlib import asynccontextmanager
from types import SimpleNamespace
from unittest.mock import patch
import numeric_pairing_worker as worker_module
from remote_gateway import RemoteGateway
from home_lab.devices import DeviceRegistry
from home_lab.sessions import ConversationSessions
from test_numeric_pairing import Clock
from test_numeric_pairing_worker import Port

class CompanionFlowTests(unittest.IsolatedAsyncioTestCase):
    async def test_cleanup_is_visible_before_usb_closes(self):
        events=[];port=Port();port.reads=[];clock=Clock();closing_states=[]
        async def sleep(n):clock.now+=n;await asyncio.sleep(0)
        def close():
            closing_states.append(events[-1]['state'])
            port.is_open=False;port.closed=True
        port.close=close
        worker=worker_module.NumericWorker(approved=True,paused=lambda:False,
            serial_factory=lambda:port,runtime_loader=lambda:None,emit=events.append,
            clock=clock,sleep=sleep)
        await worker.command('prepare');await worker.wait()
        self.assertTrue(port.closed)
        self.assertEqual(closing_states,['restoring'])
        self.assertEqual(events[-1]['state'],'failed')
        self.assertEqual(events[-1]['message'],'ready_missing')

    async def test_only_accepted_session_authority_projects_conversation(self):
        gateway=RemoteGateway(DeviceRegistry(),ConversationSessions(),fixture=True)
        self.assertEqual(getattr(gateway,'activity_state',None),'physical_consent')
        from streaming_session import StreamingSession
        from test_streaming import Bridge
        registry=DeviceRegistry();device,token=registry.redeem_pairing_code(registry.create_pairing_code(),device_id='synthetic-activity',label='Fixture',capabilities=('microphone','speaker'))
        session=StreamingSession(registry,ConversationSessions(),Bridge(),token,device.id,lambda _:None,fixture=True)
        gateway._activity_session=session
        session.begin('activity-fixture')
        with self.assertRaises(PermissionError):session.control({'type':'consent','epoch':1,'physical':True})
        self.assertEqual(gateway.activity_state,'physical_consent')
        session.control({'type':'ack','epoch':1})
        session.control({'type':'consent','epoch':1,'physical':True})
        self.assertEqual(gateway.activity_state,'conversation')
        session.interrupt()
        self.assertEqual(gateway.activity_state,'physical_consent')
        session.close()
        self.assertFalse(gateway.completion_started)

    async def test_worker_projects_activity_without_identity_or_audio_claims(self):
        events=[];clock=Clock();port=Port();sent=False;transport=SimpleNamespace(activity_state='physical_consent')
        ready=struct.pack('>4sBB16s',b'SCPS',1,1,struct.pack('>II8s',91,1,bytes(8)))
        def read(_):
            nonlocal sent
            if clock.now>=2 and not sent:sent=True;return ready
            return b''
        port.read=read
        async def sleep(n):clock.now+=n;await asyncio.sleep(0)
        class Runtime:
            def __init__(self):self.transport=transport
            def issue(self,_):return 'S'*40
            def revoke(self):pass
            @asynccontextmanager
            async def listener(self,_):yield
        worker=worker_module.NumericWorker(approved=True,audio_enabled=True,
            paused=lambda:False,serial_factory=lambda:port,runtime_loader=Runtime,
            emit=events.append,clock=clock,sleep=sleep,consume_approval=lambda:91)
        await worker.command('prepare')
        try:
            for _ in range(500):
                if worker.pairing is not None:break
                await asyncio.sleep(0)
            self.assertIsNotNone(worker.pairing)
            worker.pairing.state='paired';worker.pairing.audio_allowed=True
            for _ in range(10):await asyncio.sleep(0)
            self.assertTrue(any(e.get('activity')=='physical_consent' for e in events))
            transport.activity_state='conversation'
            for _ in range(10):await asyncio.sleep(0)
            self.assertTrue(any(e.get('activity')=='conversation' for e in events))
            self.assertTrue(all(set(e)<={'state','message','code','acknowledged','remaining','activity','audio_remaining','cleanup'} for e in events))
        finally:await worker.close()

    async def test_unknown_pairing_reason_is_never_emitted(self):
        events=[]
        def consume():raise worker_module.PairingError('synthetic private exception')
        worker=worker_module.NumericWorker(approved=True,paused=lambda:False,
            serial_factory=lambda:self.fail('must not open'),runtime_loader=lambda:None,
            emit=events.append,consume_approval=consume)
        await worker.command('prepare');await worker.wait()
        self.assertNotIn('synthetic private',str(events))
        self.assertEqual(events[-1]['message'],'setup_or_transport_failed')

class DurationBoundTests(unittest.TestCase):
    def test_native_live_mode_rejects_more_than_sixty_seconds(self):
        from native_conversation import NativeRoute
        with self.assertRaises(worker_module.PairingError):
            worker_module._validated_audio_mode(SimpleNamespace(audio_mode='native_conversation'),NativeRoute('fixture-original-dot',61))
