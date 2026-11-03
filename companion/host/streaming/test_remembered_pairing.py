"""Offline only: temporary SQLite and synthetic credentials, no listeners."""
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
try:
    import remembered_pairing as r
except ImportError:
    r = None
REQ='12'*16; DN='34'*16; HN='56'*16; CA='ab'*32
TOKEN='synthetic-credential-for-offline-check-000000'
class Tests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(r,'remembered host missing')
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.clock=lambda:1000
        self.registry=r.RememberedRegistry(Path(self.temp.name)/'private',CA,2000,clock=self.clock)
        self.addCleanup(self.registry.db.close)
        self.events=[]
        self.p=r.RememberedPairing(self.registry,emit=self.events.append,clock=self.clock,nonce_factory=lambda:HN,code_factory=lambda:'123456')
    def hello(self,**changes):
        value=dict(type='pair.remember.hello',version=1,request_id=REQ,device=r.DEVICE,device_nonce=DN,boot_nonce=7,capabilities=['microphone','speaker']);value.update(changes)
        return self.p.hello(value)
    def context(self,kind,**extra):return dict(type=kind,version=1,request_id=REQ,device_nonce=DN,host_nonce=HN,**extra)
    def grant(self):
        self.p.enroll();self.hello();self.p.mac_confirm()
        with patch('home_lab.devices.secrets.token_urlsafe',return_value=TOKEN):
            self.p.robot_confirm(self.context('pair.remember.confirm',physical=True,remember=True))
        return self.p.take_grant()
    def test_inert_and_both_confirmations(self):
        with self.assertRaises(ValueError):self.hello()
        self.p.enroll();challenge=self.hello();self.assertFalse(challenge['audio_requested'])
        self.p.mac_confirm();self.assertIsNone(self.p.take_grant())
        self.assertEqual(self.registry.status(),'unpaired')
    def test_pending_denied_then_exact_ack_survives_restart(self):
        grant=self.grant();self.assertIsNone(self.registry.authenticate(grant['credential']))
        self.p.stored(self.context('pair.remember.stored',stored=True))
        other=r.RememberedRegistry(Path(self.temp.name)/'private',CA,2000,clock=self.clock)
        self.addCleanup(other.db.close)
        self.assertEqual(other.authenticate(TOKEN),r.DEVICE)
        self.assertNotIn(TOKEN,json.dumps(self.events));self.assertNotIn(TOKEN,repr(self.p))
        self.assertNotIn(TOKEN,self.registry.store.path.read_bytes().decode('latin1'))
    def test_old_process_cache_cannot_authenticate_replaced_hash(self):
        self.grant();self.p.stored(self.context('pair.remember.stored',stored=True))
        from hashlib import sha256
        with self.registry.db:
            self.registry.db.execute("UPDATE device_credentials SET credential_hash=? WHERE device_id=?",(sha256(b'synthetic-replacement').hexdigest(),r.DEVICE))
        self.assertIsNone(self.registry.authenticate(TOKEN))
    def test_interrupted_pending_revoked_on_restart(self):
        self.grant()
        other=r.RememberedRegistry(Path(self.temp.name)/'private',CA,2000,clock=self.clock)
        self.addCleanup(other.db.close)
        self.assertIsNone(other.authenticate(TOKEN));self.assertEqual(other.status(),'revoked')
    def test_stale_ack_cannot_commit(self):
        self.grant();value=self.context('pair.remember.stored',stored=True);value['host_nonce']='78'*16
        with self.assertRaises(ValueError):self.p.stored(value)
        self.assertIsNone(self.registry.authenticate(TOKEN))
    def test_forget_persists(self):
        self.grant();self.p.stored(self.context('pair.remember.stored',stored=True));self.registry.forget()
        other=r.RememberedRegistry(Path(self.temp.name)/'private',CA,2000,clock=self.clock);self.addCleanup(other.db.close)
        self.assertIsNone(other.authenticate(TOKEN))
    def test_changed_trust_or_expiry_denied(self):
        self.grant();self.p.stored(self.context('pair.remember.stored',stored=True))
        with self.assertRaises(ValueError):r.RememberedRegistry(Path(self.temp.name)/'private','cd'*32,2000,clock=self.clock)
        other=r.RememberedRegistry(Path(self.temp.name)/'private',CA,2000,clock=lambda:2000);self.addCleanup(other.db.close)
        self.assertIsNone(other.authenticate(TOKEN))
    def test_foreign_device_and_boolean_version_denied(self):
        self.p.enroll()
        for change in ({'device':'other'},{'version':True},{'capabilities':['speaker']},{'request_id':'0'*32}):
            with self.assertRaises(ValueError):self.hello(**change)
    def test_active_record_not_overwritten(self):
        self.grant();self.p.stored(self.context('pair.remember.stored',stored=True))
        with self.assertRaises(ValueError):self.p.enroll()
    def test_expired_ack_revokes(self):
        now=[1000];self.p.clock=lambda:now[0];self.grant();now[0]=1121
        with self.assertRaises(ValueError):self.p.stored(self.context('pair.remember.stored',stored=True))
        self.assertIsNone(self.registry.authenticate(TOKEN))

