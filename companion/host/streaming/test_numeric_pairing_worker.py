import asyncio
import json
import unittest
from unittest.mock import patch
from types import SimpleNamespace
try:
    import numeric_pairing_worker as w
except ImportError:
    w=None
from test_numeric_pairing import Clock,REQ
from pairing_receipt import decode_receipt
import struct
class Port:
    def __init__(self,short=False):self.events=[];self.short=short;self.reads=[struct.pack('>4sBB16s',b'SCPS',1,1,struct.pack('>II8s',91,1,bytes(8)))];self.closed=False;self.is_open=False
    def open(self):self.events.append('open');self.is_open=True
    def read(self,n):return self.reads.pop(0) if self.reads else b''
    def write(self,x):self.events.append(('write',x));return len(x)-1 if self.short else len(x)
    def close(self):self.closed=True;self.is_open=False;self.events.append('close')
class WorkerTests(unittest.IsolatedAsyncioTestCase):
    def setUp(self):self.assertIsNotNone(w,'numeric worker missing');self.events=[];self.clock=Clock();self.port=Port();self.opened=0;self.loaded=0
    def worker(self,approved=True,paused=False):
        def factory():self.opened+=1;return self.port
        def loader():self.loaded+=1;raise RuntimeError('synthetic loader boundary')
        return w.NumericWorker(approved=approved,paused=lambda:paused,serial_factory=factory,runtime_loader=loader,emit=self.events.append,clock=self.clock,sleep=self.sleep)
    async def sleep(self,n):self.clock.now+=n;await asyncio.sleep(0)
    async def test_main_does_not_report_clean_exit_after_worker_cleanup_error(self):
        emitted=[]
        def fail_run(_args):raise RuntimeError('synthetic cleanup failure')
        with patch.object(w,'_run',new=fail_run):
            with patch.object(w,'_output',side_effect=emitted.append):
                result=w.main([])
        self.assertNotEqual(result,0)
        self.assertEqual(emitted,[{'state':'failed','message':'cleanup_unconfirmed','cleanup':['audio']}])
    async def test_import_and_constructor_inert(self):
        worker=self.worker();self.assertEqual(self.opened,0);self.assertEqual(self.loaded,0);self.assertIsNone(worker.port)
    async def test_activation_and_pause_denial_before_open(self):
        for approved,paused in ((False,False),(True,True)):
            worker=self.worker(approved,paused);await worker.command('prepare');await worker.wait()
            self.assertEqual(self.opened,0);self.assertEqual(self.loaded,0);self.assertEqual(worker.state,'failed')
    async def test_open_before_pair_no_write_during_stabilization(self):
        # No Ready until stabilization ends; recorded factory/readers are synthetic.
        self.port.reads=[];worker=self.worker();await worker.command('prepare')
        while self.clock.now<2:await asyncio.sleep(0)
        self.assertEqual(self.port.events,['open']);self.assertEqual(self.loaded,0)
        await worker.command('cancel');await worker.wait();self.assertTrue(self.port.closed);self.assertEqual(worker.state,'cancelled')
    async def test_invalid_command_never_opens(self):
        worker=self.worker()
        with self.assertRaises(Exception):await worker.command('Prepare')
        self.assertEqual(self.opened,0)
    async def test_eof_close_cancels_inflight(self):
        self.port.reads=[];worker=self.worker();await worker.command('prepare');await asyncio.sleep(0);await worker.close();self.assertTrue(self.port.closed);self.assertEqual(worker.state,'closed')
    async def test_ready_then_listener_then_one_numeric_write(self):
        from contextlib import asynccontextmanager
        events=[];port=self.port;clock=self.clock
        ready=struct.pack('>4sBB16s',b'SCPS',1,1,struct.pack('>II8s',91,1,bytes(8)))
        sent=False
        def read(cap):
            nonlocal sent
            if clock.now>=2 and not sent:sent=True;events.append('ready');return ready
            return b''
        port.read=read
        class Runtime:
            transport=object()
            def issue(self,ttl):raise AssertionError('no confirmation')
            def revoke(self):events.append('revoke')
            @asynccontextmanager
            async def listener(self,gateway):
                events.append('listener_open')
                try:yield
                finally:events.append('listener_closed')
        def load():events.append('load');return Runtime()
        worker=w.NumericWorker(approved=True,paused=lambda:False,serial_factory=lambda:port,runtime_loader=load,emit=self.events.append,clock=clock,sleep=self.sleep)
        await worker.command('prepare')
        for _ in range(500):
            if any(type(x) is tuple and x[0]=='write' for x in port.events):break
            await asyncio.sleep(0)
        writes=[x for x in port.events if type(x) is tuple]
        self.assertEqual(len(writes),1);self.assertEqual(writes[0][1][:5],b'SCNP\x01');self.assertEqual(len(writes[0][1]),25)
        self.assertEqual(events[:3],['ready','load','listener_open'])
        await worker.command('cancel');await worker.wait();self.assertTrue(port.closed);self.assertIn('listener_closed',events)
    async def test_retry_is_explicit_and_not_automatic(self):
        worker=self.worker(approved=False);await worker.command('prepare');await worker.wait()
        self.assertEqual(worker.state,'failed');self.assertEqual(self.opened,0)
        await worker.command('retry');await worker.wait();self.assertEqual(self.opened,0)
        self.assertEqual(sum(x['state']=='failed' for x in self.events),2)
    async def test_missing_ready_never_loads_or_writes(self):
        self.port.reads=[];worker=self.worker();await worker.command('prepare');await worker.wait()
        self.assertEqual(worker.state,'failed');self.assertEqual(self.loaded,0);self.assertEqual(self.port.events,['open','close'])
    async def test_terminal_cancel_withheld_until_listener_and_usb_cleanup(self):
        from contextlib import asynccontextmanager
        gate=asyncio.Event();port=self.port;clock=self.clock;sent=False
        ready=struct.pack('>4sBB16s',b'SCPS',1,1,struct.pack('>II8s',91,1,bytes(8)))
        def read(cap):
            nonlocal sent
            if clock.now>=2 and not sent:sent=True;return ready
            return b''
        port.read=read
        class Runtime:
            transport=object()
            def issue(self,ttl):return 'synthetic'
            def revoke(self):pass
            @asynccontextmanager
            async def listener(self,gateway):
                try:yield
                finally:await gate.wait()
        worker=w.NumericWorker(approved=True,paused=lambda:False,serial_factory=lambda:port,runtime_loader=Runtime,emit=self.events.append,clock=clock,sleep=self.sleep)
        await worker.command('prepare')
        while not any(type(x) is tuple for x in port.events):await asyncio.sleep(0)
        cancel=asyncio.create_task(worker.command('cancel'));await asyncio.sleep(0);await asyncio.sleep(0)
        self.assertFalse(port.closed);self.assertFalse(any(x['state']=='cancelled' for x in self.events))
        gate.set();await cancel
        self.assertTrue(port.closed);self.assertEqual(self.events[-1]['state'],'cancelled')
    async def test_close_failure_no_terminal_ack_or_retry(self):
        self.port.reads=[]
        def bad_close():raise OSError('synthetic secret')
        self.port.close=bad_close
        worker=self.worker();await worker.command('prepare');await asyncio.sleep(0)
        await worker.command('cancel');await worker.wait()
        self.assertTrue(worker.cleanup_blocked)
        self.assertFalse(any(x['state'] in ('cancelled','closed') for x in self.events))
        with self.assertRaises(Exception):await worker.command('retry')
    async def test_all_cleanup_faults_are_preserved_and_block_retry(self):
        from contextlib import asynccontextmanager
        port=self.port;clock=self.clock;sent=False
        ready=struct.pack('>4sBB16s',b'SCPS',1,1,struct.pack('>II8s',91,1,bytes(8)))
        def read(cap):
            nonlocal sent
            if clock.now>=2 and not sent:
                sent=True
                return ready
            return b''
        port.read=read
        cleanup_result={'outcome':'complete_with_cleanup_fault','native_conversation':True,
                        'conversation_complete':True,'consent_received':True,
                        'diagnostics':{'cleanup_fault_count':1,'cleanup_fault':{'type':'RuntimeError','phase':'session_cleanup','stack':[]}}}
        class Runtime:
            transport=SimpleNamespace(last_result=cleanup_result)
            def issue(self,ttl):return 'synthetic'
            def revoke(self):pass
            @asynccontextmanager
            async def listener(self,gateway):yield
        worker=w.NumericWorker(approved=True,audio_enabled=True,paused=lambda:False,
            serial_factory=lambda:port,runtime_loader=Runtime,emit=self.events.append,
            clock=clock,sleep=self.sleep,consume_approval=lambda:91)
        await worker.command('prepare')
        for _ in range(1000):
            if worker.gateway is not None and any(type(x) is tuple for x in port.events):break
            await asyncio.sleep(0)
        self.assertIsNotNone(worker.gateway)
        worker.pairing.cancel('failed','audio_cleanup_failed')
        worker.pairing.revocation_failed=True
        def failed_revoke():raise RuntimeError('synthetic grant revoke failure')
        worker.pairing.revoke=failed_revoke
        def failed_usb_close():raise OSError('synthetic usb close failure')
        port.close=failed_usb_close
        await worker.wait()
        self.assertTrue(worker.cleanup_blocked)
        self.assertFalse(port.closed)
        self.assertIs(worker.port,port)
        self.assertEqual(self.events[-1]['state'],'failed')
        self.assertEqual(self.events[-1]['message'],'cleanup_unconfirmed')
        self.assertEqual(self.events[-1]['cleanup'],['audio','grant','usb'])
        self.assertEqual(worker.first_terminal_result['reason'],'cleanup_unconfirmed')
        with self.assertRaises(Exception):await worker.command('retry')

    async def test_consumption_failure_denies_before_usb(self):
        calls=[]
        def denied():calls.append('consume');raise RuntimeError('already consumed synthetic')
        worker=self.worker();worker.consume_approval=denied
        await worker.command('prepare');await worker.wait()
        self.assertEqual(calls,['consume']);self.assertEqual(self.opened,0);self.assertEqual(self.loaded,0)
        self.assertEqual(worker.state,'failed')

