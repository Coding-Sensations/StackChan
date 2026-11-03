"""Offline read failures, live status, process custody and termination on PTYs."""
import errno
import json
import os
from pathlib import Path
import select
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch, mock_open
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import diagnostic_reader_status as status
import read_diagnostic_snapshot as helper
from diagnostic_serial import DiagnosticReadError
from test_diagnostic_snapshot import Clock,Port,frame

class LifecycleTests(unittest.TestCase):
    def capture(self,chunks):
        clock=Clock();port=Port(clock,chunks);events=[]
        self.port=port;self.events=events
        return helper.capture_snapshot('/dev/synthetic',serial_factory=lambda:port,clock=clock,event_callback=events.append)
    def test_failure_replaces_listening_and_redacts_exception(self):
        with self.assertRaises(helper.CaptureError) as caught:self.capture([OSError(errno.EIO,'synthetic-secret-do-not-save')])
        self.assertEqual(caught.exception.public,dict(code='serial_read_failed',phase='read',error_kind='os',errno=errno.EIO))
        self.assertEqual([e['state'] for e in self.events],['listening','closed'])
        self.assertTrue(self.events[-1]['reader_closed']);self.assertFalse(self.events[-1]['captured'])
        self.assertNotIn('synthetic-secret',json.dumps(self.events));self.assertEqual(self.port.closes,1)
    def test_eof_category_preserved_without_reopen(self):
        with self.assertRaises(helper.CaptureError) as caught:self.capture([DiagnosticReadError('serial_eof','read')])
        self.assertEqual(caught.exception.public['code'],'serial_eof');self.assertEqual(self.port.opens,1);self.assertEqual(self.port.closes,1)
    def test_deadline_is_distinct_from_read_failure_and_heartbeat_is_bounded(self):
        with self.assertRaises(helper.CaptureError) as caught:self.capture([])
        self.assertEqual(caught.exception.public['code'],'capture_expired');self.assertLessEqual(len(self.events),63)
        self.assertTrue(self.events[-1]['reader_closed'])
    def test_open_failure_never_reports_listening(self):
        clock=Clock();port=Port(clock,[]);events=[]
        def failed():raise OSError(errno.ENXIO,'synthetic-secret')
        port.open=failed
        with self.assertRaises(helper.CaptureError) as caught:helper.capture_snapshot('/dev/synthetic',serial_factory=lambda:port,clock=clock,event_callback=events.append)
        self.assertEqual(caught.exception.public['phase'],'open');self.assertNotIn('listening',[e['state'] for e in events]);self.assertEqual(port.closes,1)
    def test_close_failure_is_not_claimed_closed(self):
        clock=Clock();port=Port(clock,[frame()]);events=[]
        def failed():raise OSError(errno.EIO,'synthetic-secret')
        port.close=failed
        with self.assertRaises(helper.CaptureError):helper.capture_snapshot('/dev/synthetic',serial_factory=lambda:port,clock=clock,event_callback=events.append)
        self.assertFalse(events[-1]['reader_closed']);self.assertEqual(events[-1]['cleanup_failure']['phase'],'close')
    def test_five_minute_explicit_bound_and_invalid_upper_limit(self):
        clock=Clock();port=Port(clock,[frame()])
        self.assertEqual(helper.capture_snapshot('/dev/synthetic',serial_factory=lambda:port,clock=clock,timeout_seconds=300)['boot_nonce'],91)
        for seconds in (301,True,float('inf')):
            with self.assertRaises(helper.CaptureError):helper.capture_snapshot('/dev/synthetic',timeout_seconds=seconds)

