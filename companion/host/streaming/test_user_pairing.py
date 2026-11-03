"""Offline: real codec/framing/CLI; terminal, clock and serial boundary are synthetic."""
import io
import struct
import sys
import types
import unittest
from unittest.mock import patch
import user_pairing
from provisioning import unpack_grant
try:
    import pairing_receipt as codec
except ImportError:
    codec = None
TOKEN='offline-fixture-NOT-A-CREDENTIAL'
REQUEST='00112233445566778899aabbccddeeff'
def receipt(status, payload=bytes(16)):
    return struct.pack('>4sBB16s', b'SCPS', 1, status, payload)
def ready(nonce=123, reason=1):
    return receipt(1, struct.pack('>II8s', nonce, reason, bytes(8)))
class Terminal(io.StringIO):
    def __init__(self,tty=True):super().__init__();self.tty=tty
    def isatty(self):return self.tty
class Clock:
    def __init__(self):self.now=0
    def monotonic(self):return self.now
    def sleep(self,n):self.now+=n
class Boundary:
    def __init__(self, events, mode):
        object.__setattr__(self,'events',events);object.__setattr__(self,'mode',mode)
        object.__setattr__(self,'phase','initial');object.__setattr__(self,'frames',[])
        object.__setattr__(self,'pending',b'');object.__setattr__(self,'sent_ready',False)
    def __setattr__(self,k,v):self.events.append((k,v));object.__setattr__(self,k,v)
    def open(self):
        self.events.append(('open',))
        if self.mode=='open_error':raise OSError(TOKEN)
    def read(self,n):
        self.events.append(('read',))
        if self.mode=='lost':raise OSError(TOKEN)
        if self.mode=='flood':return b'x'*n
        if self.phase=='pair' and not self.sent_ready:
            self.sent_ready=True
            self.pending= b'' if self.mode=='missing_ready' else (receipt(3) if self.mode=='reject_ready' else (ready()+ready(124) if self.mode=='compound_ready' else ready()))
        if self.pending:
            size=n if self.mode=='compound_ready' else min(3,n)
            chunk=self.pending[:size];self.pending=self.pending[size:];return chunk
        return b''
    def write(self,frame):
        self.events.append(('write',));self.frames.append(frame)
        if self.mode=='write_error':raise OSError(TOKEN)
        _,_,_,req=unpack_grant(frame)
        payload=bytes.fromhex(req)
        self.pending=self.pending+{'new_boot':ready(124),'repeat_ready':ready(),'expired':receipt(4),'rejected':receipt(3),'wrong_uuid':receipt(2,bytes(16)),'missing_staged':b''}.get(self.mode,receipt(2,payload))
        return len(frame)-1 if self.mode=='short' else len(frame)
    def close(self):self.events.append(('close',))
class CodecTests(unittest.TestCase):
    def setUp(self):self.assertIsNotNone(codec,'receipt codec missing')
    def test_ready_wire_fields(self):
        parsed=codec.decode_receipt(ready(0x12345678,64))
        self.assertEqual(parsed.status,1);self.assertEqual(parsed.boot_nonce,0x12345678);self.assertEqual(parsed.reset_reason,64)
    def test_uuid_wire_fields(self):
        self.assertEqual(codec.decode_receipt(receipt(2,bytes.fromhex(REQUEST))).request,REQUEST)
    def test_fixed_empty_status_payloads(self):
        for status in (3,4):self.assertEqual(codec.decode_receipt(receipt(status)).status,status)
    def test_malformed_frames_rejected(self):
        bad=[b'',ready()[:-1],ready()+b'x',b'XXXX'+ready()[4:],ready()[:4]+b'\x02'+ready()[5:],receipt(0),receipt(5),ready(0),ready(1,65),receipt(1,struct.pack('>II8s',1,1,b'x'*8)),receipt(3,b'x'*16),receipt(4,b'x'*16)]
        for frame in bad:
            with self.subTest(frame=frame):
                with self.assertRaises(ValueError):codec.decode_receipt(frame)
