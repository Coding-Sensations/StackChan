"""Bounded closure-evidence consumer only. No camera grants, IO or activation.
Existing owner/current-custody and qualified evidence adapters are dependencies.
Parsed driver data never authenticates itself or proves physical sensor cessation.
"""
from dataclasses import dataclass, field
import json, math, secrets, time
from threading import RLock
from functools import wraps

def _serialized(method):
    @wraps(method)
    def call(self,*args,**kwargs):
        with self._lock:return method(self,*args,**kwargs)
    return call
U64=(1<<64)-1
STAGES=15 # STREAMOFF1, UNMAP2, FD_CLOSE4, VIDEO_DEINIT8

def _u64(v):return type(v) is int and 1<=v<=U64

def _id(v,n):return type(v) is str and 1<=len(v)<=n and all(c.isascii() and (c.isalnum() or c in '-_.:') for c in v)

def _hex(v):return type(v) is str and len(v)==32 and all(c in '0123456789abcdef' for c in v)

def _decimal(v):
    if type(v) is not str or not 1<=len(v)<=20 or not v.isascii() or not v.isdecimal() or v.startswith('0') or int(v)>U64:raise ValueError('exact positive UInt64 decimal required')
    return int(v)

@dataclass(frozen=True)
class StopBinding:
    device:str
    conversation:str
    turn:str
    generation:int
    epoch:int
    def __post_init__(self):
        if not (_id(self.device,64) and _id(self.conversation,128) and _id(self.turn,128) and _u64(self.generation) and _u64(self.epoch)):raise ValueError('exact existing camera tuple required')

@dataclass(frozen=True)
class RetainedStopContext:
    # A strong reference to the exact existing admitted connection/session. Not a wire ID.
    session:object=field(repr=False,compare=False)
    binding:StopBinding
    boot:str
    authority_id:int
    scope_expires:float # original capture lease metadata; never abandons owned cleanup
    def __post_init__(self):
        if self.session is None or type(self.binding) is not StopBinding or not _hex(self.boot) or not _u64(self.authority_id) or type(self.scope_expires) not in (int,float) or not math.isfinite(self.scope_expires):raise ValueError('retained owner context required')

@dataclass(frozen=True)
class StopChallenge:
    context:RetainedStopContext
    nonce:str
    sequence:int
    expires:float

@dataclass(frozen=True)
class DriverStopReport:
    binding:StopBinding
    boot:str
    authority_id:int
    challenge:str
    stop_sequence:int
    report_serial:int
    outcome:str
    required:int
    completed:int
    revoked:bool
    joined:bool
    wiped:bool
    custody_released:bool
    close_uncertain:bool
    ever_started:bool
    @classmethod
    def parse(cls,wire):
        if type(wire) is not str or not 1<=len(wire.encode())<=1024:raise ValueError('bounded Stop report required')
        def unique(pairs):
            result={}
            for k,v in pairs:
                if k in result:raise ValueError('duplicate Stop field')
                result[k]=v
            return result
        v=json.loads(wire,object_pairs_hook=unique,parse_constant=lambda _:(_ for _ in ()).throw(ValueError('invalid number')))
        keys={'type','version','device','conversation','turn','generation','epoch','boot','authority_id','challenge','stop_sequence','report_serial','outcome','required','completed','revoked','joined','wiped','custody_released','close_uncertain','ever_started'}
        if type(v) is not dict or set(v)!=keys or v['type']!='camera_driver_stop' or type(v['version']) is not int or v['version']!=1:raise ValueError('Stop schema rejected')
        b=StopBinding(v['device'],v['conversation'],v['turn'],_decimal(v['generation']),_decimal(v['epoch']))
        if not _hex(v['boot']) or not _hex(v['challenge']) or v['outcome'] not in ('Busy','Stale','DriverFailure','NotStarted','DriverClosed'):raise ValueError('Stop correlation/outcome rejected')
        if any(type(v[k]) is not bool for k in ('revoked','joined','wiped','custody_released','close_uncertain','ever_started')) or any(type(v[k]) is not int or not 0<=v[k]<=STAGES for k in ('required','completed')):raise ValueError('Stop predicates rejected')
        return cls(b,v['boot'],_decimal(v['authority_id']),v['challenge'],_decimal(v['stop_sequence']),_decimal(v['report_serial']),v['outcome'],v['required'],v['completed'],v['revoked'],v['joined'],v['wiped'],v['custody_released'],v['close_uncertain'],v['ever_started'])
    def driver_closed(self):
        return type(self.binding) is StopBinding and _hex(self.boot) and _hex(self.challenge) and _u64(self.authority_id) and _u64(self.stop_sequence) and _u64(self.report_serial) and type(self.required) is int and type(self.completed) is int and all(type(x) is bool for x in (self.revoked,self.joined,self.wiped,self.custody_released,self.close_uncertain,self.ever_started)) and self.outcome=='DriverClosed' and self.ever_started is True and self.revoked is True and self.joined is True and self.wiped is True and self.custody_released is True and self.close_uncertain is False and 0<=self.required<=STAGES and self.completed==self.required

