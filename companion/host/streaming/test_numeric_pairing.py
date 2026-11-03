"""Offline numeric pairing checks; all serial/socket/issuer boundaries are synthetic.

From the repository root, using an existing Python runtime:
  python -m unittest discover -s companion/host/streaming -p 'test_numeric_pairing*.py'
No listener, robot connection, or live credential issuance is performed.
"""
import asyncio
import json
import struct
import unittest
from types import SimpleNamespace
from unittest.mock import patch
try:
    import numeric_pairing as n
except ImportError:
    n=None
REQ='00112233445566778899aabbccddeeff';DN='ab'*16;HN='22'*16
class Clock:
    def __init__(self):self.now=0.
    def __call__(self):return self.now
class CoreTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(n,'numeric backend missing')
        self.clock=Clock();self.issued=[];self.revoked=[];self.events=[]
        def issue(ttl):self.issued.append(ttl);return 'synthetic-not-a-credential-00000000'
        self.p=n.NumericPairing(clock=self.clock,issuer=issue,revoke=lambda:self.revoked.append(True),emit=self.events.append,nonce_factory=lambda:HN,code_factory=lambda:'123456')
        self.p.prepare(REQ,91)
    def hello(self,**changes):
        x=dict(type='pair.hello',version=1,request_id=REQ,device=n.DEVICE,device_nonce=DN,boot_nonce=91,capabilities=['microphone','speaker']);x.update(changes);return self.p.hello(x)
    def confirm(self,**changes):
        x=dict(type='pair.confirm',version=1,request_id=REQ,device_nonce=DN,host_nonce=HN,physical=True);x.update(changes);return self.p.robot_confirm(x)
    def stage(self,**changes):
        x=dict(type='pair.staged',version=1,request_id=REQ,device_nonce=DN,host_nonce=HN,staged=True);x.update(changes);return self.p.staged(x)
    def test_sc_np_exact_wire(self):self.assertEqual(self.p.numeric_frame(),struct.pack('>4sB16sI',b'SCNP',1,bytes.fromhex(REQ),91))
    def test_grant_requires_both_confirmations_and_staged_ack(self):
        challenge=self.hello();self.assertEqual(challenge['code'],'123456');self.assertEqual(challenge['expires_ms'],60000)
        self.p.mac_confirm();self.assertEqual(self.issued,[]);self.confirm()
        grant=self.p.take_grant();self.assertEqual(len(self.issued),1);self.assertEqual(grant['ttl_seconds'],120);self.assertEqual(grant['uri'],n.URI)
        self.assertIsNone(self.p.take_grant());self.assertFalse(self.p.audio_allowed);self.stage();self.assertTrue(self.p.audio_allowed)
        self.assertFalse(any('credential' in event or 'synthetic-not-a-credential-00000000' in json.dumps(event) for event in self.events))
    def test_robot_first_and_duplicate_confirms_issue_once(self):
        self.hello();self.confirm();self.confirm();self.assertEqual(self.issued,[]);self.p.mac_confirm();self.p.mac_confirm();self.p.take_grant();self.assertEqual(len(self.issued),1)
    def test_no_challenge_local_confirm_rejected(self):
        with self.assertRaises(n.PairingError):self.p.mac_confirm()
    def test_wrong_reset_boot_rejected(self):
        with self.assertRaises(n.PairingError):self.hello(boot_nonce=92)
        self.assertEqual(self.issued,[])
    def test_stale_context_rejected(self):
        for field in ('request_id','device_nonce','host_nonce'):
            self.setUp();self.hello()
            with self.assertRaises(n.PairingError):self.confirm(**{field:'ff'*16})
            self.assertEqual(self.issued,[])
    def test_types_and_unknown_fields_rejected(self):
        for changes in ({'version':True},{'device_nonce':DN.upper()},{'capabilities':['speaker','microphone']},{'extra':'x'},{'boot_nonce':True}):
            self.setUp()
            with self.assertRaises(n.PairingError):self.hello(**changes)
    def test_duplicate_hello_rejected_and_code_immutable(self):
        self.hello()
        with self.assertRaises(n.PairingError):self.hello()
        self.assertEqual(self.p.code,'123456')
    def test_json_duplicate_and_bounds_rejected(self):
        for text in ('{"type":"x","type":"y"}','x'*2049,'[]','{"x":NaN}'):
            with self.assertRaises(n.PairingError):n.strict_json(text)
    def test_no_physical_confirmation_rejected(self):
        self.hello()
        for value in (False,1,'true'):
            with self.assertRaises(n.PairingError):self.confirm(physical=value)
    def test_challenge_expiry(self):
        self.hello();self.clock.now=60
        with self.assertRaises(n.PairingError):self.p.mac_confirm()
        self.assertEqual(self.p.state,'expired');self.assertEqual(self.issued,[])
    def test_grant_ttl_remaining_and_missing_ack(self):
        self.hello();self.clock.now=59;self.confirm();self.p.mac_confirm();grant=self.p.take_grant();self.assertEqual(grant['ttl_seconds'],61)
        self.clock.now=64
        with self.assertRaises(n.PairingError):self.p.tick()
        self.assertEqual(self.revoked,[True]);self.assertFalse(self.p.audio_allowed)
    def test_cancel_revokes_and_late_confirm_cannot_issue(self):
        self.hello();self.confirm();self.p.mac_confirm();self.p.cancel()
        self.assertEqual(self.revoked,[True]);self.assertFalse(self.p.audio_allowed)
        with self.assertRaises(n.PairingError):self.stage()
        self.p.cancel();self.assertEqual(self.revoked,[True])
    def test_success_expires_at_listener_deadline(self):
        self.hello();self.confirm();self.p.mac_confirm();self.p.take_grant();self.stage();self.clock.now=120
        with self.assertRaises(n.PairingError):self.p.tick()
        self.assertFalse(self.p.audio_allowed);self.assertEqual(self.revoked,[True])
    def test_ready_anchored_budget_excludes_runtime_loading_and_expires(self):
        self.p.state='idle';self.clock.now=105
        self.p.prepare(REQ,91,started_at=100,deadline_budget_seconds=118)
        self.assertEqual(self.p.deadline,218)
        self.hello();self.confirm();self.p.mac_confirm();grant=self.p.take_grant();self.stage()
        self.assertEqual(grant['ttl_seconds'],113)
        self.clock.now=217.999;self.p.tick();self.assertTrue(self.p.audio_allowed)
        self.clock.now=218
        with self.assertRaises(n.PairingError):self.p.tick()
        self.assertEqual(self.p.state,'expired');self.assertEqual(self.revoked,[True]);self.assertFalse(self.p.audio_allowed)
    def test_expired_ready_budget_never_prepares_or_writes(self):
        self.p.state='idle';self.clock.now=200
        with self.assertRaises(n.PairingError):self.p.prepare(REQ,91,started_at=0,deadline_budget_seconds=118)
        self.assertEqual(self.p.state,'idle')
        with self.assertRaises(n.PairingError):self.p.numeric_frame()
        self.assertEqual(self.issued,[])
    def test_request_write_rechecks_deadline_after_listener_startup(self):
        self.clock.now=120
        with self.assertRaises(n.PairingError):self.p.numeric_frame()
        self.assertEqual(self.p.state,'expired');self.assertEqual(self.issued,[])
    def test_invalid_ready_deadline_never_prepares(self):
        for start,budget in [(True,118),(float('nan'),118),(float('inf'),118),(-1,118),(1,118),(0,True),(0,0),(0,121)]:
            self.setUp();self.p.state='idle'
            with self.assertRaises(n.PairingError):self.p.prepare(REQ,91,started_at=start,deadline_budget_seconds=budget)
            self.assertEqual(self.p.state,'idle');self.assertEqual(self.issued,[])
    def test_zero_uuid_staged_does_not_match(self):
        self.hello();self.confirm();self.p.mac_confirm();self.p.take_grant()
        with self.assertRaises(n.PairingError):self.stage(request_id='00'*16)
    def test_ipc_command_strict(self):
        for line in ('{"command":"CONFIRM"}','{"command":"prepare","token":"x"}','{"command":"x"}'):
            with self.assertRaises(n.PairingError):n.parse_command(line)
        self.assertEqual(n.parse_command('{"command":"confirm"}'),'confirm')

    def test_issuer_failure_still_revokes_without_echo(self):
        self.hello();self.confirm()
        def fail(ttl):raise RuntimeError('synthetic secret')
        self.p.issuer=fail
        with self.assertRaises(n.PairingError):self.p.mac_confirm()
        self.assertEqual(self.revoked,[True]);self.assertFalse(self.p.audio_allowed)
    def test_zero_fresh_context_rejected(self):
        with self.assertRaises(n.PairingError):self.hello(device_nonce='00'*16)
        self.setUp();self.p.nonce_factory=lambda:'00'*16
        with self.assertRaises(n.PairingError):self.hello()
        self.setUp();self.p.state='idle'
        with self.assertRaises(n.PairingError):self.p.prepare('00'*16,91)
    def test_duplicate_hello_never_rerolls_number(self):
        calls=[];self.p.code_factory=lambda:(calls.append(True) or '123456');self.hello()
        with self.assertRaises(n.PairingError):self.hello()
        self.assertEqual(calls,[True]);self.assertEqual(self.p.code,'123456')
    def test_malformed_issuer_result_binds_failure(self):
        self.hello();self.confirm();self.p.issuer=lambda ttl:'bad credential'
        with self.assertRaises(n.PairingError):self.p.mac_confirm()
        self.assertEqual(self.p.state,'failed');self.assertEqual(self.revoked,[True])
    def test_revocation_failure_never_leaves_audio_allowed(self):
        self.hello();self.confirm();self.p.mac_confirm();self.p.take_grant();self.stage()
        def fail():raise RuntimeError('synthetic secret')
        self.p.revoke=fail
        with self.assertRaises(n.PairingError):self.p.cancel()
        self.assertTrue(self.p.revocation_failed);self.assertFalse(self.p.audio_allowed);self.assertEqual(self.p.state,'failed')
    def test_short_issuer_result_revoked_without_grant(self):
        for token in ('x','x'*31):
            self.setUp();self.hello();self.confirm();self.p.issuer=lambda ttl:token
            with self.assertRaises(n.PairingError):self.p.mac_confirm()
            self.assertEqual(self.revoked,[True]);self.assertEqual(self.p.state,'failed')
            self.assertIsNone(self.p.take_grant());self.assertFalse(self.p.audio_allowed)