class ExpectedBootTests(unittest.IsolatedAsyncioTestCase):
    async def run_ready(self,expected):
        clock=Clock();port=Port();port.reads=[];sent=False;loaded=[];events=[]
        ready=struct.pack('>4sBB16s',b'SCPS',1,1,struct.pack('>II8s',91,1,bytes(8)))
        def read(cap):
            nonlocal sent
            if clock.now>=2 and not sent:sent=True;return ready
            return b''
        port.read=read
        async def sleep(n):clock.now+=n;await asyncio.sleep(0)
        def loader():loaded.append(True);raise RuntimeError('synthetic boundary')
        worker=w.NumericWorker(approved=True,audio_enabled=True,paused=lambda:False,serial_factory=lambda:port,runtime_loader=loader,emit=events.append,clock=clock,sleep=sleep,consume_approval=lambda:expected)
        await worker.command('prepare');await worker.wait()
        return worker,port,loaded,events
    async def test_changed_nonce_aborts_before_runtime_write_or_grant(self):
        worker,port,loaded,events=await self.run_ready(92)
        self.assertEqual(loaded,[]);self.assertEqual(port.events,['open','close'])
        self.assertTrue(port.closed);self.assertIsNone(worker.port)
        self.assertEqual(worker.first_terminal_result['reason'],'reset_detected')
        self.assertEqual(worker.first_terminal_result['robot_boot_nonce'],91)
        self.assertEqual(events[-1]['state'],'failed')
    async def test_matching_nonce_reaches_runtime_after_ready(self):
        worker,port,loaded,events=await self.run_ready(91)
        self.assertEqual(loaded,[True]);self.assertTrue(port.closed)
        self.assertEqual(worker.first_terminal_result['robot_boot_nonce'],91)

