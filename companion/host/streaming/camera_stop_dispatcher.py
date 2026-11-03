"""Existing-session Stop dispatch and retained cross-connection custody only.
No issuer, credentials, network setup, hardware IO or implicit physical proof.
"""
import json, threading
from dataclasses import dataclass
from camera_stop_evidence import RetainedStopContext,DriverStopReport,StopEvidenceConsumer

STOP_TYPES=frozenset(('camera_driver_stop','camera_stop_ack','camera_stop_challenge','camera_stop_acknowledged'))

def bounded_control(value,limit=1024):
    wire=json.dumps(value,separators=(',',':'),allow_nan=False)
    if not 1<=len(wire.encode())<=limit:raise ValueError('bounded camera control required')
    return wire

def correlation(p):
    c=p.context;b=c.binding
    return dict(version=1,device=b.device,conversation=b.conversation,turn=b.turn,
                generation=str(b.generation),epoch=str(b.epoch),boot=c.boot,
                authority_id=str(c.authority_id),challenge=p.nonce,stop_sequence=str(p.sequence))

@dataclass(frozen=True)
class RetainedBarrierReceipt:
    """External trusted adapter's qualified local result, never decoded from JSON.
    The adapter verifies sealed producer worker closure and the held remote start
    barrier; these typed fields cannot substitute for that qualification.
    """
    context:RetainedStopContext
    worker_joined:bool
    authority_revoked:bool
    no_newer_stream:bool
    def matches(self,c):
        return self.context is c and self.worker_joined is True and self.authority_revoked is True and self.no_newer_stream is True

@dataclass(frozen=True)
class RetainedAckReceipt:
    """Qualified local transport adapter attests genuine producer ACK consumption.
    Not decoded from a peer control and not camera capture authority.
    """
    challenge:object
    report_serial:int

@dataclass
class _Custody:
    session:object
    context:object=None
    retained:bool=False

class CameraStartCustodian:
    """A gateway-owned bounded barrier surviving session/route replacement.
    Availability checks never grant authority. reserve+bind protect actual admission.
    Missing context or lost connection retains custody until qualified local cleanup.
    """
    def __init__(self,max_devices=64):
        if type(max_devices) is not int or not 1<=max_devices<=64:raise ValueError('bounded custody slots')
        self._records={};self._lock=threading.RLock();self._max=max_devices
    def can_admit(self,session,mode='webcam'):
        with self._lock:return mode in ('webcam','gestures') and session.device_id not in self._records
    def reserve(self,session):
        with self._lock:
            session._authorized()
            if not session._active() or session.device_id in self._records or len(self._records)>=self._max:raise PermissionError('retained camera custody blocks start')
            self._records[session.device_id]=_Custody(session)
    def bind(self,session,context):
        with self._lock:
            r=self._records.get(session.device_id)
            if type(context) is not RetainedStopContext or context.session is not session or context.binding.device!=session.device_id or r is None or r.session is not session or r.context is not None:raise PermissionError('exact admitted camera custody required')
            r.context=context
    def can_adopt_reserved(self,session,context,mode):
        with self._lock:
            r=self._records.get(session.device_id)
            return (mode in ('webcam','gestures') and type(context) is RetainedStopContext and
                    r is not None and r.session is session and r.context is context and not r.retained)
    def reserve_context(self,session,context):
        # Atomic reservation + exact cleanup context: issuer adoption cannot
        # leave a contextless record between reserve and bind.
        with self._lock:
            if type(context) is not RetainedStopContext or context.session is not session or context.binding.device!=session.device_id:
                raise PermissionError('exact camera cleanup context required')
            self.reserve(session)
            self._records[session.device_id].context=context

    def retain(self,session):
        with self._lock:
            r=self._records.get(session.device_id)
            if r is not None and r.session is session:r.retained=True
    def barrier_held(self,context):
        with self._lock:
            r=self._records.get(context.binding.device)
            return r is not None and r.session is context.session and r.context is context and r.retained
    def release(self,context,qualified_cleanup):
        # Only dispatcher passes its opaque local completion seal after consume.
        with self._lock:
            if qualified_cleanup is not _RELEASE_SEAL or not self.barrier_held(context):return False
            del self._records[context.binding.device];return True
    def pending(self,device):
        with self._lock:return device in self._records

_RELEASE_SEAL=object()