class AudioScopeTests(unittest.TestCase):
    def pair(self,**kwargs):
        self.clock=Clock();self.revoked=[];self.issued=[]
        def issue(ttl):self.issued.append(ttl);return 'synthetic-not-a-credential-00000000'
        core=n.NumericPairing(issuer=issue,revoke=lambda:self.revoked.append(True),clock=self.clock,nonce_factory=lambda:HN,code_factory=lambda:'123456',**kwargs)
        core.prepare(REQ,91)
        challenge=core.hello(dict(type='pair.hello',version=1,request_id=REQ,device=n.DEVICE,device_nonce=DN,boot_nonce=91,capabilities=['microphone','speaker']))
        return core,challenge
    def confirm(self,core):
        core.mac_confirm();self.assertEqual(self.issued,[])
        core.robot_confirm(dict(type='pair.confirm',version=1,request_id=REQ,device_nonce=DN,host_nonce=HN,physical=True))
        return core.take_grant()
    def stage(self,core):core.staged(dict(type='pair.staged',version=1,request_id=REQ,device_nonce=DN,host_nonce=HN,staged=True))
    def test_default_pairing_challenge_and_grant_are_muted(self):
        core,challenge=self.pair()
        self.assertIn('audio_requested',challenge);self.assertIs(challenge['audio_requested'],False)
        grant=self.confirm(core);self.assertIs(grant['audio_requested'],False)
        self.stage(core);self.assertTrue(core.audio_allowed)
        self.clock.now=6;core.tick();self.assertEqual(core.state,'paired')
    def test_audio_scope_stays_bound_until_correlated_ack(self):
        core,challenge=self.pair(audio_requested=True)
        self.assertIs(challenge['audio_requested'],True);self.assertFalse(core.audio_allowed)
        class Transport:
            def process_request(self,*args):return 200
        class Connection:
            def respond(self,status,text):return status
        class Request:path='/companion/audio'
        gateway=n.NumericGateway(core,Transport(),audio_enabled=True)
        self.assertEqual(gateway.process_request(Connection(),Request()),403)
        with self.assertRaises(AttributeError):core.audio_requested=False
        grant=self.confirm(core);self.assertIs(grant['audio_requested'],True);self.assertFalse(core.audio_allowed)
        self.assertEqual(gateway.process_request(Connection(),Request()),403)
        with self.assertRaises(n.PairingError):core.staged(dict(type='pair.staged',version=1,request_id='ff'*16,device_nonce=DN,host_nonce=HN,staged=True))
        self.assertFalse(core.audio_allowed);self.stage(core);self.assertTrue(core.audio_allowed)
        self.assertEqual(gateway.process_request(Connection(),Request()),200)
        core.cancel();self.assertFalse(core.audio_allowed);self.assertEqual(self.revoked,[True])
    def test_audio_scope_rejects_truthy_non_boolean(self):
        for value in (None,0,1,'true',[],{}):
            with self.subTest(value=value),self.assertRaises(n.PairingError):self.pair(audio_requested=value)
    def test_muted_scope_cannot_be_escalated_by_gateway_flag(self):
        core,_=self.pair();self.confirm(core);self.stage(core)
        class Transport:
            def process_request(self,*args):raise AssertionError('muted scope entered transport')
        class Connection:
            def respond(self,status,text):return status
        class Request:path='/companion/audio'
        gateway=n.NumericGateway(core,Transport(),audio_enabled=True)
        self.assertEqual(gateway.process_request(Connection(),Request()),403)
