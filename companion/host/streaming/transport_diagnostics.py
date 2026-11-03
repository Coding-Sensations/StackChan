"""Fixed-schema diagnostics; no exception text, source, locals or audio retention."""
import os
import time
TYPES={'NativeStartupPending','TimeoutError','PermissionError','ValueError','BufferError','AudioBackpressure','SustainedAudioBackpressure','OSError','ConnectionError','OverflowError','RuntimeError','CancelledError','UnicodeDecodeError','KeyError','TransportComplete','ConnectionClosed','ConnectionClosedError','ConnectionClosedOK','OtherError'}
PHASES={'native_startup','provider_create','authorization','startup','admission','session_create','authority','duration','receive','control','control_send','audio_receive','playback_state','playback_send','audio_pull','audio_send','session_cleanup','receive_cleanup','socket_cleanup','statistics','complete'}
CONTROLS={'none','unknown','begin','ack','consent','heartbeat','interrupt'}
FILES={'gateway_connection.py','transport_test.py','streaming_session.py','codec.py','gateway_admission.py','connection.py','protocol.py','frames.py','native_adapter.py','native_conversation.py'}
FUNCTIONS={'poll_control','start_control','prepare','prepared','arm','open_for_approved_test','_take_startup','_build_native_bridge','__init__','_cue_valid','run_connection','handle','_authorized','control','parse_control','receive','playing','outgoing','outgoing_opus','receive_opus','decode','encode','authenticate','statistics','send','recv','close','ensure_open','begin','_active','interrupt','robot_pcm','poll_voice','pull_pcm'}
COUNTS={'backpressure_mic_packets':0x7fffffff,'backpressure_mic_streak':0x7fffffff,'heartbeat_count':0x7fffffff,'sent_speaker_packets':0x7fffffff,'cleanup_fault_count':3,'connection_elapsed_ms':900000,'consent_elapsed_ms':900000,'heartbeat_remaining_ms':2000}

NATIVE_COUNTS=('input_callbacks','output_callbacks','input_frames','output_frames',
               'output_consumed_frames','input_queue_frames','output_queue_frames',
               'input_dropped','output_dropped','input_underruns','output_underruns')
def project_native_audio(value):
    """Numeric callback/queue evidence only; no PCM or exception content."""
    if type(value) is not dict:return {}
    result={key:value[key] for key in NATIVE_COUNTS if type(value.get(key)) is int and 0<=value[key]<=0x7fffffffffffffff}
    error=value.get('error')
    if type(error) is int and -0x80000000<=error<=0x7fffffff:result['error']=error
    return result

def _fault_projection(value):
    if type(value) is not dict:return None
    result={'type':value.get('type') if type(value.get('type')) is str and value.get('type') in TYPES else 'OtherError','phase':value.get('phase') if type(value.get('phase')) is str and value.get('phase') in PHASES else 'startup','stack':[]}
    stack=value.get('stack')
    if type(stack) is list:
        for frame in stack[:6]:
            if type(frame) is dict and type(frame.get('file')) is str and frame.get('file') in FILES and type(frame.get('function')) is str and frame.get('function') in FUNCTIONS and type(frame.get('line')) is int and 1<=frame['line']<=100000:
                result['stack'].append({key:frame[key] for key in ('file','function','line')})
    if type(value.get('close_code')) is int and 1000<=value['close_code']<=4999:result['close_code']=value['close_code']
    return result

def project_diagnostics(value):
    if type(value) is not dict:return {}
    result={}
    for key in ('fault','cleanup_fault'):
        fault=_fault_projection(value.get(key))
        if fault is not None:
            causes=value[key].get('causes')
            if type(causes) is list:
                projected=[_fault_projection(cause) for cause in causes[:2]]
                if any(projected):fault['causes']=[cause for cause in projected if cause is not None]
            result[key]=fault
    native=project_native_audio(value.get('native_audio'))
    if native:result['native_audio']=native
    for key,limit in COUNTS.items():
        number=value.get(key)
        if type(number) is int and 0<=number<=limit:result[key]=number
    for key in ('ack_received','consent_received'):
        if type(value.get(key)) is bool:result[key]=value[key]
    if type(value.get('last_control')) is str and value.get('last_control') in CONTROLS:result['last_control']=value['last_control']
    if type(value.get('phase')) is str and value.get('phase') in PHASES:result['phase']=value['phase']
    return result