class ApprovalMetadataTests(unittest.TestCase):
    def setUp(self):
        import tempfile
        from pathlib import Path
        self.temp=tempfile.TemporaryDirectory(prefix='numeric-approval-fixture-');self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);(self.root/'notes').mkdir();(self.root/'.tools').mkdir()
        self.patch=patch.object(w,'ROOT',self.root);self.patch.start();self.addCleanup(self.patch.stop)
    def authorization(self,**changes):
        import time
        value={'approved':True,'audio_allowed':False,'valid_until_unix':time.time()+60,'expected_boot_nonce':91};value.update(changes)
        (self.root/'notes/NUMERIC-PAIRING-RUNTIME-AUTHORIZATION.json').write_text(json.dumps(value))
    def test_consumption_is_once_and_receipt_nonsecret(self):
        self.authorization();w._consume_approval()
        path=self.root/'.tools/NUMERIC-PAIRING-CHECK-CONSUMED';receipt=json.loads(path.read_text())
        self.assertEqual(set(receipt),{'consumed_unix','pid','audio_allowed','transport'});self.assertIs(receipt['audio_allowed'],False)
        with self.assertRaises(FileExistsError):w._consume_approval()
    def test_racing_consumers_only_one_succeeds(self):
        from concurrent.futures import ThreadPoolExecutor
        import threading
        self.authorization();barrier=threading.Barrier(2)
        def attempt():
            barrier.wait()
            try:w._consume_approval();return True
            except FileExistsError:return False
        with ThreadPoolExecutor(max_workers=2) as pool:
            results=list(pool.map(lambda _:attempt(),range(2)))
        self.assertEqual(sorted(results),[False,True])
    def test_expired_unapproved_or_audio_permission_denied_without_marker(self):
        for changes in ({'valid_until_unix':0},{'approved':False},{'audio_allowed':True},{'audio_allowed':0}):
            self.authorization(**changes)
            with self.assertRaises(Exception):w._consume_approval()
            self.assertFalse((self.root/'.tools/NUMERIC-PAIRING-CHECK-CONSUMED').exists())
    def test_audio_requires_valid_expected_boot_before_consumption(self):
        for nonce in (None,True,0,-1,2**32,'91'):
            self.authorization(audio_allowed=True,expected_boot_nonce=nonce)
            with self.assertRaises(Exception):w._consume_approval(audio_enabled=True)
            self.assertFalse((self.root/'.tools/NUMERIC-PAIRING-AUDIO-CHECK-CONSUMED').exists())
        self.authorization(audio_allowed=True,expected_boot_nonce=91)
        self.assertEqual(w._consume_approval(audio_enabled=True),91)
    def test_metadata_saves_only_fixed_nonsecret_fields(self):
        payload={'state':'paired','acknowledged':True,'remaining':59,'code':'123456','credential':'synthetic_secret','message':'synthetic_raw','device_nonce':'private_fixture'}
        with patch.object(w,'_output') as output:w._status_metadata(payload)
        record=json.loads((self.root/'notes/NUMERIC-PAIRING-LIVE-STATUS.json').read_text())
        self.assertEqual(set(record),{'state','observed_unix','worker_pid','audio_enabled','acknowledged','remaining'})
        self.assertIs(record['audio_enabled'],False);self.assertTrue(record['acknowledged']);self.assertEqual(record['remaining'],59)
        self.assertNotIn('synthetic',json.dumps(record));self.assertNotIn('123456',json.dumps(record));self.assertFalse((self.root/'notes/NUMERIC-PAIRING-LIVE-STATUS.tmp').exists())
        output.assert_called_once_with(payload)
    def test_invalid_status_creates_no_metadata(self):
        with self.assertRaises(Exception):w._status_metadata({'state':'invalid'})
        self.assertFalse((self.root/'notes/NUMERIC-PAIRING-LIVE-STATUS.json').exists())
    def test_audio_requires_matching_one_use_scope(self):
        self.authorization(audio_allowed=False)
        with self.assertRaises(Exception):w._consume_approval(audio_enabled=True)
        self.assertFalse((self.root/'.tools/NUMERIC-PAIRING-AUDIO-CHECK-CONSUMED').exists())
        self.authorization(audio_allowed=True)
        with self.assertRaises(Exception):w._consume_approval()
        w._consume_approval(audio_enabled=True)
        path=self.root/'.tools/NUMERIC-PAIRING-AUDIO-CHECK-CONSUMED'
        self.assertTrue(json.loads(path.read_text())['audio_allowed'])
        with self.assertRaises(FileExistsError):w._consume_approval(audio_enabled=True)
    def test_audio_marker_does_not_reuse_pairing_receipt(self):
        self.authorization();w._consume_approval()
        self.authorization(audio_allowed=True);w._consume_approval(audio_enabled=True)
        self.assertTrue((self.root/'.tools/NUMERIC-PAIRING-CHECK-CONSUMED').exists())
        self.assertTrue((self.root/'.tools/NUMERIC-PAIRING-AUDIO-CHECK-CONSUMED').exists())
    def test_metadata_reports_authorized_audio_scope(self):
        with patch.object(w,'_output'):w._status_metadata({'state':'staging'},audio_enabled=True)
        value=json.loads((self.root/'notes/NUMERIC-PAIRING-LIVE-STATUS.json').read_text())
        self.assertIs(value['audio_enabled'],True)

    def test_metadata_reason_is_fixed_whitelist_only(self):
        for message in ('expired','transport_lost','synthetic_secret_credential'):
            with patch.object(w,'_output'):w._status_metadata({'state':'failed','message':message})
            record=json.loads((self.root/'notes/NUMERIC-PAIRING-LIVE-STATUS.json').read_text())
            if message=='synthetic_secret_credential':self.assertNotIn('reason',record)
            else:self.assertEqual(record['reason'],message)
    def test_transport_fault_details_are_projected_without_secret_text_or_stdout(self):
        diagnostic={'phase':'authorization','heartbeat_count':0,'ack_received':True,
            'fault':{'type':'TimeoutError','phase':'authorization','stack':[],
                'message':'synthetic_secret'},'credential':'synthetic_secret'}
        stats={'outcome':'fault','diagnostics':diagnostic,'raw_pcm':b'synthetic'}
        payload={'state':'failed','message':'audio_transport_failed'}
        with patch.object(w,'_output') as output:
            w._status_metadata(payload,audio_enabled=True,transport_result=stats)
        record=json.loads((self.root/'notes/NUMERIC-PAIRING-LIVE-STATUS.json').read_text())
        details=record['transport']['diagnostics']
        self.assertEqual(details['fault']['type'],'TimeoutError')
        self.assertNotIn('synthetic_secret',json.dumps(record))
        output.assert_called_once_with(payload)
        with patch.object(w,'_output'):w._status_metadata(payload,transport_result=stats)
        self.assertNotIn('transport',json.loads((self.root/'notes/NUMERIC-PAIRING-LIVE-STATUS.json').read_text()))

    def test_transport_projection_only_for_audio_and_not_stdout(self):
        stats={'decoded_mic_frames':960,'raw_pcm':b'synthetic','credential':'synthetic_secret','outcome':'fault'}
        payload={'state':'cancelled'}
        with patch.object(w,'_output') as output:w._status_metadata(payload,audio_enabled=True,transport_result=stats)
        record=json.loads((self.root/'notes/NUMERIC-PAIRING-LIVE-STATUS.json').read_text())
        self.assertEqual(record['transport']['decoded_mic_frames'],960);self.assertNotIn('raw_pcm',record['transport']);self.assertNotIn('credential',record['transport'])
        output.assert_called_once_with(payload)
        with patch.object(w,'_output'):w._status_metadata(payload,transport_result=stats)
        self.assertNotIn('transport',json.loads((self.root/'notes/NUMERIC-PAIRING-LIVE-STATUS.json').read_text()))