class ServiceTests(unittest.IsolatedAsyncioTestCase):
    async def test_commands_are_inert_until_user_and_route_gate(self):
        try:from remembered_pairing_worker import RememberedWorker
        except ImportError:RememberedWorker=None
        self.assertIsNotNone(RememberedWorker,'worker integration missing')
        loaded=[];events=[]
        worker=RememberedWorker(lambda:loaded.append(True),emit=events.append)
        self.assertEqual(loaded,[])
        with self.assertRaises((ValueError,PermissionError)):await worker.command('connect')
        self.assertEqual(loaded,[])
    async def test_app_protocol_never_accepts_secret_or_route(self):
        try:from remembered_pairing_worker import parse_command
        except ImportError:parse_command=None
        self.assertIsNotNone(parse_command,'worker app protocol missing')
        self.assertEqual(parse_command('{"command":"enroll"}'),'enroll')
        route={'input_uid':'Pro Tools Audio Bridge 2-A_UID','output_uid':'Pro Tools Audio Bridge 2-B_UID'}
        self.assertEqual(parse_command(json.dumps({'command':'connect','audio_route':route})), 'connect')
        for body in ('{"command":"connect","credential":"synthetic"}','{"command":"connect","route":{}}','{"command":"connect","audio_route":{"input_uid":"wrong"}}','{"command":"enroll","command":"forget"}'):
            with self.assertRaises((ValueError,PermissionError)):parse_command(body)

class GatewayTests(unittest.IsolatedAsyncioTestCase):
    async def exercise(self,ready_lost=False):
        import asyncio
        from types import SimpleNamespace
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup)
        registry=r.RememberedRegistry(Path(temp.name)/'private',CA,2000,clock=lambda:1000);self.addCleanup(registry.db.close)
        pairing=r.RememberedPairing(registry,clock=lambda:1000,nonce_factory=lambda:HN,code_factory=lambda:'123456');pairing.enroll()
        class Socket:
            request=SimpleNamespace(path='/companion/pair')
            def __init__(self):self.messages=asyncio.Queue();self.received=asyncio.Queue();self.closed=False
            async def recv(self):return await self.messages.get()
            async def send(self,wire):
                value=json.loads(wire)
                if ready_lost and value['type']=='pair.remember.ready':raise OSError('synthetic lost ready')
                await self.received.put(value)
            async def close(self,**kwargs):self.closed=True
        socket=Socket();gateway=r.RememberedGateway(pairing)
        await socket.messages.put(json.dumps(dict(type='pair.remember.hello',version=1,request_id=REQ,device=r.DEVICE,device_nonce=DN,boot_nonce=7,capabilities=['microphone','speaker'])))
        with patch('home_lab.devices.secrets.token_urlsafe',return_value=TOKEN):
            task=asyncio.create_task(gateway.handle(socket))
            challenge=await asyncio.wait_for(socket.received.get(),1);self.assertFalse(challenge['audio_requested'])
            pairing.mac_confirm()
            await socket.messages.put(json.dumps(pairing.message('confirm',physical=True,remember=True)))
            grant=await asyncio.wait_for(socket.received.get(),1)
            self.assertEqual(grant['type'],'pair.remember.grant');self.assertIsNone(registry.authenticate(TOKEN))
            await socket.messages.put(json.dumps(pairing.message('stored',stored=True)))
            await asyncio.wait_for(task,1)
        self.assertTrue(socket.closed);self.assertEqual(registry.authenticate(TOKEN),r.DEVICE)
        await gateway.close();self.assertEqual(registry.authenticate(TOKEN),r.DEVICE)
    async def test_direct_tls_local_grant_and_exact_storage_ack(self):await self.exercise()
    async def test_lost_final_ready_preserves_committed_registration(self):await self.exercise(ready_lost=True)


