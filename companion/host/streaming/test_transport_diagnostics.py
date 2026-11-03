"""Offline driver diagnostics; synthetic socket boundaries, no listener/audio device."""
import asyncio
import json
import unittest
from types import SimpleNamespace
from gateway_connection import run_connection
try:
    import transport_diagnostics as d
except ImportError:
    d=None
SECRET='synthetic-private-message-DO-NOT-SAVE'
class Session:
    def __init__(self,cleanup=False):
        self.armed=False;self.acknowledged=False;self.deadline=2.;self.clock=lambda:0.;self.cleanup=cleanup
    def _authorized(self):raise TimeoutError(SECRET)
    def close(self):
        self.armed=False
        if self.cleanup:raise RuntimeError(SECRET)
class Socket:
    async def recv(self):await asyncio.Event().wait()
    async def close(self):raise OSError(SECRET)
class DriverTests(unittest.IsolatedAsyncioTestCase):
    async def test_original_authority_fault_survives_both_cleanup_failures(self):
        self.assertIsNotNone(d,'sanitized diagnostics missing')
        diagnostics=d.TransportDiagnostics(lambda:0)
        with self.assertRaises(TimeoutError):await run_connection(Socket(),Session(cleanup=True),diagnostics=diagnostics)
        result=diagnostics.snapshot();self.assertEqual(result['fault']['type'],'TimeoutError');self.assertEqual(result['fault']['phase'],'authority')
        self.assertEqual(result['cleanup_fault']['type'],'RuntimeError');self.assertEqual(result['cleanup_fault_count'],2)
        self.assertNotIn(SECRET,json.dumps(result));self.assertFalse(any('source' in frame or 'locals' in frame for frame in result['fault']['stack']))
    async def test_send_failure_counts_only_successful_packets(self):
        self.assertIsNotNone(d)
        class ArmedSession:
            clock=lambda self:0.
            armed=True;acknowledged=True;epoch=1;deadline=2.
            bridge=SimpleNamespace(adapter=SimpleNamespace(playing=True))
            def _authorized(self):pass
            def playing(self,value):return {'type':'playing','epoch':1,'value':value}
            def outgoing(self,frames):return [b'x'*17,b'x'*17]
            def close(self):self.armed=False
        class SendSocket(Socket):
            def __init__(self):self.sent=0
            async def send(self,value):
                if type(value) is bytes:
                    self.sent+=1
                    if self.sent==2:raise ConnectionError(SECRET)
            async def close(self):pass
        diagnostics=d.TransportDiagnostics(lambda:0)
        with self.assertRaises(ConnectionError):await run_connection(SendSocket(),ArmedSession(),diagnostics=diagnostics)
        self.assertEqual(diagnostics.snapshot()['sent_speaker_packets'],1);self.assertEqual(diagnostics.snapshot()['fault']['phase'],'audio_send')
class ProjectionTests(unittest.TestCase):
    def test_unknown_strings_paths_raw_fields_are_discarded(self):
        self.assertIsNotNone(d)
        raw={'fault':{'type':SECRET,'phase':SECRET,'stack':[{'file':'/private/'+SECRET,'function':SECRET,'line':4,'source':SECRET}]},'last_control':SECRET,'heartbeat_count':3,'ack_received':True,'credential':SECRET,'raw_pcm':b'x'}
        value=d.project_diagnostics(raw);self.assertNotIn(SECRET,json.dumps(value));self.assertNotIn('credential',value);self.assertNotIn('raw_pcm',value)
    def test_nonfinite_or_wrong_type_counts_rejected(self):
        self.assertIsNotNone(d)
        value=d.project_diagnostics({'heartbeat_count':True,'connection_elapsed_ms':float('inf'),'ack_received':1})
        self.assertNotIn('heartbeat_count',value);self.assertNotIn('connection_elapsed_ms',value);self.assertNotIn('ack_received',value)
    def test_chained_original_fault_only_sanitized_types(self):
        self.assertIsNotNone(d)
        try:
            try:raise ValueError(SECRET)
            except ValueError:raise OSError(SECRET)
        except OSError as error:
            diag=d.TransportDiagnostics(lambda:0);diag.fault(error,'receive')
        value=diag.snapshot();self.assertEqual(value['fault']['type'],'OSError');self.assertEqual(value['fault']['causes'][0]['type'],'ValueError');self.assertNotIn(SECRET,json.dumps(value))
    def test_malformed_enum_shapes_never_raise_or_retain(self):
        for value in ([],{},123,None):
            result=d.project_diagnostics({'fault':{'type':value,'phase':value,'stack':[{'file':value,'function':value,'line':1}]},'last_control':value,'phase':value})
            self.assertEqual(result['fault']['type'],'OtherError');self.assertEqual(result['fault']['stack'],[])
    def test_native_backpressure_trace_projects_only_allowlisted_call_sites(self):
        value=d.project_diagnostics({'fault':{'type':'AudioBackpressure','phase':'audio_receive','stack':[
            {'file':'native_adapter.py','function':'robot_pcm','line':87},
            {'file':'native_conversation.py','function':'receive_opus','line':60},
            {'file':'/private/path/native_adapter.py','function':'robot_pcm','line':88},
            {'file':'native_adapter.py','function':'unlisted','line':89},
        ]}})
        self.assertEqual(value['fault']['stack'],[
            {'file':'native_adapter.py','function':'robot_pcm','line':87},
            {'file':'native_conversation.py','function':'receive_opus','line':60},
        ])

    def test_close_code_without_reason(self):
        ConnectionClosedError=type('ConnectionClosedError',(Exception,),{})
        error=ConnectionClosedError(SECRET);error.rcvd=SimpleNamespace(code=1006,reason=SECRET)
        diagnostics=d.TransportDiagnostics(lambda:0);diagnostics.fault(error,'receive')
        self.assertEqual(diagnostics.snapshot()['fault']['close_code'],1006);self.assertNotIn(SECRET,json.dumps(diagnostics.snapshot()))
if __name__=='__main__':unittest.main()
