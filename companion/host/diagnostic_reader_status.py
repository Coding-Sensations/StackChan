"""Public capture lifecycle/status observer. Import opens nothing; no serial IO."""
import hashlib
import subprocess
import datetime
import json
import math
import os
from pathlib import Path
import tempfile
import time
import uuid

def _process_probe(pid):
    try:
        os.kill(pid,0)
        query=subprocess.run(['/bin/ps','-p',str(pid),'-o','stat=,lstart='],capture_output=True,text=True,timeout=1)
        parts=query.stdout.strip().split(maxsplit=1)
        if query.returncode or len(parts)!=2 or 'Z' in parts[0]:return None
        return hashlib.sha256(parts[1].encode('ascii')).hexdigest()
    except (ProcessLookupError,PermissionError,OSError,ValueError,subprocess.TimeoutExpired):return None

class ReaderStatus:
    def __init__(self,path=None):
        self.path=Path(path) if path else None
        self.capture_id=uuid.uuid4().hex
        self.value=dict(capture_id=self.capture_id,pid=os.getpid(),state='starting',reader_closed=False,process_start_token=_process_probe(os.getpid()))
        if self.value['process_start_token'] is None:raise ValueError('process_identity_unavailable')
        self.last_printed=None
        if self.path:
            # Never replace an older capture's evidence/status accidentally.
            with self.path.open('x',encoding='utf-8') as output:output.write(json.dumps(self.value))
        self.emit(dict(state='starting'))
    def emit(self,event):
        now=time.time();state=event['state']
        self.value.update(event,observed_unix=now,observed_utc=datetime.datetime.fromtimestamp(now,datetime.timezone.utc).isoformat())
        if state=='listening' and 'opened_utc' not in self.value:
            self.value['opened_utc']=self.value['observed_utc']
            self.value['deadline_unix']=now+event['remaining_seconds']
            self.value['deadline_utc']=datetime.datetime.fromtimestamp(self.value['deadline_unix'],datetime.timezone.utc).isoformat()
        if self.path:
            temporary=None
            try:
                with tempfile.NamedTemporaryFile(mode='w',encoding='utf-8',dir=self.path.parent,prefix=self.path.name+'.',delete=False) as output:
                    temporary=Path(output.name);json.dump(self.value,output,allow_nan=False);output.write('\n')
                os.replace(temporary,self.path);temporary=None
            finally:
                if temporary:temporary.unlink(missing_ok=True)
        if state!=self.last_printed:
            print('DIAGNOSTIC_STATUS '+json.dumps(self.value,allow_nan=False),flush=True);self.last_printed=state

def check_status(path,capture_id=None,*,now=None,process_probe=_process_probe):
    result=dict(listening=False,state='unavailable',reason='status_unavailable')
    try:
        with Path(path).open('rb') as source:raw=source.read(16385)
        if len(raw)>16384:return result
        value=json.loads(raw)
        identity=value.get('capture_id');pid=value.get('pid');state=value.get('state')
        if type(identity) is not str or len(identity)!=32 or any(c not in '0123456789abcdef' for c in identity):return result
        if type(pid) is not int or not 0<pid<2**31:return result
        if state not in ('starting','listening','closed','completed','failed'):return result
        result.update(capture_id=identity,pid=pid,state=state)
        if capture_id is not None and identity!=capture_id:result['reason']='capture_mismatch';return result
        if state in ('completed','failed'):result['reason']='not_listening';return result
        token=value.get('process_start_token')
        if type(token) is not str or len(token)!=64 or process_probe(pid)!=token:result['reason']='process_absent_or_changed';return result
        if state!='listening':result['reason']='not_listening';return result
        now=time.time() if now is None else now
        observed=value.get('observed_unix');deadline=value.get('deadline_unix')
        if any(type(x) not in (int,float) or not math.isfinite(x) for x in (now,observed,deadline)):return result
        if not 0<=now-observed<=2:result['reason']='status_stale';return result
        if now>=deadline:result['reason']='deadline_passed';return result
        if value.get('reader_closed') is not False:result['reason']='reader_closed';return result
        result.update(listening=True,reason='fresh_live_reader',deadline_utc=value.get('deadline_utc'),remaining_seconds=deadline-now)
        return result
    except (OSError,ValueError,TypeError,AttributeError):return result

def watch_status(path,capture_id=None,seconds=30):
    if type(seconds) not in (int,float) or not math.isfinite(seconds) or not 0<seconds<=300:raise ValueError('invalid_watch_timeout')
    deadline=time.monotonic()+seconds;last=None;seen=False
    while time.monotonic()<deadline:
        value=check_status(path,capture_id)
        key=(value['state'],value['reason'])
        if key!=last:print('DIAGNOSTIC_WATCH '+json.dumps(value),flush=True);last=key
        if value['listening']:seen=True
        if value['reason'] in ('capture_mismatch','process_absent_or_changed'):return 1
        if value['state'] in ('completed','failed'):return 0 if value['state']=='completed' else 1
        if seen and value['state'] not in ('closed','starting') and not value['listening']:return 1
        time.sleep(.1)
    print('DIAGNOSTIC_WATCH '+json.dumps(dict(listening=False,state='watch_expired')),flush=True)
    return 1