class WorkerCleanupTests(unittest.IsolatedAsyncioTestCase):
    async def test_failed_forget_still_closes_owner_and_blocks_connect(self):
        from remembered_pairing_worker import RememberedWorker
        from types import SimpleNamespace
        closed=[]
        class Gateway:
            async def close(self):closed.append(True)
        def fail():raise OSError('synthetic database unavailable')
        worker=RememberedWorker(lambda:None,emit=lambda v:None)
        worker.runtime=SimpleNamespace(registry=SimpleNamespace(forget=fail));worker.gateway=Gateway()
        with self.assertRaises(Exception):await worker.command('forget')
        self.assertEqual(closed,[True]);self.assertTrue(worker.cleanup_blocked)
    async def test_activity_projects_armed_countdown(self):
        from remembered_pairing_worker import RememberedWorker
        from types import SimpleNamespace
        events=[];worker=RememberedWorker(lambda:None,emit=events.append)
        worker.gateway=SimpleNamespace(activity_state='conversation',audio_remaining=17)
        worker.publish_activity()
        self.assertEqual(events[-1],dict(state='paired',acknowledged=True,activity='conversation',audio_remaining=17))
    async def test_unenrolled_connect_does_not_consume_native_authority(self):
        from remembered_pairing_worker import RememberedWorker
        from types import SimpleNamespace
        loaded=[];worker=RememberedWorker(lambda:(loaded.append(True) or SimpleNamespace(registry=SimpleNamespace(status=lambda:'unpaired'))),emit=lambda v:None)
        route={'input_uid':'Pro Tools Audio Bridge 2-A_UID','output_uid':'Pro Tools Audio Bridge 2-B_UID'}
        with self.assertRaises(ValueError):await worker.command('connect',route)
        self.assertEqual(loaded,[True])

    async def test_listener_stays_ready_until_cancel_for_automatic_reconnect(self):
        import asyncio
        import remembered_pairing_worker as w
        from contextlib import asynccontextmanager
        events=[];entered=asyncio.Event();closed=[]
        @asynccontextmanager
        async def listener_context(_gateway):
            entered.set()
            try:yield
            finally:closed.append(True)
        class Runtime:
            listener=staticmethod(listener_context)
        class Gateway:
            activity_state='physical_consent';audio_remaining=None;completion_started=False
            async def close(self):pass
        worker=w.RememberedWorker(lambda:Runtime(),emit=events.append);worker.runtime=Runtime();worker.gateway=Gateway()
        task=asyncio.create_task(worker.listen(None))
        await asyncio.wait_for(entered.wait(),1);await asyncio.sleep(.12)
        self.assertFalse(task.done());self.assertEqual(closed,[])
        worker.gateway.completion_started=True
        await asyncio.sleep(.12)
        self.assertFalse(task.done());self.assertFalse(worker.gateway.completion_started)
        task.cancel()
        await asyncio.gather(task,return_exceptions=True)
        self.assertEqual(closed,[True])

    async def test_use_stackchan_starts_trusted_listener_without_expiring_route_race(self):
        import asyncio
        from contextlib import asynccontextmanager
        from types import SimpleNamespace
        from remembered_pairing_worker import RememberedWorker
        entered=asyncio.Event();events=[]
        @asynccontextmanager
        async def listener(_gateway):
            entered.set()
            try:yield
            finally:pass
        runtime=SimpleNamespace(registry=SimpleNamespace(status=lambda:'active'),listener=listener)
        worker=RememberedWorker(lambda:runtime,emit=events.append)
        route={'input_uid':'Pro Tools Audio Bridge 2-A_UID','output_uid':'Pro Tools Audio Bridge 2-B_UID'}
        await worker.command('connect',route)
        await asyncio.wait_for(entered.wait(),1)
        await asyncio.sleep(.1)
        self.assertTrue(worker.task and not worker.task.done())
        self.assertTrue(any(event.get('activity')=='physical_consent' for event in events))
        await worker.stop()
        self.assertIsNone(worker.gateway)