class AudioWorkerTests(unittest.IsolatedAsyncioTestCase):
    async def test_audio_flag_without_general_approval_never_opens(self):
        opened=[]
        worker=w.NumericWorker(approved=False,audio_enabled=True,paused=lambda:False,serial_factory=lambda:opened.append(True),runtime_loader=lambda:None,emit=lambda value:None)
        await worker.command('prepare');await worker.wait()
        self.assertEqual(opened,[]);self.assertFalse(worker.audio_authorized)
    async def test_audio_flag_without_matching_file_never_opens(self):
        import tempfile,time
        from pathlib import Path
        with tempfile.TemporaryDirectory(prefix='audio-scope-fixture-') as folder:
            root=Path(folder);(root/'notes').mkdir();(root/'.tools').mkdir()
            (root/'notes/NUMERIC-PAIRING-RUNTIME-AUTHORIZATION.json').write_text(json.dumps({'approved':True,'audio_allowed':False,'valid_until_unix':time.time()+60}))
            opened=[]
            worker=w.NumericWorker(approved=True,audio_enabled=True,paused=lambda:False,serial_factory=lambda:opened.append(True),runtime_loader=lambda:None,emit=lambda value:None,consume_approval=lambda:w._consume_approval(audio_enabled=True))
            with patch.object(w,'ROOT',root):await worker.command('prepare');await worker.wait()
            self.assertEqual(opened,[]);self.assertFalse(worker.audio_authorized)
    async def test_ready_receipt_is_emitted_only_after_correlated_staged_ack(self):
        import numeric_pairing as n
        from test_numeric_pairing import Clock,REQ,DN,HN
        for scope in (False,True):
            clock=Clock();core=n.NumericPairing(issuer=lambda ttl:'synthetic-noncredential-000000000000',revoke=lambda:None,clock=clock,nonce_factory=lambda:HN,code_factory=lambda:'123456',audio_requested=scope)
            core.prepare(REQ,91)
            class Transport:
                def process_request(self,*args):return None
            gateway=n.NumericGateway(core,Transport(),audio_enabled=True)
            class Connection:
                def respond(self,status,body):return status
            class AudioRequest:path='/companion/audio'
            class Socket:
                request=type('Request',(),{'path':'/companion/pair'})()
                def __init__(self):
                    self.queue=asyncio.Queue();self.ready=None
                    self.queue.put_nowait(json.dumps(dict(type='pair.hello',version=1,request_id=REQ,device=n.DEVICE,device_nonce=DN,boot_nonce=91,capabilities=['microphone','speaker'])))
                async def recv(self):return await self.queue.get()
                async def send(self,text):
                    value=json.loads(text)
                    if value['type']=='pair.challenge':
                        self.assert_scope(value)
                        core.mac_confirm()
                        self.queue.put_nowait(json.dumps(dict(type='pair.confirm',version=1,request_id=REQ,device_nonce=DN,host_nonce=HN,physical=True)))
                    elif value['type']=='pair.grant':
                        assert not core.audio_allowed
                        self.assert_scope(value)
                        self.queue.put_nowait(json.dumps(dict(type='pair.staged',version=1,request_id=REQ,device_nonce=DN,host_nonce=HN,staged=True)))
                    elif value['type']=='pair.ready':
                        assert core.audio_allowed and core.state=='paired'
                        assert gateway.process_request(Connection(),AudioRequest())==(None if scope else 403)
                        self.ready=value;core.cancel('closed','closed')
                    else:raise AssertionError('unexpected response')
                def assert_scope(self,value):assert value['audio_requested'] is scope
                async def close(self):pass
            socket=Socket()
            async with asyncio.timeout(1):await gateway.handle(socket)
            self.assertEqual(socket.ready,dict(type='pair.ready',version=1,request_id=REQ,device_nonce=DN,host_nonce=HN,audio_requested=scope,ready=True))

    async def test_opt_in_gateway_requires_both_confirm_and_ack_one_audio_only(self):
        import numeric_pairing as n
        from test_numeric_pairing import Clock,REQ,DN,HN
        revoked=[];clock=Clock();handled=[]
        core=n.NumericPairing(issuer=lambda ttl:'synthetic-noncredential-000000000000',revoke=lambda:revoked.append(True),clock=clock,nonce_factory=lambda:HN,code_factory=lambda:'123456',audio_requested=True)
        core.prepare(REQ,91)
        class Transport:
            def process_request(self,*args):return None
            async def handle(self,socket):handled.append(socket)
        gateway=n.NumericGateway(core,Transport(),audio_enabled=True)
        class Connection:
            def respond(self,status,body):return status
        class Request:path='/companion/audio'
        self.assertEqual(gateway.process_request(Connection(),Request()),403)
        core.hello(dict(type='pair.hello',version=1,request_id=REQ,device=n.DEVICE,device_nonce=DN,boot_nonce=91,capabilities=['microphone','speaker']))
        core.mac_confirm();self.assertEqual(gateway.process_request(Connection(),Request()),403)
        core.robot_confirm(dict(type='pair.confirm',version=1,request_id=REQ,device_nonce=DN,host_nonce=HN,physical=True));core.take_grant()
        self.assertEqual(gateway.process_request(Connection(),Request()),403)
        core.staged(dict(type='pair.staged',version=1,request_id=REQ,device_nonce=DN,host_nonce=HN,staged=True))
        self.assertIsNone(gateway.process_request(Connection(),Request()))
        class Socket:
            request=Request()
            async def close(self):pass
        first=Socket();await gateway.handle(first);await gateway.handle(Socket())
        self.assertEqual(handled,[first])
        await gateway.close();self.assertEqual(revoked,[True]);self.assertFalse(core.audio_allowed)
    async def test_opt_in_audio_expiry_revokes(self):
        import numeric_pairing as n
        from test_numeric_pairing import Clock,REQ,DN,HN
        clock=Clock();revoked=[]
        core=n.NumericPairing(issuer=lambda ttl:'synthetic-noncredential-000000000000',revoke=lambda:revoked.append(True),clock=clock,nonce_factory=lambda:HN,code_factory=lambda:'123456');core.prepare(REQ,91)
        core.hello(dict(type='pair.hello',version=1,request_id=REQ,device=n.DEVICE,device_nonce=DN,boot_nonce=91,capabilities=['microphone','speaker']));core.mac_confirm();core.robot_confirm(dict(type='pair.confirm',version=1,request_id=REQ,device_nonce=DN,host_nonce=HN,physical=True));core.take_grant();core.staged(dict(type='pair.staged',version=1,request_id=REQ,device_nonce=DN,host_nonce=HN,staged=True))
        clock.now=120
        with self.assertRaises(n.PairingError):core.tick()
        self.assertEqual(revoked,[True]);self.assertFalse(core.audio_allowed)
    async def test_worker_gate_uses_consumed_scope_and_ready_anchor(self):
        from contextlib import asynccontextmanager
        import tempfile,time
        from pathlib import Path
        for enabled in (False,True):
            with self.subTest(audio_enabled=enabled),tempfile.TemporaryDirectory(prefix='scope-ready-fixture-') as folder:
                root=Path(folder);(root/'notes').mkdir();(root/'.tools').mkdir()
                (root/'notes/NUMERIC-PAIRING-RUNTIME-AUTHORIZATION.json').write_text(json.dumps({'approved':True,'audio_allowed':enabled,'valid_until_unix':time.time()+60,'expected_boot_nonce':91}))
                clock=Clock();port=Port();port.reads=[];sent=False;observed=[]
                ready=struct.pack('>4sBB16s',b'SCPS',1,1,struct.pack('>II8s',91,1,bytes(8)))
                def read(cap):
                    nonlocal sent
                    if clock.now>=2 and not sent:sent=True;return ready
                    return b''
                port.read=read
                class Runtime:
                    transport=object()
                    def issue(self,ttl):return 'synthetic-noncredential-000000000000'
                    def revoke(self):pass
                    @asynccontextmanager
                    async def listener(self,gateway):
                        observed.append((gateway.audio_enabled,gateway.pairing.deadline))
                        yield
                loaded_at=[]
                def load():loaded_at.append(clock.now);clock.now+=5;return Runtime()
                async def sleep(n):clock.now+=n;await asyncio.sleep(0)
                worker=w.NumericWorker(approved=True,audio_enabled=enabled,paused=lambda:False,serial_factory=lambda:port,runtime_loader=load,emit=lambda value:None,clock=clock,sleep=sleep,consume_approval=lambda:w._consume_approval(audio_enabled=enabled))
                with patch.object(w,'ROOT',root):
                    await worker.command('prepare')
                    for _ in range(500):
                        if any(type(x) is tuple for x in port.events):break
                        await asyncio.sleep(0)
                    self.assertTrue(worker.audio_authorized is enabled)
                    self.assertEqual(observed,[(enabled,loaded_at[0]+118)])
                    self.assertEqual(len([x for x in port.events if type(x) is tuple]),1)
                    await worker.command('cancel');await worker.wait()
                self.assertTrue(port.closed)
