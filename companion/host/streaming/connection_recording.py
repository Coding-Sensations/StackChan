"""Bounded public connection evidence; never retains headers, media or error text.

Construction is inert. The worker supplies one private destination. Each event
atomically replaces a latest-16-event file; a new worker starts a new history.
"""
from collections import deque
import json
import os
from pathlib import Path
import tempfile
import time
from transport_diagnostics import project_diagnostics

EVENTS={('listener','ready'),('listener','closed'),('listener','setup_failed'),
        ('http_admission','admission_denied'),('http_admission','route_unavailable'),
        ('http_admission','upgrade_allowed'),('websocket','session_started'),
        ('session_closed','transport_fault'),('session_closed','transport_complete'),
        ('session_closed','cleanup_fault')}

def record_event(sink,stage,reason,diagnostics=None):
    # Diagnostic storage cannot change authentication, closure or readiness.
    if sink is not None:
        try:sink(stage,reason,diagnostics)
        except Exception:pass

class ConnectionRecording:
    def __init__(self,path):
        self.path=Path(path);self.events=deque(maxlen=16);self.sequence=0
        self.started_unix=time.time();self.pid=os.getpid()
    def __call__(self,stage,reason,diagnostics=None):
        if type(stage) is not str or type(reason) is not str or (stage,reason) not in EVENTS:
            raise ValueError('invalid public event')
        self.sequence+=1
        event=dict(sequence=self.sequence,observed_unix=time.time(),stage=stage,reason=reason)
        if diagnostics is not None:event['diagnostics']=project_diagnostics(diagnostics)
        self.events.append(event)
        value=dict(version=1,pid=self.pid,started_unix=self.started_unix,events=list(self.events))
        if self.path.is_symlink():raise OSError('diagnostic destination unavailable')
        temporary=None
        try:
            with tempfile.NamedTemporaryFile(mode='w',encoding='utf-8',dir=self.path.parent,prefix='.connection-diagnostic-',delete=False) as output:
                temporary=Path(output.name)
                json.dump(value,output,separators=(',',':'),allow_nan=False);output.write('\n')
            os.replace(temporary,self.path);temporary=None
        finally:
            if temporary is not None:temporary.unlink(missing_ok=True)