class ExistingSessionStopDispatcher:
    """Owner methods are trusted local boundary adapters, never peer booleans.
    context_for_stop(session,purpose) -> exact retained owner context
    is_current_cleanup(context), driver_report_origin(challenge,report)
    optional qualify_physical_stop(challenge,report) -> PhysicalStopAttestation
    cleanup_barrier_receipt(context) -> RetainedBarrierReceipt from the trusted
    adapter qualifying sealed worker join/revocation/no-newer-stream observations
    No production adapter is fabricated by this dispatcher.
    """
    def __init__(self,session,owner,custodian,*,clock=None):
        self.session=session;self.owner=owner;self.custodian=custodian;self.context=None
        self._queued=None;self._proof=None;self._ack=None;self.ack_sent=False;self.ack_confirmed=False;self.ack_confirmed=False;self.completed=False;self.cleanup_completion=None
        self._lock=threading.RLock()
        def current(c):return self.context is c and self.custodian.barrier_held(c) and self._owner('is_current_cleanup',c) is True
        def no_newer(p):return self.custodian.barrier_held(p.context) and self._barrier_verified(p.context)
        self.consumer=StopEvidenceConsumer(current,driver_origin=lambda p,r:self._owner('driver_report_origin',p,r),physical_qualification=lambda p,r:self._owner('qualify_physical_stop',p,r),no_newer_stream=no_newer,clock=clock or session.clock)
    def _owner(self,name,*args):
        method=getattr(self.owner,name,None)
        if not callable(method):return None
        try:return method(*args)
        except Exception:return None
    def _barrier_verified(self,context):
        receipt=self._owner('cleanup_barrier_receipt',context)
        return type(receipt) is RetainedBarrierReceipt and receipt.matches(context)
    def reserve_for_media(self):
        # Reuse only this exact active permission reservation. Retained cleanup,
        # another session/context or a missing adoption proof still blocks start.
        with self._lock:
            if (self.context is not None and self.custodian.can_adopt_reserved(self.session,self.context,'webcam') and
                self._owner('permits_media_reservation',self.context,'webcam') is True):
                return
            self.custodian.reserve(self.session)
    def bind_active(self,purpose):
        with self._lock:
            context=self._owner('context_for_stop',self.session,purpose)
            if type(context) is not RetainedStopContext:raise PermissionError('trusted cleanup owner context unavailable')
            b=context.binding
            if (b.device,b.conversation,b.turn,b.generation,b.epoch)!=(purpose.device,purpose.conversation,purpose.turn,purpose.generation,purpose.epoch):raise PermissionError('camera cleanup tuple mismatch')
            if self.custodian.can_adopt_reserved(self.session,context,'webcam'):
                if self._owner('permits_media_reservation',context,'webcam') is not True:
                    raise PermissionError('exact webcam permission reservation required')
            else:self.custodian.bind(self.session,context)
            self.context=context;self.completed=False;self.cleanup_completion=None
    def request_stop(self):
        with self._lock:
            self.custodian.retain(self.session)
            if self.context is None:return False
            try:
                p=self.consumer.begin(self.context)
                control=dict(type='camera_stop_challenge',**correlation(p));bounded_control(control)
                self._queued=control;self._proof=None;self._ack=None;self.ack_sent=False;self.ack_confirmed=False
                return True
            except (PermissionError,ValueError,OverflowError):return False
    def authority_revoked(self,reason='authority_revoked'):
        with self._lock:
            route=getattr(self.session,'webcam_route',None)
            if route is not None:route.revoke()
            return self.request_stop() if self.context is not None else False
    def pop_control(self):
        with self._lock:
            value=self._queued;self._queued=None;return value
    def handle_control(self,value):
        with self._lock:
            # Audio-session authentication proves only admitted channel origin.
            # Separate camera-owner adapter must verify report provenance.
            self.session._authorized()
            if type(value) is not dict:return None
            if value.get('type')=='camera_stop_acknowledged':
                if self._ack is None or not self.ack_sent:return None
                expected=dict(self._ack,type='camera_stop_acknowledged')
                if set(value)!=set(expected) or any(type(value[k]) is not type(expected[k]) for k in expected) or value!=expected:return None
                self.confirm_retained_local_ack(value)
                return None
            if value.get('type')!='camera_driver_stop':return None
            try:report=DriverStopReport.parse(bounded_control(value))
            except (ValueError,TypeError):return None
            return self._observe_report(report)
    def _observe_report(self,report):
        before=self.consumer.driver_serial
        try:proof=self.consumer.observe(self.session,report)
        except (PermissionError,ValueError):return None
        if not self.consumer.driver_closed or self.consumer.driver_serial<=before:return None
        p=self.consumer.pending
        self._proof=proof;self.ack_sent=False;self.ack_confirmed=False
        ack=dict(type='camera_stop_ack',**correlation(p),report_serial=str(report.report_serial));bounded_control(ack)
        self._ack=ack
        return ack # acknowledgment never grants capture or proves physical Off
    def handle_retained_local_report(self,report):
        """Trusted adapter recovery after heartbeat expiry/connection closure.
        Exact retained context and origin qualifier still required. This cannot
        admit media, authenticate a replacement socket, or issue a new purpose.
        """
        with self._lock:
            if type(report) is not DriverStopReport:return None
            return self._observe_report(report)
    def confirm_retained_local_ack(self,opaque_receipt):
        with self._lock:
            p=self.consumer.pending
            if p is None or self._ack is None:return False
            receipt=self._owner('verify_retained_ack_receipt',p,self._ack,opaque_receipt)
            if type(receipt) is not RetainedAckReceipt or receipt.challenge is not p or receipt.report_serial!=int(self._ack['report_serial']):return False
            self.ack_sent=True;self.ack_confirmed=True
            return self._complete_if_qualified()
    def control_sent(self,value):
        with self._lock:
            if self._ack is not None and value==self._ack:
                self.ack_sent=True
                return False # socket send completion is not producer ACK consumption
            return False
    def _complete_if_qualified(self):
        if not self.ack_sent or not self.ack_confirmed or self._proof is None or self.context is None or not self._barrier_verified(self.context):return False
        if not self.consumer.consume(self.session,self._proof):return False
        if not self.custodian.release(self.context,_RELEASE_SEAL):raise RuntimeError('consumed closure retained custody')
        self._proof=None;self.completed=True;self.cleanup_completion=object()
        return True
    def qualified_cleanup_matches(self,context,receipt):
        # Created only after sealed physical proof + genuine ACK consumption.
        with self._lock:return self.completed and context is self.context and receipt is not None and receipt is self.cleanup_completion
    def connection_lost(self):
        with self._lock:
            self.custodian.retain(self.session);self._queued=None;self._ack=None;self.ack_sent=False;self.ack_confirmed=False
    @property
    def physical_off(self):return self.completed # default false; only sealed physical consumption sets true