class ShutdownTests(unittest.IsolatedAsyncioTestCase):
    async def test_failed_close_retains_owner_without_success_status_until_retry(self):
        import asyncio
        import remembered_pairing_worker as w
        self.assertTrue(hasattr(w,'shutdown_owned'),'checked shutdown helper missing')
        events=[];pause=asyncio.Event();entered=asyncio.Event()
        class Gateway:
            fails=True
            async def close(self):
                if self.fails:raise OSError('synthetic close blocked')
        owner=Gateway();worker=w.RememberedWorker(lambda:None,emit=events.append);worker.gateway=owner
        async def sleep(_):entered.set();await pause.wait()
        task=asyncio.create_task(w.shutdown_owned(worker,sleep=sleep))
        await asyncio.wait_for(entered.wait(),1)
        self.assertFalse(task.done());self.assertIs(worker.gateway,owner)
        self.assertFalse(any(v.get('state')=='closed' for v in events))
        self.assertTrue(worker.cleanup_blocked)
        owner.fails=False;pause.set();await asyncio.wait_for(task,1)
        self.assertIsNone(worker.gateway);self.assertEqual(events[-1],{'state':'closed','message':'remembered_cleanup_verified'})
    async def test_top_level_setup_error_is_nonzero_sanitized_failure(self):
        import remembered_pairing_worker as w
        from unittest.mock import AsyncMock
        from io import StringIO
        output=StringIO()
        # main's asyncio boundary is replaced; this fixture starts no event loop or IO.
        with patch.object(w.asyncio,'run',side_effect=lambda coroutine:(coroutine.close(),(_ for _ in ()).throw(OSError('synthetic private detail')))[1]),patch('sys.stdout',output):
            result=w.main([])
        self.assertNotEqual(result,0);self.assertNotIn('closed',output.getvalue());self.assertNotIn('private detail',output.getvalue())

class USBTrustTests(unittest.TestCase):
    def test_explicit_new_trust_uses_shared_tls_without_durable_registry(self):
        import numeric_pairing_worker as n
        import remembered_pairing_worker as w
        from types import SimpleNamespace
        self.assertTrue(hasattr(w,'load_trust_profile'),'shared trust loader missing')
        args=SimpleNamespace(persistent_trust=True,audio_mode='transport_test',cert=str(n.ROOT/'.tools/persistent-companion/server.pem'),key=str(n.ROOT/'.tools/persistent-companion/server-key.pem'),cue=str(n.ROOT/'.tools/native-call-preparation/transport-speaker-cue.wav'))
        context=object();transport=object()
        with patch.object(w,'load_trust_profile',return_value=(context,{'ca_sha256':CA,'remember_until_unix':2000},Path('/synthetic'))),patch.object(w,'RememberedRegistry',side_effect=AssertionError('USB must not create durable registry')),patch.object(n.subprocess,'check_output',return_value=n.HOST),patch.object(n,'_audio_transport',return_value=transport),patch.object(n.time,'time',return_value=1000):
            runtime=n._runtime(args)
        self.assertIs(runtime.context,context);self.assertIs(runtime.transport,transport);self.assertIsNone(runtime.registry._store)
    def test_new_trust_requires_full_transient_expiry_margin(self):
        import numeric_pairing_worker as n
        import remembered_pairing_worker as w
        from types import SimpleNamespace
        args=SimpleNamespace(persistent_trust=True,audio_mode='transport_test',cert=str(n.ROOT/'.tools/persistent-companion/server.pem'),key=str(n.ROOT/'.tools/persistent-companion/server-key.pem'),cue=str(n.ROOT/'.tools/native-call-preparation/transport-speaker-cue.wav'))
        with patch.object(w,'load_trust_profile',return_value=(object(),{'remember_until_unix':2000},Path('/synthetic'))),patch.object(n.time,'time',return_value=1870),patch.object(n.subprocess,'check_output',return_value=n.HOST),patch('home_lab.devices.DeviceRegistry',side_effect=AssertionError('no registry beyond trust budget')):
            with self.assertRaises(n.PairingError) as failure:n._runtime(args)
        self.assertEqual(str(failure.exception),'certificate_expired')
    def test_default_old_trust_does_not_load_new_profile(self):
        import numeric_pairing_worker as n
        import remembered_pairing_worker as w
        from types import SimpleNamespace
        args=SimpleNamespace(audio_mode='transport_test',cert=str(n.ROOT/'.tools/local-companion-tls/server.pem'),key=str(n.ROOT/'.tools/local-companion-tls/server-key.pem'),cue=str(n.ROOT/'.tools/native-call-preparation/transport-speaker-cue.wav'))
        with patch.object(w,'load_trust_profile',side_effect=AssertionError('default must preserve legacy')),patch.object(n.time,'time',return_value=n.CERT_END+1):
            with self.assertRaises(n.PairingError) as failure:n._runtime(args)
        self.assertEqual(str(failure.exception),'certificate_expired')