def _exception(exc,phase):
    kind=type(exc).__name__
    result={'type':kind if kind in TYPES else 'OtherError','phase':phase if phase in PHASES else 'startup','stack':[]}
    trace=exc.__traceback__
    while trace is not None:
        code=trace.tb_frame.f_code;file=os.path.basename(code.co_filename)
        if file in FILES and code.co_name in FUNCTIONS and 1<=trace.tb_lineno<=100000:
            result['stack'].append({'file':file,'function':code.co_name,'line':trace.tb_lineno})
            result['stack']=result['stack'][-6:]
        trace=trace.tb_next
    if kind in ('ConnectionClosed','ConnectionClosedError','ConnectionClosedOK'):
        for side in ('rcvd','sent'):
            try:close=getattr(getattr(exc,side,None),'code',None)
            except Exception:close=None
            if type(close) is int and 1000<=close<=4999:result['close_code']=close;break
    return result

class TransportDiagnostics:
    def __init__(self,clock=time.monotonic):
        self.clock=clock;self.started=clock();self.consent_at=None
        self.value={'phase':'startup','last_control':'none','heartbeat_count':0,'sent_speaker_packets':0,'ack_received':False,'consent_received':False,'cleanup_fault_count':0}
    @property
    def phase(self):return self.value['phase']
    def set_phase(self,value):self.value['phase']=value if type(value) is str and value in PHASES else 'startup'
    def observe(self,session):
        # Read only the bridge's already-projected counters, never native IO or PCM.
        # Observe again on failure before cleanup can discard the exact owner.
        try:
            native=project_native_audio(getattr(getattr(session,'bridge',None),'native_audio',None))
            if native:self.value['native_audio']=native
            for key in ('backpressure_mic_packets','backpressure_mic_streak'):
                number=getattr(session,key,None)
                if type(number) is int and 0<=number<=COUNTS[key]:self.value[key]=number
        except Exception:pass # Evidence collection cannot change transport authority.
        if getattr(session,'acknowledged',False) is True:self.value['ack_received']=True
        if getattr(session,'armed',False) is True:
            self.value['consent_received']=True
            if self.consent_at is None:self.consent_at=self.clock()
        deadline=getattr(session,'deadline',None)
        if type(deadline) in (int,float):
            try:self.value['heartbeat_remaining_ms']=max(0,min(2000,int((deadline-self.clock())*1000)))
            except (OverflowError,ValueError):pass
    def control_label(self,value):
        self.value['last_control']=value if type(value) is str and value in CONTROLS else 'unknown'
    def accepted_control(self,session,kind):
        if kind=='heartbeat':self.value['heartbeat_count']=min(0x7fffffff,self.value['heartbeat_count']+1)
        self.observe(session)
    def speaker_sent(self):self.value['sent_speaker_packets']=min(0x7fffffff,self.value['sent_speaker_packets']+1)
    def fault(self,exc,phase=None,*,cleanup=False):
        key='cleanup_fault' if cleanup else 'fault'
        if cleanup:self.value['cleanup_fault_count']=min(3,self.value['cleanup_fault_count']+1)
        if key in self.value:return
        value=_exception(exc,phase or self.phase);causes=[];seen={id(exc)}
        cause=exc.__cause__ or exc.__context__
        while cause is not None and id(cause) not in seen and len(causes)<2:
            seen.add(id(cause));causes.append(_exception(cause,phase or self.phase));cause=cause.__cause__ or cause.__context__
        if causes:value['causes']=causes
        self.value[key]=value
    def expected_complete(self):
        if self.value.get('fault',{}).get('type')=='TransportComplete':self.value.pop('fault')
        self.set_phase('complete')
    def snapshot(self):
        now=self.clock()
        self.value['connection_elapsed_ms']=max(0,min(900000,int((now-self.started)*1000)))
        if self.consent_at is not None:self.value['consent_elapsed_ms']=max(0,min(900000,int((now-self.consent_at)*1000)))
        return project_diagnostics(self.value)