class StatusTests(unittest.TestCase):
    def record(self,path,**updates):
        value=dict(capture_id='a'*32,pid=123,state='listening',observed_unix=10.,deadline_unix=100.,deadline_utc='synthetic',reader_closed=False,process_start_token='b'*64);value.update(updates);path.write_text(json.dumps(value));return value
    def test_only_fresh_matching_live_identity_is_listening(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)/'status.json';self.record(p)
            self.assertTrue(status.check_status(p,'a'*32,now=11,process_probe=lambda _: 'b'*64)['listening'])
            for changes in ({'observed_unix':1.},{'deadline_unix':10.},{'state':'failed'},{'state':'closed'},{'reader_closed':True}):
                self.record(p,**changes);self.assertFalse(status.check_status(p,'a'*32,now=11,process_probe=lambda _:'b'*64)['listening'])
            self.record(p)
            self.assertFalse(status.check_status(p,'c'*32,now=11,process_probe=lambda _:'b'*64)['listening'])
            self.assertFalse(status.check_status(p,'a'*32,now=11,process_probe=lambda _:None)['listening'])
            self.assertFalse(status.check_status(p,'a'*32,now=11,process_probe=lambda _:'c'*64)['listening'])
    def test_terminal_status_replaces_ready_atomically_and_refuses_old_file(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)/'status.json';writer=status.ReaderStatus(p)
            writer.emit(dict(state='listening',remaining_seconds=30,reader_closed=False));self.assertTrue(status.check_status(p,writer.capture_id)['listening'])
            writer.emit(dict(state='failed',reader_closed=True,failure=dict(code='serial_eof',phase='read',error_kind='diagnostic_serial',errno=None)))
            self.assertFalse(status.check_status(p,writer.capture_id)['listening']);self.assertEqual(json.loads(p.read_text())['state'],'failed')
            self.assertEqual(status.watch_status(p,writer.capture_id,.1),1)
            with self.assertRaises(FileExistsError):status.ReaderStatus(p)
    def test_status_read_is_bounded_before_parsing(self):
        reader=mock_open(read_data=b'x'*16385)
        with patch.object(Path,'open',reader):
            self.assertFalse(status.check_status('/synthetic')['listening'])
        reader().read.assert_called_once_with(16385)
    def test_dead_starting_and_closed_processes_end_watcher_promptly(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)/'status.json'
            for state_name in ('starting','closed'):
                self.record(p,state=state_name)
                result=status.check_status(p,process_probe=lambda _:None)
                self.assertEqual(result['reason'],'process_absent_or_changed')
                with patch.object(status,'check_status',return_value=result), patch.object(status.time,'sleep') as sleep:
                    self.assertEqual(status.watch_status(p,seconds=300),1)
                    sleep.assert_not_called()
    def test_invalid_or_missing_status_never_listening(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)/'status.json'
            self.assertFalse(status.check_status(p)['listening'])
            for text in ('{}','bad','[]','x'*16385):p.write_text(text);self.assertFalse(status.check_status(p)['listening'])

class RunnerProcessTests(unittest.TestCase):
    def setUp(self):
        self.master,self.slave=os.openpty();self.path=os.ttyname(self.slave);self.directory=tempfile.TemporaryDirectory();self.root=Path(self.directory.name);self.process=None
    def tearDown(self):
        if self.process and self.process.poll() is None:self.process.kill();self.process.communicate(timeout=3)
        os.close(self.master);os.close(self.slave);self.directory.cleanup()
    def start(self,seconds=3):
        self.status=self.root/'status.json';self.output=self.root/'snapshot.json'
        self.process=subprocess.Popen([sys.executable,'-B',str(Path(helper.__file__)),'--port',self.path,'--output',str(self.output),'--status-file',str(self.status),'--timeout-seconds',str(seconds)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1'})
        end=time.monotonic()+3
        while time.monotonic()<end:
            if self.status.exists():
                try:
                    value=json.loads(self.status.read_text())
                    if value.get('state')=='listening':self.identity=value['capture_id'];return value
                except ValueError:pass
            if self.process.poll() is not None:break
            time.sleep(.01)
        self.fail('runner did not reach listening')
    def test_complete_frame_eof_loss_repair_integrates_with_success_exit(self):
        self.start();os.write(self.master,frame(changes={0:777}))
        stdout,stderr=self.process.communicate(timeout=3)
        self.assertEqual(self.process.returncode,0,stderr);self.assertEqual(json.loads(self.output.read_text())['boot_nonce'],777)
        value=json.loads(self.status.read_text());self.assertEqual(value['state'],'completed');self.assertTrue(value['reader_closed']);self.assertFalse(status.check_status(self.status,self.identity)['listening'])
    def test_sigterm_reports_failed_closure_and_nonzero_exit(self):
        self.start();self.process.terminate();stdout,stderr=self.process.communicate(timeout=3)
        self.assertEqual(self.process.returncode,1,stderr);value=json.loads(self.status.read_text());self.assertEqual(value['state'],'failed');self.assertTrue(value['reader_closed']);self.assertEqual(value['failure']['code'],'capture_cancelled');self.assertFalse(self.output.exists())
    def test_dead_process_stale_ready_is_never_claimed_listening(self):
        self.start();self.process.kill();self.process.communicate(timeout=3)
        value=status.check_status(self.status,self.identity)
        self.assertFalse(value['listening']);self.assertEqual(value['reason'],'process_absent_or_changed')
    def test_expiry_returns_nonzero_and_terminal_status(self):
        self.start(.5);stdout,stderr=self.process.communicate(timeout=3)
        self.assertEqual(self.process.returncode,1,stderr);value=json.loads(self.status.read_text());self.assertEqual(value['state'],'failed');self.assertEqual(value['failure']['code'],'capture_expired');self.assertTrue(value['reader_closed'])
if __name__=='__main__':unittest.main()