class ServerCertificatePinTests(unittest.TestCase):
    def check_profile(self, changed):
        import hashlib
        from contextlib import ExitStack
        from datetime import datetime, timezone
        from unittest.mock import MagicMock
        import remembered_pairing_worker as w
        with tempfile.TemporaryDirectory() as directory,ExitStack() as patches:
            base=Path(directory);base.chmod(0o700)
            ca=base/'ca.pem';cert=base/'server.pem';key=base/'server-key.pem'
            ca.write_bytes(b'synthetic CA');cert.write_bytes(b'synthetic certificate');key.write_bytes(b'not a key');key.chmod(0o600)
            digest=hashlib.sha256(cert.read_bytes()).hexdigest()
            profile=dict(exact_device_id='stackchan-001122334455',host='192.168.42.10',port=18443,
                pair_path='/companion/pair',audio_path='/companion/audio',ca_path=str(ca),
                ca_sha256=hashlib.sha256(ca.read_bytes()).hexdigest(),cert_path=str(cert),key_path=str(key),
                remember_until_unix=2000,state_directory=str(base/'state'),transport_scopes=['usb-tls-pcm-v1','wifi-wss'],
                version=1,begin=1000,end=2000,not_after_utc=datetime.fromtimestamp(2000,timezone.utc).isoformat(),
                approval_sha256='a'*64,ca_private_key_serialized=False,profile_activated=False,automatic_renewal=False)
            path=base/'profile.json';path.write_text(json.dumps(profile))
            pins=dict(DEVICE=profile['exact_device_id'],HOST=profile['host'],PORT=18443,
                URI='wss://192.168.42.10:18443/companion/audio',CA_HASH=profile['ca_sha256'],EXPIRY=2000,
                CERT_HASH=digest,CERT_BEGIN=1000,CERT_END=2000,_ERROR=None)
            if changed=='bytes':cert.write_bytes(b'replacement with identical dates')
            elif changed=='begin':pins['CERT_BEGIN']=999
            elif changed=='end':pins['CERT_END']=2001
            patches.enter_context(patch.multiple(w.config,**pins))
            patches.enter_context(patch.object(w,'PROFILE',path))
            patches.enter_context(patch.object(w,'DEVICE',profile['exact_device_id']))
            patches.enter_context(patch.object(w.time,'time',return_value=1500))
            patches.enter_context(patch.object(w.ssl,'cert_time_to_seconds',side_effect=lambda value:{'begin':1000,'end':2000}[value]))
            patches.enter_context(patch.object(w.ssl._ssl,'_test_decode_cert',return_value=dict(notBefore='begin',notAfter='end',subjectAltName=(('IP Address',profile['host']),))))
            context=MagicMock();factory=patches.enter_context(patch.object(w.ssl,'SSLContext',return_value=context))
            if changed:
                with self.assertRaises(ValueError):w.load_trust_profile(path)
                factory.assert_not_called();context.load_cert_chain.assert_not_called()
            else:
                result,_,_=w.load_trust_profile(path)
                self.assertIs(result,context);context.load_cert_chain.assert_called_once_with(cert,key)

    def test_exact_certificate_reaches_tls_setup(self):self.check_profile(None)
    def test_changed_certificate_or_each_pin_fails_before_private_key_loading(self):
        for changed in ('bytes','begin','end'):
            with self.subTest(changed=changed):self.check_profile(changed)


class InitialForgetRecoveryTests(unittest.IsolatedAsyncioTestCase):
    async def test_initial_load_failure_retries_pinned_registry_on_close(self):
        import remembered_pairing_worker as w
        from types import SimpleNamespace
        loaded=[];forgot=[];closed=[]
        registry=SimpleNamespace(forget=lambda:forgot.append(True),store=SimpleNamespace(_connection=SimpleNamespace(close=lambda:closed.append(True))))
        def loader():
            loaded.append(True)
            if len(loaded)==1:raise OSError('synthetic initial profile inaccessible')
            return SimpleNamespace(registry=registry)
        worker=w.RememberedWorker(loader,emit=lambda value:None)
        with self.assertRaises(ValueError):await worker.command('forget')
        self.assertIsNone(worker.runtime);self.assertTrue(worker.revocation_pending)
        await worker.close()
        self.assertEqual(forgot,[True]);self.assertEqual(closed,[True]);self.assertFalse(worker.revocation_pending)

if __name__=='__main__':unittest.main()
