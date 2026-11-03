import unittest,asyncio
from types import SimpleNamespace
from contextlib import asynccontextmanager
from unittest.mock import patch
from native_conversation import NativeVoiceBridge
from remote_gateway import RemoteGateway,QualifiedProvider
from gateway_connection import TransportComplete
from numeric_pairing_worker import NumericWorker
from home_lab.devices import DeviceRegistry
from home_lab.sessions import ConversationSessions
from websockets.datastructures import Headers

class Resource:
    def reset(self):pass
    def __init__(self,fail=False):self.calls=0;self.fail=fail;self.muted=True
    def close(self):
        self.calls+=1
        if self.fail:raise RuntimeError('synthetic close failure')

class BridgeRetention(unittest.TestCase):
    def test_failed_component_is_retained_until_checked_retry(self):
        adapter=Resource(True);others=[Resource() for _ in range(4)]
        bridge=NativeVoiceBridge(adapter,*others)
        with self.assertRaises(RuntimeError):bridge.close()
        self.assertFalse(bridge.closed)
        with self.assertRaises(PermissionError):bridge.begin(1)
        self.assertEqual([r.calls for r in others],[1]*4)
        adapter.fail=False;bridge.close()
        self.assertTrue(bridge.closed);self.assertEqual(adapter.calls,2)
        self.assertEqual([r.calls for r in others],[1]*4)
        bridge.close();self.assertEqual(adapter.calls,2)

class GatewayRetention(unittest.IsolatedAsyncioTestCase):
    async def test_device_stays_busy_and_provider_retained_after_close_fault(self):
        registry=DeviceRegistry();device,token=registry.redeem_pairing_code(registry.create_pairing_code(),device_id='offline-retention',label='Fixture',capabilities=('microphone','speaker'))
        resource=Resource(True);factories=[]
        def factory(_):
            factories.append(True);return QualifiedProvider(SimpleNamespace(adapter=SimpleNamespace(playing=False)),lambda _:None,resource.close,'offline-original-call',True,'existing_mira')
        class Session:
            armed=False
            def __init__(self,*args,**kwargs):pass
            def statistics(self):return {}
            def close(self):pass
        class Socket:
            request=SimpleNamespace(path='/companion/audio',headers=Headers([('Authorization','Bearer '+token),('X-Companion-Device',device.id)]))
            async def close(self,**kw):pass
        async def complete(*args,**kwargs):raise TransportComplete()
        gateway=RemoteGateway(registry,ConversationSessions(),factory,session_factory=Session)
        with patch('remote_gateway.run_connection',complete):await gateway.handle(Socket())
        self.assertIn(device.id,gateway._active)
        await gateway.handle(Socket());self.assertEqual(len(factories),1)
        self.assertEqual(resource.calls,2) # New admission retries the retained owner before rejecting.
        with self.assertRaises(RuntimeError):await gateway.close()
        self.assertIn(device.id,gateway._active)
        resource.fail=False;await gateway.close()
        self.assertNotIn(device.id,gateway._active);self.assertEqual(resource.calls,4)

class WorkerRetention(unittest.IsolatedAsyncioTestCase):
    async def test_worker_keeps_gateway_when_socket_teardown_fails(self):
        class Port:
            is_open=False
            def open(self):self.is_open=True
            def close(self):self.is_open=False
            def write(self,wire):return len(wire)
        class Socket:
            async def close(self):raise RuntimeError('synthetic close failure')
        class Runtime:
            transport=object()
            def issue(self,ttl):return 'synthetic-fixture-0000000000000000'
            def revoke(self):pass
            @asynccontextmanager
            async def listener(self,gateway):
                gateway.sockets.add(Socket());gateway.pairing.cancel();yield
        worker=NumericWorker(approved=True,paused=lambda:False,serial_factory=Port,runtime_loader=Runtime,emit=lambda _:None)
        async def drain():pass
        async def receipt():return SimpleNamespace(status=1,boot_nonce=91,reset_reason=1)
        worker._drain=drain;worker._receipt=receipt
        await worker._prepare()
        self.assertTrue(worker.cleanup_blocked)
        self.assertIsNotNone(worker.gateway)
        with self.assertRaises(Exception):await worker.close()
        self.assertIsNotNone(worker.gateway)