@dataclass(frozen=True)
class PhysicalStopAttestation:
    """Result of an external qualified local hardware adapter; not a wire schema.
    Constructing this value does not establish provenance. Only the configured
    physical_qualification callback may return it under existing owner custody.
    """
    challenge:StopChallenge
    report_serial:int
    qualification_reference:str
    sensor_ceased:bool
    dma_quiescent:bool
    no_new_frames:bool
    start_barrier_held:bool
    def matches(self,p,r):
        return self.challenge is p and self.report_serial==r.report_serial and _u64(self.report_serial) and _id(self.qualification_reference,128) and all(x is True for x in (self.sensor_ceased,self.dma_quiescent,self.no_new_frames,self.start_barrier_held))

class QualifiedPhysicalStop:
    """Consumer-sealed, one-use local result; never constructed from peer JSON."""
    __slots__=('_seal','_challenge','_report')
    def __init__(self,seal,challenge,report):self._seal=seal;self._challenge=challenge;self._report=report
    @property
    def binding(self):return self._challenge.context.binding

class StopEvidenceConsumer:
    def __init__(self,owner_current,*,driver_origin=None,physical_qualification=None,no_newer_stream=None,clock=time.monotonic,nonce=lambda:secrets.token_hex(16)):
        # owner_current is the existing camera owner callback; no new grant verifier.
        self.current=owner_current;self.driver_origin=driver_origin;self.physical=physical_qualification;self.no_newer=no_newer_stream
        self.clock=clock;self.nonce=nonce;self.pending=None;self.sequence=0;self.last_clock=-1;self.driver_serial=0;self.driver_closed=False;self._seal=object();self._issued=None;self._serial_scope=None;self._lock=RLock()
    @property
    def stop_pending(self):return self.pending is not None
    def _now(self):
        now=self.clock()
        if type(now) not in (int,float) or not math.isfinite(now) or now<0 or now<self.last_clock:raise PermissionError('monotonic Stop clock unavailable')
        self.last_clock=now;return now
    @_serialized
    def begin(self,context):
        now=self._now()
        if type(context) is not RetainedStopContext or not self.current(context):raise PermissionError('existing current camera owner unavailable')
        if self.pending is not None:
            p=self.pending.context
            if p.session is not context.session or p!=context:raise PermissionError("older Stop retains custody")
            if now<self.pending.expires:return self.pending
            # Same retained custody may retry after expiry; retire the old challenge/receipt.
        if self.sequence==U64:raise OverflowError('Stop challenge sequence exhausted')
        value=self.nonce()
        if not _hex(value):raise ValueError('bounded fresh challenge required')
        self.sequence+=1;self.pending=StopChallenge(context,value,self.sequence,now+2);self.driver_closed=False;self._issued=None
        serial_scope=(context.session,context.boot,context.authority_id)
        if self._serial_scope is None or self._serial_scope[0] is not context.session or self._serial_scope[1:]!=serial_scope[1:]:self.driver_serial=0
        self._serial_scope=serial_scope
        return self.pending
    def _matches(self,session,report,now):
        p=self.pending
        return (p is not None and session is p.context.session and now<p.expires and self.current(p.context) and report.binding==p.context.binding and report.boot==p.context.boot and report.authority_id==p.context.authority_id and report.challenge==p.nonce and report.stop_sequence==p.sequence)
    @_serialized
    def observe(self,session,report):
        now=self._now()
        if type(report) is not DriverStopReport or not self._matches(session,report,now) or report.report_serial<=self.driver_serial or not report.driver_closed():return None
        p=self.pending
        # Parsed flags are untrusted until a separately qualified local adapter
        # validates genuine producer-report provenance under the retained owner.
        if self.driver_origin is None or self.driver_origin(p,report) is not True:return None
        if not self._matches(session,report,self._now()):return None
        self.driver_serial=report.report_serial;self.driver_closed=True
        # Driver closure never proves sensor rail/reset, DMA cessation or no-new frames.
        # These callbacks must attest genuine hardware/start-barrier observations,
        # not merely echo report booleans. No production adapter is supplied.
        if self.physical is None or self.no_newer is None:return None
        attestation=self.physical(p,report)
        if type(attestation) is not PhysicalStopAttestation or not attestation.matches(p,report) or self.no_newer(p) is not True:return None
        if not self._matches(session,report,self._now()) or self.no_newer(p) is not True:return None
        self._issued=QualifiedPhysicalStop(self._seal,p,report);return self._issued
    @_serialized
    def consume(self,session,receipt):
        now=self._now();p=self.pending
        if type(receipt) is not QualifiedPhysicalStop or receipt is not self._issued or receipt._seal is not self._seal or receipt._challenge is not p or not self._matches(session,receipt._report,now) or self.no_newer is None or self.no_newer(p) is not True:return False
        # Caller must run this and release custody under existing owner serialization.
        self.pending=None;self._issued=None;self.driver_closed=False;return True