class DeadlineReceiptTests(unittest.IsolatedAsyncioTestCase):
    async def scenario(self,loader_delay=0,listener_delay=0,timeout=False,audio=True):
        from contextlib import asynccontextmanager
        clock=Clock();port=Port();port.reads=[];sent=False;entered=[];closed=[];outputs=[]
        ready=struct.pack('>4sBB16s',b'SCPS',1,1,struct.pack('>II8s',91,1,bytes(8)))
        def read(cap):
            nonlocal sent
            if clock.now>=2 and not sent:sent=True;return ready
            return b''
        port.read=read
        class Transport:
            last_result={'decoded_mic_frames':960,'decoded_mic_peak':900,'decoded_mic_energy':1000,'speaker_packets':1,'identity_verified':False,'robot_playback_verified':False,'outcome':'fault','raw_pcm':b'synthetic','credential':'synthetic_secret','code':'123456'}
        class Runtime:
            transport=Transport()
            def issue(self,ttl):return 'synthetic-noncredential-000000000000'
            def revoke(self):pass
            @asynccontextmanager
            async def listener(self,gateway):
                entered.append(True);clock.now+=listener_delay
                try:
                    if timeout:raise TimeoutError('synthetic listener boundary')
                    yield
                finally:closed.append(True)
        def load():clock.now+=loader_delay;return Runtime()
        async def sleep(n):clock.now+=n;await asyncio.sleep(0)
        worker=w.NumericWorker(approved=True,audio_enabled=audio,paused=lambda:False,serial_factory=lambda:port,runtime_loader=load,emit=outputs.append,clock=clock,sleep=sleep)
        await worker.command('prepare')
        if loader_delay>=118 or listener_delay>=118 or timeout:
            await worker.wait()
        else:
            for _ in range(500):
                if any(type(x) is tuple for x in port.events):break
                await asyncio.sleep(0)
            await worker.command('cancel');await worker.wait()
        return worker,port,entered,closed,outputs
    async def test_loader_elapsed_budget_no_listener_or_numeric_write(self):
        worker,port,entered,_,_=await self.scenario(loader_delay=120)
        self.assertEqual(worker.state,'expired');self.assertEqual(entered,[]);self.assertFalse(any(type(x) is tuple for x in port.events));self.assertTrue(port.closed)
    async def test_slow_listener_start_no_numeric_write_and_cleanup(self):
        worker,port,entered,closed,_=await self.scenario(listener_delay=118)
        self.assertEqual(worker.state,'expired');self.assertEqual(entered,[True]);self.assertEqual(closed,[True]);self.assertFalse(any(type(x) is tuple for x in port.events));self.assertTrue(port.closed)
    async def test_early_timeout_remains_failed(self):
        worker,port,_,closed,_=await self.scenario(timeout=True)
        self.assertEqual(worker.state,'failed');self.assertTrue(port.closed);self.assertEqual(closed,[True])
    async def test_timeout_at_actual_deadline_is_expired(self):
        worker,port,_,closed,_=await self.scenario(listener_delay=118,timeout=True)
        self.assertEqual(worker.state,'expired');self.assertTrue(port.closed);self.assertEqual(closed,[True])
    async def test_audio_receipt_only_after_owned_cleanup_and_projected(self):
        worker,port,_,closed,_=await self.scenario()
        self.assertTrue(port.closed);self.assertEqual(closed,[True]);self.assertEqual(worker.last_transport_result['decoded_mic_frames'],960)
        self.assertFalse(any(key in worker.last_transport_result for key in ('credential','raw_pcm','code')))
        self.assertFalse(any(isinstance(value,(bytes,bytearray)) for value in worker.last_transport_result.values()))
        self.assertFalse(worker.last_transport_result['identity_verified'])
    async def test_pairing_only_does_not_capture_audio_receipt(self):
        worker,_,_,_,_=await self.scenario(audio=False)
        self.assertIsNone(worker.last_transport_result)
    async def test_runtime_uses_remaining_pairing_budget_after_import(self):
        import numeric_pairing as n,sys,types
        from contextlib import asynccontextmanager
        from test_numeric_pairing import REQ
        clock=Clock();core=n.NumericPairing(issuer=lambda ttl:'synthetic',revoke=lambda:None,clock=clock);core.prepare(REQ,91,deadline_budget_seconds=118);clock.now=7
        gateway=n.NumericGateway(core,object());observed=[]
        @asynccontextmanager
        async def fake_serve(*args,**kwargs):yield
        @asynccontextmanager
        async def timeout_context(budget):observed.append(budget);yield
        runtime=w._Runtime(None,None,None)
        with patch.dict(sys.modules,{'websockets.asyncio.server':types.SimpleNamespace(serve=fake_serve)}),patch.object(w.asyncio,'timeout',timeout_context):
            async with runtime.listener(gateway):pass
        self.assertEqual(observed,[111])
