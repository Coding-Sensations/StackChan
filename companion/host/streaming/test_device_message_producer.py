import asyncio
from dataclasses import replace
from pathlib import Path
import tempfile
import unittest

ROOT=Path(__file__).resolve().parent

from device_message_producer import Scope, Envelope, Receipt, Producer, UnavailableAuthority

class Source:
    def __init__(self): self.valid=True; self.hook=lambda:None; self.port=None; self.scope=None; self.captured=set()
    def current(self,scope,action,port):
        self.hook()
        if action=='cleanup':return port is self.port and scope in self.captured
        valid=self.valid and port is self.port and scope==self.scope
        if valid:self.captured.add(scope)
        return valid

class Port:
    def __init__(self): self.calls=[]; self.reply=None; self.hook=lambda:None
    async def submit(self,envelope,text):
        self.calls.append((envelope,text));self.hook()
        return self.reply or Receipt(envelope,'receiver_acknowledged')
    async def cancel(self,envelope): return Receipt(envelope,'cancelled')

class Contract(unittest.IsolatedAsyncioTestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.path=Path(self.tmp.name)/'ledger.sqlite'
        self.now=10.;self.scope=Scope('user-1','device-1','api-session-1','device-principal-1','hermes','binding-1','generation-1','native-incarnation-1','op-1',70.)
        self.port=Port();self.source=Source();self.source.scope=self.scope;self.source.port=self.port
        self.p=Producer(self.path,self.source,self.port,lambda:self.now,wait_limit=.02)
    async def asyncTearDown(self):
        await self.p.close();self.tmp.cleanup()
    async def test_exact_envelope_checked_receipt_and_metadata_only_journal(self):
        result=await self.p.submit(self.scope,'unique private text こんにちは')
        self.assertEqual(result.state,'receiver_acknowledged');self.assertTrue(result.receipt_checked);self.assertFalse(result.native_attested)
        envelope,text=self.port.calls[0]
        self.assertEqual((envelope.user,envelope.device,envelope.session,envelope.receiver,envelope.provider,envelope.binding,envelope.generation,envelope.native_incarnation),('user-1','device-1','api-session-1','device-principal-1','hermes','binding-1','generation-1','native-incarnation-1'))
        self.assertEqual(len(envelope.message_sha256),64);self.assertEqual(len(envelope.envelope_sha256),64)
        self.assertEqual(self.p.receipt(self.scope),result)
        self.assertEqual(self.path.stat().st_mode&0o777,0o600)
        self.assertNotIn(text.encode(),self.path.read_bytes())
    async def test_wrong_principal_receiver_or_port_never_dispatches(self):
        for field in ['user','device','session','receiver','provider','binding','generation','native_incarnation']:
            result=await self.p.submit(replace(self.scope,**{field:'other'}),'text')
            self.assertEqual(result.state,'unavailable',field)
        self.source.port=object();self.assertEqual((await self.p.submit(self.scope,'text')).state,'unavailable')
        self.assertEqual(self.port.calls,[])
    async def test_replay_and_changed_payload_are_consumed_before_transport(self):
        await self.p.submit(self.scope,'first')
        for text in ['first','changed']:
            self.assertEqual((await self.p.submit(self.scope,text)).state,'replay')
        self.assertEqual(len(self.port.calls),1)
    async def test_restart_never_resends_and_does_not_mint_a_live_receipt(self):
        await self.p.submit(self.scope,'first');await self.p.close()
        self.p=Producer(self.path,self.source,self.port,lambda:self.now,wait_limit=.02)
        self.assertEqual((await self.p.submit(self.scope,'first')).state,'replay')
        result=self.p.receipt(self.scope)
        self.assertEqual(result.state,'historical_unknown');self.assertFalse(result.receipt_checked)
        self.assertEqual(len(self.port.calls),1)
    async def test_model_dict_bool_or_wrong_envelope_never_receipt(self):
        for value in [True,{'authenticated':True},Receipt(Envelope.from_scope(replace(self.scope,receiver='other'),'text'),'receiver_acknowledged')]:
            self.port.reply=value
            self.assertEqual((await self.p.submit(self.scope,'text')).state,'unknown')
            # A separately authorized operation is needed for each test case.
            self.scope=replace(self.scope,operation=self.scope.operation+'x',native_incarnation=self.scope.native_incarnation+'x');self.source.scope=self.scope
    async def test_utf8_limits_nul_empty_and_wrong_types_never_dispatch(self):
        for text in ['', '\x00', '\ud800', '界'*342, {}, True]:
            self.assertEqual((await self.p.submit(self.scope,text)).state,'invalid')
        self.assertEqual(self.port.calls,[])
    async def test_default_unavailable_source_never_dispatches(self):
        self.p.source=UnavailableAuthority();self.assertEqual((await self.p.submit(self.scope,'text')).state,'unavailable');self.assertEqual(self.port.calls,[])


class Lifetime(unittest.IsolatedAsyncioTestCase):
    setUp=Contract.setUp
    asyncTearDown=Contract.asyncTearDown
    # These behavioral regressions catch missing post-call authority checks,
    # cancellation that abandons pending work, and restart retry of uncertain writes.
    async def test_expiry_rollback_revoke_and_grant_exception_prevent_dispatch(self):
        for now in [70.,9.,float('nan')]:
            self.now=now
            self.assertEqual((await self.p.submit(self.scope,'text')).state,'unavailable')
        self.now=10.;self.source.valid=False
        self.assertEqual((await self.p.submit(self.scope,'text')).state,'unavailable')
        self.source.valid=True
        def fault():raise RuntimeError('private exception details')
        self.source.hook=fault
        self.assertEqual((await self.p.submit(self.scope,'text')).state,'unavailable')
        self.assertEqual(self.port.calls,[])
    async def test_post_dispatch_revoke_never_publishes_receipt_or_allows_replacement(self):
        self.port.hook=lambda:setattr(self.source,'valid',False)
        self.assertEqual((await self.p.submit(self.scope,'text')).state,'unknown')
        self.source.valid=True;self.scope=replace(self.scope,operation='new-op');self.source.scope=self.scope
        self.assertEqual((await self.p.submit(self.scope,'changed')).state,'unresolved')
        self.assertEqual(len(self.port.calls),1)
    async def test_pending_late_receipt_is_retained_and_cannot_become_success(self):
        entered=asyncio.Event();release=asyncio.Event()
        async def stubborn(envelope,text):
            self.port.calls.append((envelope,text));entered.set()
            try:await release.wait()
            except asyncio.CancelledError:await release.wait()
            return Receipt(envelope,'receiver_acknowledged')
        self.port.submit=stubborn
        result=await self.p.submit(self.scope,'secret retained until join')
        self.assertEqual(result.state,'unknown');self.assertTrue(entered.is_set())
        self.assertFalse(await self.p.close())
        release.set();await asyncio.sleep(0);await asyncio.sleep(0)
        self.assertTrue(await self.p.close())
        self.p=Producer(self.path,self.source,self.port,lambda:self.now,wait_limit=.02)
        self.assertEqual(self.p.receipt(self.scope).state,'historical_unknown')
        self.assertEqual((await self.p.submit(self.scope,'secret retained until join')).state,'replay')
    async def test_cancel_only_checked_original_transport_receipt_and_join(self):
        await self.p.submit(self.scope,'text')
        self.assertEqual((await self.p.cancel(replace(self.scope,receiver='desktop-principal'))).state,'unavailable')
        self.port.cancel=lambda envelope:asyncio.sleep(0,result={'authenticated':True})
        self.assertEqual((await self.p.cancel(self.scope)).state,'cleanup_pending')
        self.assertFalse(await self.p.close())
        # Never retry an uncertain cleanup under a replacement task/port.
        self.assertFalse(await self.p.close())
    async def test_cancel_acknowledgement_is_not_physical_delivery(self):
        result=await self.p.submit(self.scope,'text');self.assertEqual(result.state,'receiver_acknowledged')
        result=await self.p.cancel(self.scope)
        self.assertEqual(result.state,'cancelled');self.assertTrue(result.receipt_checked);self.assertFalse(result.native_attested)
        self.assertEqual(self.p.receipt(self.scope).state,'cancelled')
        self.assertEqual((await self.p.submit(self.scope,'text')).state,'replay')
    async def test_cleanup_wait_is_bounded_and_retains_cancel_resistant_task(self):
        release=asyncio.Event();entered=asyncio.Event()
        await self.p.submit(self.scope,'text')
        async def stubborn(envelope):
            entered.set()
            try:await release.wait()
            except asyncio.CancelledError:await release.wait()
            return Receipt(envelope,'cancelled')
        self.port.cancel=stubborn
        before=asyncio.get_running_loop().time();result=await self.p.cancel(self.scope)
        self.assertLess(asyncio.get_running_loop().time()-before,.3)
        self.assertEqual(result.state,'cleanup_pending');self.assertTrue(entered.is_set())
        self.assertFalse(await self.p.close())
        release.set();await asyncio.sleep(0);await asyncio.sleep(0)
        self.assertTrue(await self.p.close())
    async def test_reentrant_receipt_is_inert_and_cannot_mint_authority(self):
        observed=[]
        self.source.hook=lambda:observed.append(self.p.receipt(self.scope).state)
        self.assertEqual((await self.p.submit(self.scope,'text')).state,'receiver_acknowledged')
        self.assertTrue(observed);self.assertEqual(set(observed),{'unavailable'})
    async def test_capacity_retains_replay_barrier_instead_of_evicting(self):
        for index in range(64):
            self.scope=replace(self.scope,operation=f'op-{index}');self.source.scope=self.scope
            self.assertEqual((await self.p.submit(self.scope,'text')).state,'receiver_acknowledged')
        self.scope=replace(self.scope,operation='op-65');self.source.scope=self.scope
        self.assertEqual((await self.p.submit(self.scope,'text')).state,'capacity')
        self.assertEqual(len(self.port.calls),64)
        self.scope=replace(self.scope,operation='op-0');self.source.scope=self.scope
        self.assertEqual((await self.p.submit(self.scope,'changed')).state,'replay')
    async def test_crash_after_durable_consume_never_replays_under_restart(self):
        import subprocess,sys
        await self.p.close()
        script='''
import asyncio,os,socket
def denied(*a,**kw):raise AssertionError("child network forbidden")
socket.create_connection=denied;socket.socket.connect=denied;socket.socket.connect_ex=denied;socket.socket.bind=denied;socket.socket.listen=denied
from device_message_producer import *
scope=Scope('user-1','device-1','api-session-1','device-principal-1','hermes','binding-1','generation-1','native-incarnation-1','op-1',70.)
class Source:
 def current(self,scope,action,port):return True
class Port:
 async def submit(self,envelope,text):os._exit(17)
p=Producer(JOURNAL,Source(),Port(),lambda:10.)
asyncio.run(p.submit(scope,'private crash message'))
'''.replace('JOURNAL',repr(str(self.path)))
        run=subprocess.run([sys.executable,'-c',script],cwd=ROOT,env={**__import__('os').environ,'PYTHONDONTWRITEBYTECODE':'1'},capture_output=True)
        self.assertEqual(run.returncode,17,run.stderr)
        self.p=Producer(self.path,self.source,self.port,lambda:self.now,wait_limit=.02)
        self.assertEqual((await self.p.submit(self.scope,'private crash message')).state,'replay')
        self.scope=replace(self.scope,operation='replacement-op');self.source.scope=self.scope
        self.assertEqual((await self.p.submit(self.scope,'private crash message')).state,'unresolved')
        self.assertEqual(self.port.calls,[]);self.assertNotIn(b'private crash message',self.path.read_bytes())

class FinalBoundaries(unittest.IsolatedAsyncioTestCase):
    setUp=Contract.setUp
    asyncTearDown=Contract.asyncTearDown
    async def test_expiry_during_durable_receipt_record_is_not_success(self):
        original=self.p._journal.record
        def slow(envelope,state):
            original(envelope,state)
            if state=='receiver_acknowledged':self.now=70.
        self.p._journal.record=slow
        result=await self.p.submit(self.scope,'text')
        self.assertEqual(result.state,'unknown');self.assertFalse(result.receipt_checked)
    async def test_journal_consumption_failure_never_dispatches(self):
        self.p._journal.db.close()
        self.assertEqual((await self.p.submit(self.scope,'text')).state,'unavailable')
        self.assertEqual(self.port.calls,[])
    async def test_short_remaining_lifetime_bounds_wait(self):
        self.scope=replace(self.scope,expires=10.005);self.source.scope=self.scope
        entered=asyncio.Event()
        async def hanging(envelope,text):
            entered.set();await asyncio.Event().wait()
        self.port.submit=hanging
        result=await self.p.submit(self.scope,'text')
        self.assertEqual(result.state,'unknown');self.assertTrue(entered.is_set())
    async def test_cleanup_authority_is_required_even_for_original_port(self):
        self.port.reply=True
        self.assertEqual((await self.p.submit(self.scope,'text')).state,'unknown')
        self.source.captured.clear()
        self.assertFalse(await self.p.close())
        self.source.captured.add(self.scope)
        self.assertTrue(await self.p.close())
    async def test_private_journal_rejects_symlink_and_shared_file_permissions(self):
        import os
        for mode in ['symlink','shared']:
            path=Path(self.tmp.name)/mode
            if mode=='symlink':path.symlink_to(self.path)
            else:path.touch(mode=0o644)
            with self.assertRaises((ValueError,OSError)):
                Producer(path,self.source,self.port,lambda:10.)
    async def test_producer_with_no_source_defaults_to_unavailable(self):
        p=Producer(Path(self.tmp.name)/'default.sqlite',clock=lambda:10.)
        self.assertEqual((await p.submit(self.scope,'text')).state,'unavailable')
        self.assertTrue(await p.close())

class CancellationRace(unittest.IsolatedAsyncioTestCase):
    setUp=Contract.setUp
    asyncTearDown=Contract.asyncTearDown
    async def test_explicit_cancel_during_resistant_send_never_accepts_late_receipt(self):
        entered=asyncio.Event();release=asyncio.Event()
        async def stubborn(envelope,text):
            entered.set()
            try:await release.wait()
            except asyncio.CancelledError:await release.wait()
            return Receipt(envelope,'receiver_acknowledged')
        self.port.submit=stubborn
        pending=asyncio.create_task(self.p.submit(self.scope,'text'))
        await entered.wait()
        result=await self.p.cancel(self.scope)
        self.assertEqual(result.state,'cleanup_pending')
        release.set();result=await pending
        self.assertNotEqual(result.state,'receiver_acknowledged');self.assertFalse(result.receipt_checked)
        result=await self.p.cancel(self.scope)
        self.assertEqual(result.state,'cancelled')
    async def test_cleanup_revoke_before_ack_retains_unknown(self):
        await self.p.submit(self.scope,'text')
        async def cancelling(envelope):
            self.source.captured.clear()
            return Receipt(envelope,'cancelled')
        self.port.cancel=cancelling
        self.assertEqual((await self.p.cancel(self.scope)).state,'cleanup_pending')
        self.assertFalse(await self.p.close())
    async def test_wrong_expiry_envelope_or_receipt_disposition_cannot_ack(self):
        for receipt in [Receipt(Envelope.from_scope(replace(self.scope,expires=71.),'text'),'receiver_acknowledged'),Receipt(Envelope.from_scope(self.scope,'text'),'displayed')]:
            self.port.reply=receipt
            self.assertEqual((await self.p.submit(self.scope,'text')).state,'unknown')
            self.scope=replace(self.scope,operation=self.scope.operation+'x',native_incarnation=self.scope.native_incarnation+'x');self.source.scope=self.scope

class CloseBudget(unittest.IsolatedAsyncioTestCase):
    setUp=Contract.setUp
    asyncTearDown=Contract.asyncTearDown
    async def test_close_has_one_total_budget_across_retained_operations(self):
        await self.p.close()
        self.p=Producer(self.path,self.source,self.port,lambda:self.now,wait_limit=.05)
        release=asyncio.Event()
        async def invalid(envelope,text):return True
        async def slow_cancel(envelope):
            try:await release.wait()
            except asyncio.CancelledError:await release.wait()
            return Receipt(envelope,'cancelled')
        self.port.submit=invalid;self.port.cancel=slow_cancel
        for index in range(3):
            self.scope=replace(self.scope,operation=f'pending-{index}',native_incarnation=f'native-{index}');self.source.scope=self.scope
            self.assertEqual((await self.p.submit(self.scope,'text')).state,'unknown')
        start=asyncio.get_running_loop().time()
        try:
            self.assertFalse(await self.p.close())
            self.assertLess(asyncio.get_running_loop().time()-start,.11)
        finally:
            release.set();await asyncio.sleep(0);await asyncio.sleep(0)
        self.assertTrue(await self.p.close())

if __name__=='__main__':unittest.main()