class CliTests(unittest.TestCase):
    def run_cli(self, mode='', answers=None, tty=True, paused=False, token=TOKEN, argv=None):
        events=[];port=Boundary(events,mode);clock=Clock();streams=[Terminal(tty) for _ in range(3)]
        choices=iter(answers if answers is not None else ['OPEN','STABLE','PAIR','MUTED'])
        def prompt(text):
            answer=next(choices);events.append(('confirm',answer))
            if answer.strip().casefold()=='pair':port.phase='pair'
            return answer
        def masked(text):events.append(('masked',));return token
        def factory(**kw):events.append(('construct',kw));return port
        with patch.object(sys,'stdin',streams[0]),patch.object(sys,'stdout',streams[1]),patch.object(sys,'stderr',streams[2]),patch('builtins.input',prompt),patch('getpass.getpass',masked),patch.dict(sys.modules,{'serial':types.SimpleNamespace(Serial=factory)}),patch.object(user_pairing,'_pairing_paused',return_value=paused,create=True),patch.object(user_pairing.time,'monotonic',clock.monotonic),patch.object(user_pairing.time,'sleep',clock.sleep):
            result=user_pairing.main(argv or ['--port','/dev/offline-fixture'])
        output=''.join(s.getvalue() for s in streams)
        self.assertNotIn(TOKEN,output)
        return result,events,port,output,clock.now
    def test_pause_blocks_all_input_and_io(self):
        result,events,port,out,_=self.run_cli(paused=True)
        self.assertNotEqual(result,0);self.assertEqual(events,[]);self.assertIn('paused',out.lower())
    def test_non_tty_denied(self):
        result,events,_,_,_=self.run_cli(tty=False);self.assertNotEqual(result,0);self.assertEqual(events,[])
    def test_open_default_denies_before_serial_and_token(self):
        for answer in ('','yes','opne','OPEN NOW'):
            result,events,_,_,_=self.run_cli(answers=[answer]);self.assertNotEqual(result,0)
            self.assertEqual(events,[('confirm',answer)])
    def test_stable_denial_never_requests_token_or_writes(self):
        result,events,port,_,_=self.run_cli(answers=['OPEN',''])
        self.assertNotEqual(result,0);self.assertFalse(port.frames);self.assertNotIn(('masked',),events);self.assertEqual(events[-1],('close',))
    def test_happy_path_order_flags_and_real_frame(self):
        result,events,port,out,elapsed=self.run_cli()
        self.assertEqual(result,0);self.assertEqual(len(port.frames),1)
        self.assertLessEqual(len(port.frames[0]),755)
        uri,device,token,req=unpack_grant(port.frames[0]);self.assertEqual(uri,user_pairing.ENDPOINT);self.assertEqual(device,user_pairing.DEVICE);self.assertEqual(token,TOKEN);self.assertEqual(len(bytes.fromhex(req)),16)
        self.assertLess(events.index(('open',)),events.index(('confirm','STABLE')))
        self.assertLess(events.index(('confirm','STABLE')),events.index(('masked',)))
        self.assertLess(events.index(('confirm','PAIR')),events.index(('write',)))
        self.assertLess(events.index(('confirm','MUTED')),events.index(('close',)))
        self.assertGreaterEqual(elapsed,2)
        construct=next(e[1] for e in events if e[0]=='construct');self.assertIsNone(construct['port']);self.assertLessEqual(construct['write_timeout'],2)
        self.assertLess(events.index(('dtr',False)),events.index(('open',)));self.assertLess(events.index(('rts',False)),events.index(('open',)))
        self.assertIn('staged',out.lower());self.assertIn('reset',out.lower());self.assertIn('not',out.lower())
    def test_no_ready_never_writes(self):
        result,_,port,out,elapsed=self.run_cli(mode='missing_ready');self.assertNotEqual(result,0);self.assertFalse(port.frames);self.assertIn('unsupported',out);self.assertLess(elapsed,8)
    def test_ready_reject_never_writes(self):
        result,_,port,out,_=self.run_cli(mode='reject_ready');self.assertNotEqual(result,0);self.assertFalse(port.frames);self.assertIn('rejected',out)
    def test_staged_failure_labels_and_no_retries(self):
        for mode,label in [('new_boot','reset_detected'),('repeat_ready','window_changed'),('expired','expired'),('rejected','rejected'),('wrong_uuid','request_mismatch'),('missing_staged','receipt_missing'),('short','incomplete_write')]:
            with self.subTest(mode=mode):
                result,events,port,out,_=self.run_cli(mode=mode);self.assertNotEqual(result,0);self.assertEqual(len(port.frames),1);self.assertIn(label,out);self.assertEqual(events[-1],('close',))
    def test_transport_exceptions_sanitized(self):
        for mode in ('open_error','lost','write_error'):
            result,events,_,out,_=self.run_cli(mode=mode);self.assertNotEqual(result,0);self.assertIn('transport_lost',out);self.assertEqual(events[-1],('close',))
    def test_flood_is_bounded(self):
        result,_,port,out,elapsed=self.run_cli(mode='flood');self.assertNotEqual(result,0);self.assertFalse(port.frames);self.assertIn('input_limit',out);self.assertLess(elapsed,2)
    def test_invalid_token_does_not_write(self):
        for token in ('','x'*257,'space token','é'):
            result,_,port,_,_=self.run_cli(token=token);self.assertNotEqual(result,0);self.assertFalse(port.frames)
    def test_largest_token_supported(self):
        result,_,port,_,_=self.run_cli(token='x'*256);self.assertEqual(result,0);self.assertLessEqual(len(port.frames[0]),755)
    def test_pair_denial_never_writes(self):
        result,_,port,_,_=self.run_cli(answers=['OPEN','STABLE','']);self.assertNotEqual(result,0);self.assertFalse(port.frames)
    def test_keep_open_until_exact_muted(self):
        result,events,_,_,_=self.run_cli(answers=['OPEN','STABLE','PAIR','','mte','MUTED']);self.assertEqual(result,0);self.assertEqual(events[-2:],[('confirm','MUTED'),('close',)])
    def test_argv_secret_never_echoed(self):
        result,events,_,_,_=self.run_cli(argv=['--token',TOKEN]);self.assertNotEqual(result,0);self.assertEqual(events,[])
    def test_alarm_lifecycle_no_prior_alarm_overwrite(self):
        import signal
        with patch.object(user_pairing.signal,'getitimer',return_value=(10,0)):
            result,events,_,out,_=self.run_cli();self.assertNotEqual(result,0);self.assertFalse(any(e[0]=='construct' for e in events));self.assertIn('deadline_unavailable',out)
    def test_two_receipts_in_one_usb_read_preserve_nonce_change(self):
        result,_,port,out,_=self.run_cli(mode='compound_ready')
        self.assertNotEqual(result,0);self.assertEqual(len(port.frames),1);self.assertIn('reset_detected',out)
    def test_session_expiry_restores_alarm_and_closes_once(self):
        with patch.object(user_pairing,'_stabilize',side_effect=user_pairing.PairingFailure('session_expired')),patch.object(user_pairing.signal,'getitimer',return_value=(0,0)),patch.object(user_pairing.signal,'getsignal',return_value='prior'),patch.object(user_pairing.signal,'signal') as handler,patch.object(user_pairing.signal,'setitimer') as timer:
            result,events,port,out,_=self.run_cli()
        self.assertNotEqual(result,0);self.assertIn('session_expired',out);self.assertEqual(events.count(('close',)),1);self.assertFalse(port.frames)
        self.assertEqual(timer.call_args_list[-1].args,(user_pairing.signal.ITIMER_REAL,0))
        self.assertEqual(handler.call_args_list[-1].args,(user_pairing.signal.SIGALRM,'prior'))
    def test_authorized_opener_skips_only_open_prompt(self):
        result,events,port,_,_=self.run_cli(argv=['--port','/dev/offline-fixture','--open-approved-usb'],answers=['STABLE','PAIR','MUTED'])
        self.assertEqual(result,0);self.assertNotIn(('confirm','OPEN'),events)
        self.assertLess(events.index(('open',)),events.index(('confirm','STABLE')))
        self.assertLess(events.index(('confirm','STABLE')),events.index(('masked',)))
        self.assertLess(events.index(('confirm','PAIR')),events.index(('write',)))
        self.assertEqual(len(port.frames),1)
    def test_authorized_opener_stable_denial_no_credential_or_write(self):
        result,events,port,_,elapsed=self.run_cli(argv=['--port','/dev/offline-fixture','--open-approved-usb'],answers=[''])
        self.assertNotEqual(result,0);self.assertIn(('open',),events);self.assertGreaterEqual(elapsed,2)
        self.assertNotIn(('masked',),events);self.assertFalse(port.frames)
    def test_authorized_opener_does_not_bypass_pause(self):
        result,events,_,out,_=self.run_cli(argv=['--port','/dev/offline-fixture','--open-approved-usb'],paused=True)
        self.assertNotEqual(result,0);self.assertEqual(events,[]);self.assertIn('paused',out)
    def test_default_open_prompt_precedes_deadline_start(self):
        events=[]
        with patch.object(user_pairing.signal,'setitimer',side_effect=lambda *a:events.append(a)):
            def prompt(text):
                self.assertEqual(events,[])
                return ''
            streams=[Terminal() for _ in range(3)]
            with patch.object(sys,'stdin',streams[0]),patch.object(sys,'stdout',streams[1]),patch.object(sys,'stderr',streams[2]),patch.object(user_pairing,'_pairing_paused',return_value=False),patch('builtins.input',prompt):
                self.assertNotEqual(user_pairing.main(['--port','/dev/offline-fixture']),0)
        self.assertEqual(events,[])
    def test_checkpoints_accept_case_and_surrounding_whitespace(self):
        result,events,port,_,_=self.run_cli(answers=['  open  ','\tStable\t',' pair ',' MuTeD '])
        self.assertEqual(result,0);self.assertEqual(len(port.frames),1)
        self.assertEqual(events[-1],('close',))
    def test_checkpoint_typos_do_not_authorize(self):
        for answers in (['opne'],['OPEN','stabel'],['OPEN','STABLE','piar']):
            result,_,port,_,_=self.run_cli(answers=answers)
            self.assertNotEqual(result,0);self.assertFalse(port.frames)
    def test_muted_typo_keeps_descriptor_open(self):
        result,events,_,_,_=self.run_cli(answers=['OPEN','STABLE','PAIR','mutde',' muted '])
        self.assertEqual(result,0)
        self.assertEqual(events[-2:],[('confirm',' muted '),('close',)])
    def test_masking_warning_never_falls_back(self):
        import getpass,warnings
        def warning(text):warnings.warn('synthetic',getpass.GetPassWarning)
        # Standalone fixture invokes the actual warning guard.
        streams=[Terminal() for _ in range(3)];events=[];port=Boundary(events,'');clock=Clock()
        with patch.object(sys,'stdin',streams[0]),patch.object(sys,'stdout',streams[1]),patch.object(sys,'stderr',streams[2]),patch('builtins.input',side_effect=['OPEN','STABLE']),patch('getpass.getpass',warning),patch.dict(sys.modules,{'serial':types.SimpleNamespace(Serial=lambda **kw:port)}),patch.object(user_pairing,'_pairing_paused',return_value=False,create=True),patch.object(user_pairing.time,'monotonic',clock.monotonic),patch.object(user_pairing.time,'sleep',clock.sleep):
            self.assertNotEqual(user_pairing.main(['--port','/dev/offline-fixture']),0)
        self.assertFalse(port.frames);self.assertEqual(events[-1],('close',))
if __name__=='__main__':unittest.main()