class DiagnosticPreservationTests(unittest.IsolatedAsyncioTestCase):
    async def test_denied_retry_preserves_original_reason_audio_scope_and_counts(self):
        fixture=DeadlineReceiptTests();worker,_,_,_,_=await fixture.scenario(listener_delay=118,timeout=True)
        self.assertEqual(worker.state,'expired');self.assertTrue(worker.audio_authorized)
        before=dict(worker.last_transport_result);first=dict(worker.first_terminal_result)
        def denied():raise FileExistsError('synthetic consumed marker')
        worker.consume_approval=denied
        await worker.command('retry');await worker.wait()
        self.assertEqual(worker.first_terminal_result,first);self.assertEqual(worker.first_terminal_result['reason'],'expired')
        self.assertEqual(worker.last_transport_result,before);self.assertTrue(worker.audio_authorized)
        self.assertEqual(worker.state,'failed')
    async def test_first_receipt_snapshot_not_mutable_by_later_stats(self):
        fixture=DeadlineReceiptTests();worker,_,_,_,_=await fixture.scenario()
        original=worker.first_terminal_result['transport']['decoded_mic_frames']
        worker.last_transport_result['decoded_mic_frames']=0
        self.assertEqual(worker.first_terminal_result['transport']['decoded_mic_frames'],original)

