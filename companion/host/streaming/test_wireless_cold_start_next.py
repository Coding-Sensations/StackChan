import unittest,asyncio,json
from contextlib import asynccontextmanager
from types import SimpleNamespace
import numeric_pairing as n
from numeric_pairing_worker import NumericWorker
from test_numeric_pairing import Clock,REQ,DN,HN

def hello(**extra):
    return dict(type='pair.hello',version=1,request_id=REQ,device=n.DEVICE,device_nonce=DN,boot_nonce=91,capabilities=['microphone','speaker'],**extra)

class ColdStart(unittest.TestCase):
    def core(self):
        self.clock=Clock();self.issued=[]
        self.p=n.NumericPairing(issuer=lambda ttl:self.issued.append(ttl) or 'synthetic-only-00000000000000000000',revoke=lambda:None,clock=self.clock,nonce_factory=lambda:HN,code_factory=lambda:'123456',audio_requested=True)
        self.p.prepare_wireless()
    def test_adopts_robot_context_only_after_valid_hello_and_both_confirmations(self):
        self.core();challenge=self.p.hello(hello())
        self.assertEqual(challenge['request_id'],REQ);self.assertEqual(self.p.boot_nonce,91)
        self.assertFalse(self.p.audio_allowed);self.assertEqual(self.issued,[])
        self.p.mac_confirm();self.assertEqual(self.issued,[])
        confirm=dict(type='pair.confirm',version=1,request_id=REQ,device_nonce=DN,host_nonce=HN,physical=True)
        self.p.robot_confirm(confirm);self.assertEqual(len(self.issued),1)
        self.assertFalse(self.p.audio_allowed);self.p.take_grant()
        self.p.staged(dict(type='pair.staged',version=1,request_id=REQ,device_nonce=DN,host_nonce=HN,staged=True))
        self.assertTrue(self.p.audio_allowed)
    def test_bad_hello_cannot_select_candidate_and_stale_confirmation_cannot_issue(self):
        for key,value in [('device','foreign'),('boot_nonce',True),('boot_nonce',0),('request_id','0'*32),('device_nonce','0'*32),('capabilities',['microphone'])]:
            with self.subTest(key=key):
                self.core();bad=hello();bad[key]=value
                with self.assertRaises(n.PairingError):self.p.hello(bad)
                self.assertIsNone(self.p.request);self.assertIsNone(self.p.boot_nonce)
        self.core();self.p.hello(hello());self.p.mac_confirm()
        with self.assertRaises(n.PairingError):self.p.robot_confirm(dict(type='pair.confirm',version=1,request_id=REQ,device_nonce=DN,host_nonce='c'*32,physical=True))
        self.assertEqual(self.issued,[])
    def test_deadline_and_duplicate_candidate_and_usb_frame_refused(self):
        self.core()
        with self.assertRaises(n.PairingError):self.p.numeric_frame()
        self.p.hello(hello())
        with self.assertRaises(n.PairingError):self.p.hello(hello())
        self.clock.now=120
        with self.assertRaises(n.PairingError):self.p.tick()
        self.assertFalse(self.p.audio_allowed);self.assertEqual(self.issued,[])

class WorkerColdStart(unittest.IsolatedAsyncioTestCase):
    async def test_full_pairing_without_constructing_a_serial_port(self):
        serial=[];statuses=[];issued=[];revoked=[];ready=[]
        def forbidden_serial():serial.append(True);raise AssertionError('Wi-Fi opened serial')
        class Socket:
            request=SimpleNamespace(path='/companion/pair')
            def __init__(self,core):self.core=core;self.queue=asyncio.Queue();self.queue.put_nowait(json.dumps(hello()))
            async def recv(self):return await self.queue.get()
            async def close(self):pass
            async def send(self,wire):
                value=json.loads(wire);context={k:value[k] for k in ('request_id','device_nonce','host_nonce')}
                if value['type']=='pair.challenge':
                    self.core.mac_confirm();self.queue.put_nowait(json.dumps(dict(type='pair.confirm',version=1,physical=True,**context)))
                elif value['type']=='pair.grant':self.queue.put_nowait(json.dumps(dict(type='pair.staged',version=1,staged=True,**context)))
                elif value['type']=='pair.ready':ready.append(value);self.core.cancel('closed','closed')
        class Runtime:
            transport=object()
            def issue(self,ttl):issued.append(ttl);return 'synthetic-only-00000000000000000000'
            def revoke(self):revoked.append(True)
            @asynccontextmanager
            async def listener(self,gateway):
                task=asyncio.create_task(gateway.handle(Socket(gateway.pairing)))
                try:yield
                finally:await task
        worker=NumericWorker(approved=True,paused=lambda:False,serial_factory=forbidden_serial,runtime_loader=Runtime,emit=statuses.append,transport_mode='wifi',audio_enabled=True)
        async with asyncio.timeout(1):await worker._prepare()
        self.assertEqual(serial,[]);self.assertEqual(len(issued),1);self.assertEqual(revoked,[True])
        self.assertEqual(len(ready),1);self.assertIsNone(worker.port);self.assertIsNone(worker.gateway)
        self.assertFalse(worker.cleanup_blocked)
    async def test_pause_blocks_wireless_runtime_and_no_serial(self):
        calls=[]
        worker=NumericWorker(approved=True,paused=lambda:True,serial_factory=lambda:calls.append('serial'),runtime_loader=lambda:calls.append('runtime'),emit=lambda _:None,transport_mode='wifi')
        await worker._prepare();self.assertEqual(calls,[]);self.assertEqual(worker.state,'failed')

class WirelessDeadlineAndScope(unittest.TestCase):
    def test_wireless_preparation_cannot_reset_elapsed_physical_window(self):
        clock=Clock();clock.now=120
        core=n.NumericPairing(issuer=lambda _:None,revoke=lambda:None,clock=clock)
        with self.assertRaises(n.PairingError):core.prepare_wireless(started_at=0)
        self.assertEqual(core.state,'idle')
    def test_wireless_authorization_is_not_usb_authorization(self):
        import numeric_pairing_worker as w
        from unittest.mock import patch
        import tempfile,time
        from pathlib import Path
        for requested,approved in [('wifi','usb'),('usb','wifi')]:
            with self.subTest(requested=requested),tempfile.TemporaryDirectory() as folder:
                root=Path(folder);(root/'notes').mkdir();(root/'.tools').mkdir()
                (root/'notes/NUMERIC-PAIRING-RUNTIME-AUTHORIZATION.json').write_text(json.dumps(dict(approved=True,audio_allowed=False,valid_until_unix=time.time()+60,transport=approved)))
                with patch.object(w,'ROOT',root):
                    with self.assertRaises(n.PairingError):w._consume_approval(requested_transport=requested)
                self.assertEqual(list((root/'.tools').iterdir()),[])