class PartialProviderRetention(unittest.IsolatedAsyncioTestCase):
    async def test_listen_setup_failure_keeps_partial_native_owner_in_gateway(self):
        import native_conversation as native
        import tempfile
        from pathlib import Path
        import hashlib
        adapter=Resource(True)
        def failed_open():raise RuntimeError('synthetic partial open')
        adapter.open_for_approved_test=failed_open
        registry=DeviceRegistry();device,token=registry.redeem_pairing_code(registry.create_pairing_code(),device_id='partial-open',label='Fixture',capabilities=('microphone','speaker'))
        class Socket:
            request=SimpleNamespace(path='/companion/audio',headers=Headers([('Authorization','Bearer '+token),('X-Companion-Device',device.id)]))
            def __init__(self):
                self.messages=iter(['{"type":"begin","request_id":"synthetic-listen"}',
                                    '{"type":"ack","epoch":1}',
                                    '{"type":"consent","epoch":1,"physical":true}'])
            async def recv(self):return next(self.messages)
            async def send(self,wire):pass
            async def close(self,**kw):pass
        with tempfile.TemporaryDirectory() as folder:
            lib=Path(folder)/'fixture';lib.write_bytes(b'fixture-not-library')
            route=native.NativeRoute('offline-original-call')
            with patch.object(native,'LIBRARY',lib),patch.object(native,'LIBRARY_HASH',hashlib.sha256(lib.read_bytes()).hexdigest()),patch('native_adapter.NativeHostAdapter',lambda _:adapter),patch('codec.OpusCodec',lambda _:Resource()),patch('codec.Resampler',lambda *args:Resource()):
                gateway=RemoteGateway(registry,ConversationSessions(),lambda _:native.make_provider(route))
                await gateway.handle(Socket())
        self.assertIn(device.id,gateway._active)
        attempts=adapter.calls
        self.assertGreaterEqual(attempts,1)
        adapter.fail=False;await gateway.close()
        self.assertEqual(adapter.calls,attempts+1);self.assertNotIn(device.id,gateway._active)

class NativeMuteCleanup(unittest.TestCase):
    def test_successful_disposal_after_mute_fault_does_not_retry_dead_handle(self):
        adapter=Resource();adapter.muted=False
        def failed_interrupt(epoch):raise RuntimeError('synthetic mute failure')
        adapter.interrupt=failed_interrupt
        adapter.begin=lambda _:None
        bridge=NativeVoiceBridge(adapter,*[Resource() for _ in range(4)])
        bridge.begin(1)
        with self.assertRaises(RuntimeError):bridge.close()
        self.assertTrue(bridge.closed)
        bridge.close();self.assertEqual(adapter.calls,1)

class ExactCleanupResources(unittest.IsolatedAsyncioTestCase):
    async def test_socket_fault_does_not_retry_already_disposed_session(self):
        registry=DeviceRegistry();device,token=registry.redeem_pairing_code(registry.create_pairing_code(),device_id='socket-only',label='Fixture',capabilities=('microphone','speaker'))
        bridge=SimpleNamespace(closed=False);session_closes=[];provider_closes=[]
        def dispose():provider_closes.append(True);bridge.closed=True
        class Session:
            armed=False;acknowledged=False
            def __init__(self,*args,**kwargs):self.clock=lambda:0
            def _authorized(self):raise TransportComplete()
            def statistics(self):return {}
            def close(self):
                if bridge.closed:raise RuntimeError('native already disposed')
                session_closes.append(True)
        class Socket:
            calls=0
            request=SimpleNamespace(path='/companion/audio',headers=Headers([('Authorization','Bearer '+token),('X-Companion-Device',device.id)]))
            async def recv(self):await asyncio.Future()
            async def close(self,**kw):
                self.calls+=1
                if self.calls==1:raise RuntimeError('transient socket failure')
        socket=Socket();gateway=RemoteGateway(registry,ConversationSessions(),lambda _:QualifiedProvider(bridge,lambda _:None,dispose,'fixture',True,'existing_mira'),session_factory=Session)
        await gateway.handle(socket)
        self.assertIn(device.id,gateway._active)
        await gateway.close()
        self.assertNotIn(device.id,gateway._active);self.assertEqual(session_closes,[True]);self.assertEqual(provider_closes,[True]);self.assertEqual(socket.calls,2)

class SessionAuthorityOnClose(unittest.TestCase):
    def test_mute_failure_still_invalidates_session_credential_and_arm(self):
        from test_streaming import Tests
        fixture=Tests();fixture.setUp();fixture.armed()
        def fail(_):raise RuntimeError('mute failure')
        fixture.bridge.interrupt=fail
        with self.assertRaises(RuntimeError):fixture.session.close()
        self.assertFalse(fixture.session.armed);self.assertFalse(fixture.session.acknowledged);self.assertEqual(fixture.session.credential,'')
        fixture.bridge.closed=True;fixture.session.close()

class UncertainGrantRevoke(unittest.TestCase):
    def test_partial_issuer_fault_retries_actual_revoke(self):
        import numeric_pairing as n
        from test_numeric_pairing import REQ,DN,HN
        attempts=[]
        def issuer(_):raise RuntimeError('partial issue failure')
        def revoke():
            attempts.append(True)
            if len(attempts)==1:raise RuntimeError('revoke failure')
        p=n.NumericPairing(issuer=issuer,revoke=revoke,nonce_factory=lambda:HN,code_factory=lambda:'123456')
        p.prepare(REQ,91);p.hello(dict(type='pair.hello',version=1,request_id=REQ,device=n.DEVICE,device_nonce=DN,boot_nonce=91,capabilities=['microphone','speaker']))
        p.mac_confirm()
        with self.assertRaises(n.PairingError):p.robot_confirm(dict(type='pair.confirm',version=1,request_id=REQ,device_nonce=DN,host_nonce=HN,physical=True))
        self.assertTrue(p.revocation_failed);p.cancel()
        self.assertEqual(attempts,[True,True]);self.assertFalse(p.revocation_failed)