class HistoryTests(unittest.TestCase):
    setUp=ApprovalMetadataTests.setUp
    authorization=ApprovalMetadataTests.authorization
    def test_history_appends_projected_records_and_keeps_first_failure(self):
        with patch.object(w,'_output'):
            w._status_metadata({'state':'failed','message':'transport_lost','code':'123456','credential':'synthetic_secret'},audio_enabled=True,transport_result={'decoded_mic_frames':960})
            w._status_metadata({'state':'failed','message':'approval_consumed'})
        history=(self.root/'notes/NUMERIC-PAIRING-STATUS-HISTORY.jsonl').read_text().splitlines()
        records=[json.loads(line) for line in history]
        self.assertEqual(records[0]['reason'],'transport_lost');self.assertEqual(records[0]['transport']['decoded_mic_frames'],960)
        self.assertEqual(records[1]['reason'],'approval_consumed');self.assertNotIn('synthetic_secret',''.join(history));self.assertNotIn('123456',''.join(history))
    def test_history_is_bounded_append_only(self):
        with patch.object(w,'_output'):
            for _ in range(140):w._status_metadata({'state':'idle'})
        path=self.root/'notes/NUMERIC-PAIRING-STATUS-HISTORY.jsonl';before=path.read_bytes()
        self.assertLessEqual(len(before),65536);self.assertLessEqual(len(before.splitlines()),128)
        with patch.object(w,'_output'):w._status_metadata({'state':'failed','message':'approval_consumed'})
        self.assertEqual(path.read_bytes(),before)
    def test_first_terminal_projection_excludes_unknown_fields(self):
        first={'state':'failed','reason':'transport_lost','audio_enabled':True,'transport':{'decoded_mic_frames':960,'raw_pcm':b'not-retained'},'credential':'synthetic_secret','code':'123456'}
        with patch.object(w,'_output'):w._status_metadata({'state':'failed','message':'approval_consumed'},first_terminal_result=first)
        record=json.loads((self.root/'notes/NUMERIC-PAIRING-LIVE-STATUS.json').read_text())
        self.assertEqual(record['first_terminal']['reason'],'transport_lost');self.assertEqual(record['first_terminal']['transport']['decoded_mic_frames'],960)
        self.assertNotIn('credential',record['first_terminal']);self.assertNotIn('raw_pcm',record['first_terminal']['transport'])
