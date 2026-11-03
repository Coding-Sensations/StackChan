"""Pure model checks: no subprocess, hardware, network or native library loading."""
import json
from pathlib import Path
import signal
import tempfile
import time
import unittest
from unittest.mock import patch
import run_process_fixtures as runner

TICKET = 'AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE'

class ModelKernel:
    def __init__(self):
        self.rows = {100:dict(birth=(1, 1),parent=99,sid=100,pgid=100,status=2),
                     101:dict(birth=(1, 2),parent=100,sid=100,pgid=101,status=4)}
        self.sent = []
        self.denied = False
    def info(self,pid):
        if self.denied: raise runner.CustodyError('denied telemetry')
        return self.rows.get(pid)
    def children(self,pid): return {p for p,i in self.rows.items() if i['parent']==pid}
    def task(self,pid): return (1024, 1, 1)
    def send(self,pid,sig): self.sent.append((pid,sig))

class CustodyChecks(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);self.k=ModelKernel()
        self.m=runner.FoundationMonitor(self.root,'nonce',self.root/'active.json',self.k)
        self.request=dict(ticket=TICKET,nonce='nonce',pid=101,ppid=100,sid=100)
        (self.root/(TICKET+'.planned')).write_text(TICKET)
    def write(self): (self.root/(TICKET+'.request')).write_text(json.dumps(self.request))
    def test_different_group_same_session_and_prebody_accounting(self):
        self.write(); result=self.m.sample(100)
        self.assertEqual((result['rss'],result['threads']),(2048,2))
        self.assertEqual(self.k.sent,[(101,signal.SIGCONT)])
        saved=json.loads((self.root/'active.json').read_text())
        self.assertEqual(saved['admitted'][TICKET]['birth'],[1,2])
        self.m.verify_complete()
    def test_stale_nonce(self):
        self.request['nonce']='old';self.write()
        with self.assertRaises(runner.CustodyError):self.m.sample(100)
        self.assertFalse(self.k.sent)
    def test_unowned_pid(self):
        self.request['pid']=999;self.write()
        with self.assertRaises(runner.CustodyError):self.m.sample(100)
        self.assertFalse(self.k.sent)
    def test_wrong_parent(self):
        self.request['ppid']=99;self.write()
        with self.assertRaises(runner.CustodyError):self.m.sample(100)
    def test_session_escape_retains_cleanup_custody(self):
        self.k.rows[101]['sid']=101;self.write()
        with self.assertRaises(runner.CustodyError):self.m.sample(100)
        with patch.object(runner.guard.os, 'kill', self.k.send):
            runner.guard.signal_owned_group(self.m,100,signal.SIGKILL)
        self.assertIn((101,signal.SIGKILL),self.k.sent)
        self.assertNotIn((101,signal.SIGCONT),self.k.sent)
    def test_pid_reuse_never_signalled(self):
        self.m.owned_alive(100);self.k.rows[101]['birth']=(2,2)
        self.m.owned_alive(100)
        with self.assertRaises(runner.CustodyError):self.m.sample(100)
        self.assertFalse(self.k.sent)
    def test_parent_birth_changed(self):
        alive=self.m.owned_alive(100);self.write();self.k.rows[100]['birth']=(4,4)
        with self.assertRaises(runner.CustodyError):self.m.admit(alive)
        self.assertFalse(self.k.sent)
    def test_writer_vanished(self):
        alive=self.m.owned_alive(100);self.write();del self.k.rows[101]
        with self.assertRaises(runner.CustodyError):self.m.admit(alive)
    def test_root_exited_before_handshake(self):
        self.m.owned_alive(100);del self.k.rows[100];self.k.rows[101]['parent']=1;self.write()
        with self.assertRaises(runner.CustodyError):self.m.admit({101})
        self.assertFalse(self.k.sent)
    def test_stopped_child_survives_root_and_is_cleaned(self):
        self.write();self.m.sample(100);del self.k.rows[100];self.k.rows[101]['parent']=1
        with patch.object(runner.guard.os, 'kill', self.k.send):
            runner.guard.signal_owned_group(self.m,100,signal.SIGKILL)
        self.assertEqual(self.k.sent[-1],(101,signal.SIGKILL))
    def test_registration_replay(self):
        self.write();self.m.sample(100);self.request['ppid']=9;self.write()
        with self.assertRaises(runner.CustodyError):self.m.sample(100)
    def test_no_duplicate_cont(self):
        self.write();self.m.sample(100);self.m.sample(100)
        self.assertEqual(len(self.k.sent),1)
    def test_nonregular_or_symlink_request(self):
        request=self.root/(TICKET+'.request');request.symlink_to(self.root/(TICKET+'.planned'))
        with self.assertRaises(runner.CustodyError):self.m.sample(100)
    def test_oversized_request(self):
        (self.root/(TICKET+'.request')).write_text('x'*2049)
        with self.assertRaises(runner.CustodyError):self.m.sample(100)
    def test_fifo(self):
        import os
        path=self.root/(TICKET+'.request');os.mkfifo(path)
        with self.assertRaises(runner.CustodyError):self.m.sample(100)
    def test_denied_telemetry(self):
        self.k.denied=True
        with self.assertRaises(runner.CustodyError):self.m.sample(100)
        self.assertFalse(self.k.sent)
    def test_stop_timeout(self):
        self.write();self.k.rows[101]['status']=2;self.m.owned_alive(100);self.m.first_seen[101]=time.monotonic()-3
        with self.assertRaises(runner.CustodyError):self.m.sample(100)
        self.assertFalse(self.k.sent)
    def test_missing_registration(self):
        self.m.owned_alive(100);self.m.first_seen[101]=time.monotonic()-3
        with self.assertRaises(runner.CustodyError):self.m.sample(100)
    def test_incomplete_launch_intent(self):
        with self.assertRaises(runner.CustodyError):self.m.verify_complete()
    def test_missing_plan(self):
        (self.root/(TICKET+'.planned')).unlink();self.write()
        with self.assertRaises((runner.CustodyError,OSError)):self.m.sample(100)
    def test_unexpected_grandchild(self):
        self.k.rows[102]=dict(birth=(2,2),parent=101,sid=100,pgid=102,status=4);self.write()
        with self.assertRaises(runner.CustodyError):self.m.sample(100)
    def test_identity_changed_before_cont(self):
        self.write();alive=self.m.owned_alive(100);self.m.identity=lambda pid:(9,9)
        with self.assertRaises(runner.CustodyError):self.m.admit(alive)
        self.assertFalse(self.k.sent)

    def test_short_lived_unregistered_child(self):
        self.k.rows[102]=dict(birth=(2,2),parent=100,sid=100,pgid=102,status=2)
        self.write();self.m.sample(100);del self.k.rows[102]
        with self.assertRaises(runner.CustodyError):self.m.verify_complete()
    def test_reuse_does_not_block_other_owned_cleanup(self):
        self.k.rows[102]=dict(birth=(2,2),parent=100,sid=100,pgid=102,status=2)
        self.m.owned_alive(100);self.k.rows[101]['birth']=(5,5)
        with patch.object(runner.guard.os, 'kill', self.k.send):
            runner.guard.signal_owned_group(self.m,100,signal.SIGKILL)
        self.assertIn((102,signal.SIGKILL),self.k.sent)
        self.assertNotIn((101,signal.SIGKILL),self.k.sent)
    def test_pending_registration_fails_completion(self):
        self.write();self.m.sample(100);(self.root/'other.pending').write_text('partial')
        with self.assertRaises(runner.CustodyError):self.m.verify_complete()
    def test_two_tickets_cannot_admit_same_birth(self):
        self.write();self.m.sample(100)
        self.m.admitted['other']=self.m.admitted[TICKET]
        with self.assertRaises(runner.CustodyError):self.m.verify_complete()

if __name__=='__main__':unittest.main()
