"""Offline diagnostic coverage: temporary files, synthetic grants, no real IO/media."""
import asyncio
import json
import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from websockets.datastructures import Headers
from home_lab.devices import DeviceRegistry
from home_lab.sessions import ConversationSessions
from remote_gateway import RemoteGateway,QualifiedProvider
from test_streaming import Bridge

SECRET='SYNTHETIC-SECRET-MUST-NOT-BE-RECORDED'

class RecordingTests(unittest.IsolatedAsyncioTestCase):
    def recording_type(self):
        try:
            from connection_recording import ConnectionRecording
        except ImportError:
            self.fail('bounded public connection recorder is missing')
        return ConnectionRecording

    def gateway(self, sink, *, session_factory=None, provider=None):
        registry=DeviceRegistry()
        device,token=registry.redeem_pairing_code(registry.create_pairing_code(),device_id='synthetic-recorder',label='Synthetic',capabilities=('microphone','speaker'))
        kw={} if session_factory is None else {'session_factory':session_factory}
        runtime=RemoteGateway(registry,ConversationSessions(),provider or (lambda _:QualifiedProvider(Bridge(),lambda _:self.fail('audio must remain unarmed'),lambda:None,'synthetic',True,'synthetic_fixture')),fixture=True,**kw)
        runtime.diagnostic_sink=sink
        async def close(**kw):pass
        async def recv():return '{invalid '+SECRET
        async def send(_):pass
        socket=SimpleNamespace(request=SimpleNamespace(path='/companion/audio',headers=Headers([('Authorization','Bearer '+token),('X-Companion-Device',device.id)])),close=close,recv=recv,send=send)
        return runtime,socket

    async def test_http_rejection_recorded_without_headers_and_without_provider(self):
        events=[];calls=[]
        runtime,socket=self.gateway(lambda *v:events.append(v),provider=lambda _:calls.append(True))
        socket.request.headers=Headers([('Authorization',SECRET)])
        response=runtime.process_request(SimpleNamespace(respond=lambda code,body:code),socket.request)
        self.assertEqual(response,403)
        self.assertEqual(calls,[])
        self.assertTrue(events,'HTTP rejection currently leaves no retained diagnostic event')
        self.assertEqual(events[0][:2],('http_admission','admission_denied'))
        self.assertNotIn(SECRET,repr(events))

    async def test_provider_failure_identifies_provider_stage(self):
        events=[]
        def fail(_):raise ValueError(SECRET)
        runtime,socket=self.gateway(lambda *v:events.append(v),provider=fail)
        await runtime.handle(socket)
        self.assertTrue(events,'provider exception currently only updates private last_result')
        self.assertEqual(events[-1][:2],('session_closed','transport_fault'))
        self.assertEqual(events[-1][2]['fault']['phase'],'provider_create')
        self.assertNotIn(SECRET,repr(events))

    async def test_session_creation_failure_identifies_stage(self):
        events=[]
        def fail(*args,**kw):raise ValueError(SECRET)
        runtime,socket=self.gateway(lambda *v:events.append(v),session_factory=fail)
        await runtime.handle(socket)
        self.assertTrue(events,'session creation currently leaves no retained event')
        self.assertEqual(events[-1][2]['fault']['phase'],'session_create')
        self.assertNotIn(SECRET,repr(events))

    async def test_invalid_control_records_phase_without_arming_or_echo(self):
        events=[];runtime,socket=self.gateway(lambda *v:events.append(v))
        await asyncio.wait_for(runtime.handle(socket),1)
        self.assertTrue(events,'transport close currently leaves no retained event')
        self.assertIn(('websocket','session_started'),[v[:2] for v in events])
        self.assertEqual(events[-1][2]['fault']['phase'],'control')
        self.assertFalse(events[-1][2]['consent_received'])
        self.assertNotIn(SECRET,repr(events))

    async def test_failed_sink_cannot_change_admission_or_cleanup(self):
        def broken(*_):raise OSError(SECRET)
        runtime,socket=self.gateway(broken)
        self.assertIsNone(runtime.process_request(SimpleNamespace(respond=lambda code,body:code),socket.request))
        await asyncio.wait_for(runtime.handle(socket),1)
        self.assertFalse(runtime._active)

    def test_bounded_private_atomic_file_redacts_unknowns(self):
        recorder_type=self.recording_type()
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'connection.json';record=recorder_type(path)
            self.assertFalse(path.exists(),'construction must be inert')
            for _ in range(25):record('session_closed','transport_fault',{'fault':{'type':SECRET,'phase':SECRET,'message':SECRET,'stack':[]},'credential':SECRET})
            value=json.loads(path.read_text())
            self.assertEqual(len(value['events']),16)
            self.assertEqual(value['events'][-1]['sequence'],25)
            self.assertNotIn(SECRET,path.read_text())
            self.assertEqual(os.stat(path).st_mode & 0o777,0o600)
            self.assertEqual(list(Path(directory).iterdir()),[path])
            self.assertGreater(value['events'][-1]['observed_unix'],0)

    def test_symlink_destination_never_touches_target(self):
        recorder_type=self.recording_type()
        with tempfile.TemporaryDirectory() as directory:
            target=Path(directory)/'target';target.write_text(SECRET)
            link=Path(directory)/'connection.json';link.symlink_to(target)
            with self.assertRaises(OSError):recorder_type(link)('listener','ready')
            self.assertEqual(target.read_text(),SECRET)

    async def test_worker_records_readiness_without_changing_protocol(self):
        from contextlib import asynccontextmanager
        from remembered_pairing_worker import RememberedWorker
        events=[];protocol=[]
        worker=RememberedWorker(lambda:None,emit=protocol.append)
        worker.diagnostic_sink=lambda *v:events.append(v)
        @asynccontextmanager
        async def listener(_):yield
        worker.runtime=SimpleNamespace(listener=listener)
        async def close():pass
        worker.gateway=SimpleNamespace(close=close,activity_state='physical_consent')
        task=asyncio.create_task(worker.listen(None))
        try:
            await asyncio.sleep(.01)
            self.assertIn(('listener','ready'),[v[:2] for v in events])
            self.assertEqual(protocol[0]['state'],'paired')
            self.assertNotIn('diagnostics',protocol[0])
        finally:
            task.cancel();await asyncio.gather(task,return_exceptions=True)

if __name__=='__main__':unittest.main()