class BootMetadataTests(unittest.TestCase):
    setUp=ApprovalMetadataTests.setUp
    def test_verified_identity_is_metadata_only_and_first_terminal_retained(self):
        payload={'state':'failed','message':'transport_lost'}
        first={'state':'failed','audio_enabled':False,'robot_boot_nonce':91,'robot_reset_reason':3}
        with patch.object(w,'_output') as output:
            w._status_metadata(payload,robot_boot_nonce=92,robot_reset_reason=4,first_terminal_result=first)
        record=json.loads((self.root/'notes/NUMERIC-PAIRING-LIVE-STATUS.json').read_text())
        self.assertEqual(record['robot_boot_nonce'],92);self.assertEqual(record['robot_reset_reason'],4)
        self.assertEqual(record['first_terminal']['robot_boot_nonce'],91);self.assertEqual(record['first_terminal']['robot_reset_reason'],3)
        output.assert_called_once_with(payload)
        history=json.loads((self.root/'notes/NUMERIC-PAIRING-STATUS-HISTORY.jsonl').read_text().splitlines()[0])
        self.assertEqual(history['robot_boot_nonce'],92)
    def test_invalid_or_unverified_identity_is_omitted_as_a_pair(self):
        for nonce,reason in ((None,None),(0,1),(True,1),(0x100000000,1),(91,True),(91,-1),(91,65),(91,'3')):
            with self.subTest(nonce=nonce,reason=reason),patch.object(w,'_output'):
                w._status_metadata({'state':'failed'},robot_boot_nonce=nonce,robot_reset_reason=reason,first_terminal_result={'state':'failed','robot_boot_nonce':nonce,'robot_reset_reason':reason})
            value=json.loads((self.root/'notes/NUMERIC-PAIRING-LIVE-STATUS.json').read_text())
            self.assertNotIn('robot_boot_nonce',value);self.assertNotIn('robot_reset_reason',value)
            self.assertNotIn('robot_boot_nonce',value['first_terminal']);self.assertNotIn('robot_reset_reason',value['first_terminal'])
class WorkerBootIdentityTests(unittest.IsolatedAsyncioTestCase):
    async def test_ready_identity_reset_on_new_authorized_prepare_without_old_reuse(self):
        import tempfile
        from pathlib import Path
        temporary=tempfile.TemporaryDirectory(prefix='ready-identity-fixture-');self.addCleanup(temporary.cleanup)
        root=Path(temporary.name);(root/'notes').mkdir()
        root_patch=patch.object(w,'ROOT',root);root_patch.start();self.addCleanup(root_patch.stop)
        output_patch=patch.object(w,'_output');output=output_patch.start();self.addCleanup(output_patch.stop)
        clock=Clock();attempts=[];observed=[]
        async def sleep(n):clock.now+=n;await asyncio.sleep(0)
        def factory():
            port=Port();port.reads=[];attempts.append(port)
            if len(attempts)==2:
                def open_failure():raise OSError('synthetic-no-secret')
                port.open=open_failure;return port
            sent=False
            ready=struct.pack('>4sBB16s',b'SCPS',1,1,struct.pack('>II8s',91,3,bytes(8)))
            def read(cap):
                nonlocal sent
                if clock.now>=2 and not sent:sent=True;return ready
                return b''
            port.read=read;return port
        def emit(value):
            observed.append((value['state'],worker.robot_boot_nonce,worker.robot_reset_reason))
            w._status_metadata(value,robot_boot_nonce=worker.robot_boot_nonce,robot_reset_reason=worker.robot_reset_reason,first_terminal_result=worker.first_terminal_result)

        def load():raise RuntimeError('synthetic runtime boundary')
        worker=w.NumericWorker(approved=True,paused=lambda:False,serial_factory=factory,runtime_loader=load,emit=emit,clock=clock,sleep=sleep)
        await worker.command('prepare');await worker.wait()
        self.assertEqual((worker.robot_boot_nonce,worker.robot_reset_reason),(91,3))
        self.assertEqual(worker.first_terminal_result['robot_boot_nonce'],91)
        record=json.loads((root/'notes/NUMERIC-PAIRING-LIVE-STATUS.json').read_text())
        self.assertEqual((record['robot_boot_nonce'],record['robot_reset_reason']),(91,3))
        self.assertEqual(record['first_terminal']['robot_boot_nonce'],91)
        self.assertTrue(all('robot_boot_nonce' not in call.args[0] and 'robot_reset_reason' not in call.args[0] for call in output.call_args_list))
        observed.clear();await worker.command('retry');await worker.wait()
        self.assertEqual(observed[0],('opening',None,None));self.assertEqual((worker.robot_boot_nonce,worker.robot_reset_reason),(None,None))
        self.assertEqual(worker.first_terminal_result['robot_boot_nonce'],91)
        record=json.loads((root/'notes/NUMERIC-PAIRING-LIVE-STATUS.json').read_text())
        self.assertNotIn('robot_boot_nonce',record);self.assertNotIn('robot_reset_reason',record)
        self.assertEqual(record['first_terminal']['robot_boot_nonce'],91)
if __name__=='__main__':unittest.main()