class AsyncTests(unittest.IsolatedAsyncioTestCase):
    async def test_gateway_denies_audio_before_grant(self):
        self.assertIsNotNone(n)
        class Transport:
            def process_request(self,*a):raise AssertionError('must not enter transport')
        core=n.NumericPairing(issuer=lambda ttl:'fixture',revoke=lambda:None);core.prepare(REQ,91)
        gateway=n.NumericGateway(core,Transport())
        class Connection:
            def respond(self,status,text):return status
        class Request:path='/companion/audio'
        self.assertEqual(gateway.process_request(Connection(),Request()),403)
        core.audio_allowed=True;core.state='paired'
        self.assertEqual(gateway.process_request(Connection(),Request()),403)
        class Socket:
            request=Request()
            async def close(self):pass
        await gateway.handle(Socket())
        self.assertFalse(gateway._audio)
    async def test_audio_cleanup_fault_is_failed_not_success(self):
        pairing=SimpleNamespace(state='paired',audio_requested=True,audio_allowed=True,cancel_calls=[])
        def cancel(state,message):pairing.cancel_calls.append((state,message));pairing.state=state
        pairing.cancel=cancel
        class Transport:
            last_result={'outcome':'complete_with_cleanup_fault','native_conversation':True,
                         'conversation_complete':True,'consent_received':True,
                         'diagnostics':{'cleanup_fault_count':1}}
            async def handle(self,socket):pass
        class Request:path='/companion/audio'
        class Socket:
            request=Request()
            async def close(self):pass
        await n.NumericGateway(pairing,Transport(),audio_enabled=True).handle(Socket())
        self.assertEqual(pairing.cancel_calls,[('failed','audio_cleanup_failed')])

    async def test_partial_numeric_write_no_retry_and_close(self):
        self.assertIsNotNone(n)
        class Port:
            def __init__(self):self.writes=0
            def write(self,x):self.writes+=1;return len(x)-1
        port=Port()
        with self.assertRaises(n.PairingError):n.write_numeric_once(port,struct.pack('>4sB16sI',b'SCNP',1,bytes.fromhex(REQ),91))
        self.assertEqual(port.writes,1)
    async def test_actual_gateway_socket_flow_grant_and_disconnect_revoke(self):
        for end_state in ('failed','expired'):
            issued=[];revoked=[];clock=Clock()
            core=n.NumericPairing(clock=clock,issuer=lambda ttl:(issued.append(ttl) or 'synthetic-not-a-credential-00000000'),revoke=lambda:revoked.append(True),nonce_factory=lambda:HN,code_factory=lambda:'123456')
            core.prepare(REQ,91)
            class Socket:
                request=type('Request',(),{'path':'/companion/pair'})()
                def __init__(self):self.queue=asyncio.Queue();self.sent=[];self.closed=False
                async def recv(self):
                    result=await self.queue.get()
                    if isinstance(result,Exception):raise result
                    return result
                async def send(self,value):self.sent.append(json.loads(value))
                async def close(self):self.closed=True
            socket=Socket();gateway=n.NumericGateway(core,object())
            socket.queue.put_nowait(json.dumps(dict(type='pair.hello',version=1,request_id=REQ,device=n.DEVICE,device_nonce=DN,boot_nonce=91,capabilities=['microphone','speaker'])))
            task=asyncio.create_task(gateway.handle(socket))
            for _ in range(100):
                if socket.sent:break
                await asyncio.sleep(.001)
            self.assertEqual(socket.sent[0]['type'],'pair.challenge');self.assertFalse(issued)
            core.mac_confirm();socket.queue.put_nowait(json.dumps(dict(type='pair.confirm',version=1,request_id=REQ,device_nonce=DN,host_nonce=HN,physical=True)))
            for _ in range(100):
                if len(socket.sent)>1:break
                await asyncio.sleep(.001)
            self.assertEqual(socket.sent[1]['type'],'pair.grant');self.assertEqual(len(issued),1);self.assertFalse(core.audio_allowed)
            socket.queue.put_nowait(json.dumps(dict(type='pair.staged',version=1,request_id=REQ,device_nonce=DN,host_nonce=HN,staged=True)))
            for _ in range(100):
                if core.audio_allowed:break
                await asyncio.sleep(.001)
            self.assertTrue(core.audio_allowed)
            clock.now=119 if end_state=='failed' else 120  # True deadline wins; one-second-early loss stays failure.
            socket.queue.put_nowait(OSError('synthetic disconnect'))
            await asyncio.wait_for(task,.5)
            self.assertFalse(core.audio_allowed);self.assertEqual(revoked,[True]);self.assertTrue(socket.closed);self.assertEqual(core.state,end_state)
    async def test_expected_audio_completion_closes_and_faults_remain_failures(self):
        for outcome,complete,expected in [('complete',True,'closed'),('fault',True,'failed'),('complete',False,'failed')]:
            with self.subTest(outcome=outcome,complete=complete):
                revoked=[];clock=Clock()
                core=n.NumericPairing(clock=clock,issuer=lambda ttl:'synthetic-not-a-credential-00000000',revoke=lambda:revoked.append(True),nonce_factory=lambda:HN,code_factory=lambda:'123456',audio_requested=True)
                core.prepare(REQ,91)
                core.hello(dict(type='pair.hello',version=1,request_id=REQ,device=n.DEVICE,device_nonce=DN,boot_nonce=91,capabilities=['microphone','speaker']))
                core.mac_confirm();core.robot_confirm(dict(type='pair.confirm',version=1,request_id=REQ,device_nonce=DN,host_nonce=HN,physical=True));core.take_grant()
                core.staged(dict(type='pair.staged',version=1,request_id=REQ,device_nonce=DN,host_nonce=HN,staged=True))
                class Transport:
                    last_result=dict(outcome=outcome,consent_received=True,cue_complete=complete,mic_complete=complete)
                    calls=0
                    async def handle(self,socket):self.calls+=1
                class Socket:
                    request=type('Request',(),{'path':'/companion/audio'})()
                    async def close(self):pass
                transport=Transport();gateway=n.NumericGateway(core,transport,audio_enabled=True)
                await gateway.handle(Socket())
                self.assertEqual(core.state,expected);self.assertFalse(core.audio_allowed);self.assertEqual(revoked,[True])
                await gateway.handle(Socket());self.assertEqual(transport.calls,1);self.assertEqual(revoked,[True])
    async def test_audio_handoff_keeps_pair_socket_until_completion_or_actual_disconnect(self):
        async def until(predicate):
            async with asyncio.timeout(2):
                while not predicate():await asyncio.sleep(.001)
        for cause in ('audio_complete','pair_disconnect'):
            with self.subTest(cause=cause):
                revoked=[];clock=Clock()
                core=n.NumericPairing(clock=clock,issuer=lambda ttl:'synthetic-not-a-credential-00000000',revoke=lambda:revoked.append(True),nonce_factory=lambda:HN,code_factory=lambda:'123456',audio_requested=True)
                core.prepare(REQ,91)
                class Socket:
                    def __init__(self,path):self.request=type('Request',(),{'path':path})();self.queue=asyncio.Queue();self.sent=[];self.closed=False
                    async def recv(self):
                        value=await self.queue.get()
                        if isinstance(value,Exception):raise value
                        return value
                    async def send(self,value):self.sent.append(json.loads(value))
                    async def close(self):self.closed=True
                class Transport:
                    def __init__(self):self.started=asyncio.Event();self.release=asyncio.Event();self.last_result=dict(outcome='complete',consent_received=True,cue_complete=True,mic_complete=True)
                    async def handle(self,socket):self.started.set();await self.release.wait()
                transport=Transport();gateway=n.NumericGateway(core,transport,audio_enabled=True);pair=Socket('/companion/pair');audio=Socket('/companion/audio')
                pair.queue.put_nowait(json.dumps(dict(type='pair.hello',version=1,request_id=REQ,device=n.DEVICE,device_nonce=DN,boot_nonce=91,capabilities=['microphone','speaker'])))
                pair_task=asyncio.create_task(gateway.handle(pair));audio_task=None
                try:
                    await until(lambda:len(pair.sent)==1);core.mac_confirm()
                    pair.queue.put_nowait(json.dumps(dict(type='pair.confirm',version=1,request_id=REQ,device_nonce=DN,host_nonce=HN,physical=True)))
                    await until(lambda:len(pair.sent)==2)
                    pair.queue.put_nowait(json.dumps(dict(type='pair.staged',version=1,request_id=REQ,device_nonce=DN,host_nonce=HN,staged=True)))
                    await until(lambda:len(pair.sent)==3);self.assertEqual(pair.sent[-1]['type'],'pair.ready')
                    self.assertTrue(core.audio_allowed);self.assertFalse(pair.closed);self.assertFalse(pair_task.done());self.assertEqual(revoked,[])
                    audio_task=asyncio.create_task(gateway.handle(audio));await transport.started.wait()
                    self.assertFalse(pair.closed);self.assertFalse(pair_task.done());self.assertEqual(revoked,[])
                    if cause=='pair_disconnect':
                        pair.queue.put_nowait(RuntimeError('synthetic disconnect'))
                        await asyncio.wait_for(pair_task,2)
                        self.assertEqual(core.state,'failed');self.assertFalse(core.audio_allowed);self.assertEqual(revoked,[True])
                    transport.release.set();await asyncio.wait_for(audio_task,2);await asyncio.wait_for(pair_task,2)
                    self.assertTrue(pair.closed);self.assertFalse(core.audio_allowed);self.assertEqual(revoked,[True]);self.assertEqual(core.state,'closed' if cause=='audio_complete' else 'failed')
                finally:
                    transport.release.set();pair_task.cancel()
                    if audio_task:audio_task.cancel()
                    await asyncio.gather(pair_task,*([audio_task] if audio_task else []),return_exceptions=True)
    async def test_gateway_close_closes_sockets_when_grant_revocation_fails(self):
        pairing=SimpleNamespace(state='paired',revocation_failed=False,audio_allowed=True)
        def cancel():
            pairing.revocation_failed=True
            pairing.audio_allowed=False
            raise n.PairingError('revocation_failed')
        pairing.cancel=cancel
        class Socket:
            closed=False
            async def close(self):self.closed=True
        socket=Socket();gateway=n.NumericGateway(pairing,object());gateway.sockets.add(socket)
        await gateway.close()
        self.assertTrue(pairing.revocation_failed)
        self.assertTrue(socket.closed)

    async def test_failed_handler_close_retained_and_reported_during_revoke_failure(self):
        pairing=SimpleNamespace(state='paired',revocation_failed=False,audio_allowed=True)
        def cancel():
            pairing.revocation_failed=True
            pairing.audio_allowed=False
            raise n.PairingError('revocation_failed')
        pairing.cancel=cancel
        class Request:path='/invalid'
        class Socket:
            request=Request()
            def __init__(self):self.close_calls=0
            async def close(self):
                self.close_calls+=1
                raise RuntimeError('synthetic socket close failure')
        socket=Socket();gateway=n.NumericGateway(pairing,object())
        await gateway.handle(socket)
        self.assertIn(socket,gateway.sockets)
        with self.assertRaises(RuntimeError):await gateway.close()
        self.assertTrue(pairing.revocation_failed)
        self.assertIn(socket,gateway.sockets)
        self.assertEqual(socket.close_calls,2)

    async def test_cleanup_preserves_expired_cause(self):
        core=n.NumericPairing(issuer=lambda ttl:'fixture',revoke=lambda:None);core.prepare(REQ,91);core.cancel('expired','expired')
        gateway=n.NumericGateway(core,object());await gateway.close()
        self.assertEqual(core.state,'expired')
if __name__=='__main__':unittest.main()
