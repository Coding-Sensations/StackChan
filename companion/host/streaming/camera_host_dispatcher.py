"""Existing-session camera permission/clock/capture consumers. No issuer or IO.

Only already-authenticated external adapter objects establish authority. Raw wire
booleans and parse success cannot do so. Runtime adapter/physical qualifier defaults
absent. Permission consumption and clock calibration never open a camera.
"""
from dataclasses import dataclass
from collections import deque
import json,math,secrets,time
from clock_mapping import ClockOwner,ClockHandshake,Challenge,Response,CaptureStamp,DOMAIN,MAX64,ident,u64,finite

MODE=('webcam','gestures')
REQUEST_ID_MAX_BYTES=64
def request_id(value):return ident(value) and len(value.encode('ascii'))<=REQUEST_ID_MAX_BYTES
CTX=('device_id','conversation_id','turn_id','turn_generation','generation','epoch','purpose_id','boot_id')
PROBE_KEYS={'type','version','nonce',*CTX}
SAMPLE_KEYS=PROBE_KEYS|{'clock_domain','producer_receive_ms','producer_send_ms'}

def decimal(value,minimum=0):
    if (type(value) is not str or not 1<=len(value)<=20 or not value.isascii() or not value.isdecimal()
        or (len(value)>1 and value[0]=='0')):raise ValueError('canonical UInt64 decimal string required')
    number=int(value)
    if not minimum<=number<=MAX64:raise ValueError('UInt64 range')
    return number

def boot_id(v):return type(v) is str and len(v)==32 and all(c in '0123456789abcdef' for c in v)

def bounded(value):
    if type(value) is not dict:raise ValueError('control object required')
    raw=json.dumps(value,separators=(',',':'),allow_nan=False).encode()
    if len(raw)>2048:raise ValueError('bounded camera control required')
    return value

@dataclass(frozen=True)
class BorrowedCameraAuthority:
    owner: ClockOwner
    mode: str
    authority_id: int
    producer_boot_id: str
    expires_at: float
    opaque_reference: object
    def __post_init__(self):
        if (type(self.owner) is not ClockOwner or self.mode not in MODE or not u64(self.authority_id,1)
            or not boot_id(self.producer_boot_id) or not finite(self.expires_at) or self.expires_at<0
            or self.opaque_reference is None):raise ValueError('typed already-verified external authority required')

@dataclass(frozen=True)
class QualifiedProducerClockReceipt:
    authority: BorrowedCameraAuthority
    response: Response
    qualification_reference: object
    maximum_rate_deviation: float

@dataclass(frozen=True)
class QualifiedPhysicalCaptureReceipt:
    authority: BorrowedCameraAuthority
    stamp: CaptureStamp
    qualification_reference: object

class ExistingSessionCaptureProvider:
    """Metadata lookup consumes physical qualification supplied by existing driver owner.

    No metadata parser, timeout, sequence, timestamp or network flag generates proof.
    """
    def __init__(self,dispatcher):self.dispatcher=dispatcher;self.last=None
    def lookup(self,session,purpose,captured_ns):
        d=self.dispatcher;d.require_current()
        if session is not d.session or not u64(captured_ns,1) or captured_ns%1000000:
            raise PermissionError('exact current session/ms-domain timestamp required')
        if d.backend is None or d.physical_qualifier is None:return None
        receipt=d.backend.capture_receipt(session,d.authority,purpose,captured_ns,d.physical_qualifier)
        if type(receipt) is not QualifiedPhysicalCaptureReceipt:return None
        stamp=receipt.stamp
        if (receipt.authority is not d.authority or receipt.qualification_reference is not d.physical_qualifier
            or type(stamp) is not CaptureStamp or stamp.owner!=d.authority.owner
            or stamp.boot_id!=d.authority.producer_boot_id or stamp.clock_domain!=DOMAIN
            or stamp.captured_ms!=captured_ns//1000000 or not u64(stamp.sequence,1)):return None
        d.require_current();self.last=receipt;return stamp
    def accepts(self,stamp):
        try:
            self.dispatcher.require_current()
            r=self.last
            return (type(r) is QualifiedPhysicalCaptureReceipt and r.stamp==stamp
                and r.authority is self.dispatcher.authority
                and r.qualification_reference is self.dispatcher.physical_qualifier
                and self.dispatcher.backend.current_capture_receipt(self.dispatcher.session,r) is r)
        except Exception:return False

class ExistingSessionCameraDispatcher:
    """Attach as session.camera_dispatcher; trusted runtime backend remains absent.

    Backend contract: consume_authority(session,token,mode,request_id)->BorrowedCameraAuthority;
    current_authority(session,authority)->same authority object or None;
    clock_receipt(session,authority,response,physical_qualifier)->QualifiedProducerClockReceipt;
    capture_receipt(...)->QualifiedPhysicalCaptureReceipt; current_capture_receipt(...)->same object.
    These are boundary adapters to existing verified evidence, not cryptographic verifiers.
    """
    def __init__(self,session,custodian=None,*,backend=None,physical_qualifier=None,clock=time.monotonic,nonce=None):
        self.session=session;self.custodian=custodian;self.backend=backend;self.physical_qualifier=physical_qualifier
        self.clock=clock;self.authority=None;self.out=deque();self.bytes=0;self.last_now=None;self.request_ids=set();self.next_probe_at=None
        self.reason='existing issuer adapter and physical timing qualification absent'
        self.provider=ExistingSessionCaptureProvider(self)
        args=dict(current_owner=self._current_owner,accepted_clock_receipt=self._accept_clock,
                  accepted_capture_receipt=self.provider.accepts,clock=self._now)
        if nonce is not None:args['nonce']=nonce
        self.handshake=ClockHandshake(**args)
    def _now(self):
        n=self.clock()
        if not finite(n) or not 0<=n<=2**32 or (self.last_now is not None and n<self.last_now):
            self.revoke('host_clock_invalid');raise ValueError('monotonic time required')
        self.last_now=n;return n
    def _session(self):
        self.session._authorized()
        if self.session._active() is not True:raise PermissionError('current admitted turn required')
    def require_current(self):
        try:self._session()
        except Exception:
            self.revoke('session_authority_lost');raise
        a=self.authority;n=self._now()
        if type(a) is not BorrowedCameraAuthority or self.backend is None or not n<a.expires_at:
            self.revoke('camera_permission_expired_or_unavailable')
            raise PermissionError('unexpired external camera purpose unavailable')
        o=a.owner;t=self.session.turn
        if (o.device_id!=self.session.device_id or o.conversation_id!=self.session.conversation.id
            or o.turn_id!=t.id or o.turn_generation!=t.generation
            or self.backend.current_authority(self.session,a) is not a):
            self.revoke('current_camera_owner_lost');raise PermissionError('exact current external purpose unavailable')
        return a
    def _current_owner(self,owner):
        try:return self.require_current().owner==owner
        except Exception:return False
    def _accept_clock(self,response):
        try:
            a=self.require_current()
            if self.physical_qualifier is None:return False
            r=self.backend.clock_receipt(self.session,a,response,self.physical_qualifier)
            return (type(r) is QualifiedProducerClockReceipt and r.authority is a and r.response==response
                and r.qualification_reference is self.physical_qualifier
                and finite(r.maximum_rate_deviation) and 0<=r.maximum_rate_deviation<=.01
                and response.boot_id==a.producer_boot_id)
        except Exception:return False
    def _queue(self,value):
        bounded(value);size=len(json.dumps(value,separators=(',',':')).encode())
        if len(self.out)>=2 or self.bytes+size>4096:raise BufferError('camera control backpressure')
        self.out.append((value,size));self.bytes+=size
    def pop_control(self):
        if not self.out:return None
        try:self.require_current()
        except Exception:
            self.revoke('camera_outbound_authority_lost');return None
        value,size=self.out.popleft();self.bytes-=size;return value
    def revoke(self,reason='camera_permission_off',*,revoke_outstanding=False):
        held=self.authority
        if held is not None and self.backend is not None:
            revoke=getattr(self.backend,'revoke_authority',None)
            if callable(revoke):
                try:revoke(self.session,held)
                except Exception:reason='camera_owner_revoke_fault_cleanup_retained'
        if revoke_outstanding and self.backend is not None:
            revoke_pending=getattr(self.backend,'revoke_session_authority',None)
            if callable(revoke_pending):
                try:revoke_pending(self.session)
                except Exception:reason='camera_owner_revoke_fault_cleanup_retained'
        self.authority=None;self.provider.last=None;self.out.clear();self.bytes=0;self.reason=reason;self.next_probe_at=None
        stop=getattr(self.session,'camera_stop_dispatcher',None)
        if held is not None and stop is not None:
            stop.authority_revoked(reason) # Retains actual cleanup context; no closure acknowledgment.
        if hasattr(self,'handshake'):self.handshake.invalidate()
        # Physical stop is separately owned by retained Stop custodian, never acknowledged here.
    def readiness(self):
        try:
            self.require_current()
            m=self.handshake.mapping;n=self._now()
            return dict(permission_current=True,clock_ready=m is not None and m.host_upper<=n<m.valid_until,
                capture_ready=False,
                enabled=False,reason='physical capture admission/activation remains separate')
        except Exception:return dict(permission_current=False,clock_ready=False,capture_ready=False,enabled=False,reason=self.reason)
    def handle_control(self,value):
        # Network lane consumes clock receipts only; permission opt-in comes from Mac local worker.
        if type(value) is not dict or value.get('type')!='camera_clock_sample':return None
        return self._handle(value)
    def handle_local_request(self,value):
        if type(value) is not dict or value.get('type') not in ('camera_permission_request','camera_permission_off'):return None
        return self._handle(value)
    def _handle(self,value):
        kind=value.get('type') if type(value) is dict else None
        if kind not in ('camera_permission_request','camera_permission_off','camera_clock_sample'):return None
        try:
            bounded(value);self._session()
            if kind=='camera_permission_off':
                if set(value)!={'type','version','request_id'} or type(value['version']) is not int or value['version']!=1 or not request_id(value['request_id']):raise ValueError('bounded explicit Off required')
                self.revoke(revoke_outstanding=True);return self._status(value['request_id'])
            if kind=='camera_permission_request':return self._permission(value)
            return self._sample(value)
        except Exception as error:
            self.revoke('camera_control_rejected_'+type(error).__name__,revoke_outstanding=True)
            return self._status(value.get('request_id','camera') if request_id(value.get('request_id','camera')) else 'camera')
    def _status(self,request_id):return dict(type='camera_permission_status',version=1,request_id=request_id,**self.readiness())
    def _can_adopt_permission(self,mode):
        return self.custodian is not None and self.custodian.can_admit(self.session,mode) is True
    def _permission(self,v):
        if (set(v)!={'type','version','request_id','mode','ttl_seconds','opaque_authority'} or type(v['version']) is not int or v['version']!=1
            or not request_id(v['request_id']) or v['mode'] not in MODE or type(v['ttl_seconds']) is not int or not 1<=v['ttl_seconds']<=60
            or type(v['opaque_authority']) is not str or not 1<=len(v['opaque_authority'])<=1024):raise ValueError('explicit expiring permission request required')
        if v['request_id'] in self.request_ids or len(self.request_ids)>=64:raise PermissionError('local permission request replay/budget')
        self.request_ids.add(v['request_id'])
        self.revoke('camera_authority_unavailable')
        if self.backend is None:return self._status(v['request_id'])
        if not self._can_adopt_permission(v['mode']):
            self.reason='camera_stop_custody_or_admission_barrier_unavailable';return self._status(v['request_id'])
        a=self.backend.consume_authority(self.session,v['opaque_authority'],v['mode'],v['request_id'])
        if type(a) is not BorrowedCameraAuthority:
            raise PermissionError('typed consumed issuer receipt required')
        # Hold receipt custody before any clock/TTL/current-owner validation.
        # Rejection must revoke this exact consumed ledger object, never orphan it.
        self.authority=a
        n=self._now()
        if (a.mode!=v['mode'] or not n<a.expires_at<=n+v['ttl_seconds']):
            raise PermissionError('existing issuer current expiring receipt required')
        self.require_current()
        self.reason='permission consumed; physical timing qualification and capture admission pending'
        if self.physical_qualifier is not None:self.next_probe_at=n
        # Availability precheck is NOT reserve/bind; no camera start occurs here.
        return self._status(v['request_id'])
    def clock_probe(self):
        a=self.require_current();c=self.handshake.begin(a.owner)
        o=a.owner
        v=dict(type='camera_clock_probe',version=1,device_id=o.device_id,conversation_id=o.conversation_id,turn_id=o.turn_id,
            turn_generation=str(o.turn_generation),generation=str(o.camera_generation),epoch=str(o.camera_epoch),
            purpose_id=o.owner_purpose_id,boot_id=a.producer_boot_id,nonce=c.nonce)
        self._queue(v);self.next_probe_at=None;return c
    def _sample(self,v):
        if set(v)!=SAMPLE_KEYS or type(v['version']) is not int or v['version']!=1:raise ValueError('clock sample schema')
        a=self.require_current();o=a.owner
        expected=dict(device_id=o.device_id,conversation_id=o.conversation_id,turn_id=o.turn_id,
            turn_generation=str(o.turn_generation),generation=str(o.camera_generation),epoch=str(o.camera_epoch),purpose_id=o.owner_purpose_id,boot_id=a.producer_boot_id)
        if any(v[k]!=value for k,value in expected.items()) or not boot_id(v['boot_id']) or v['clock_domain']!=DOMAIN:
            raise PermissionError('clock tuple/boot/domain mismatch')
        for field in ('turn_generation','generation','epoch'):decimal(v[field],1)
        r=Response(Challenge(o,v['nonce']),v['boot_id'],v['clock_domain'],decimal(v['producer_receive_ms']),decimal(v['producer_send_ms']))
        mapped=self.handshake.complete(r)
        self.next_probe_at=(mapped.host_lower+1.3) if mapped is not None else self._now()+.6
        return dict(type='camera_clock_status',version=1,nonce=v['nonce'],clock_ready=mapped is not None,capture_ready=False,enabled=False,
            reason='calibration consumed; physical capture provenance and admission remain separate')

    def tick(self):
        """Existing bounded connection pump only; creates no background worker/task.

        Permission/qualifier absent => no probe. Recalibration renews no permission.
        """
        if self.authority is None:return
        try:
            self.require_current();n=self._now()
            if self.physical_qualifier is None:return
            if self.handshake.pending is not None and n-self.handshake.pending[1]>.1:
                self.handshake.invalidate();self.next_probe_at=n
            if self.handshake.mapping is not None and n>=self.handshake.mapping.valid_until:
                self.handshake.invalidate();self.next_probe_at=n
            if self.next_probe_at is not None and n>=self.next_probe_at:self.clock_probe()
        except Exception:self.revoke('camera_clock_or_permission_lost')

def attach_camera_host_consumer(session,custodian,**kwargs):
    dispatcher=ExistingSessionCameraDispatcher(session,custodian,**kwargs)
    session.camera_dispatcher=dispatcher;return dispatcher

class ExistingWorkerCameraCommands:
    """Actual RememberedWorker local-pipe adapter, never a network command ingress.

    Reads only that worker's current gateway._activity_session, already admitted
    by existing TLS/registry/session code. No session creation, credential access,
    permission issuer or camera activation. New sessions require a fresh request.
    """
    def __init__(self,worker):self.worker=worker
    def handle_command(self,value):
        if type(value) is not dict or value.get('type') not in ('camera_permission_request','camera_permission_off'):return None
        bounded(value)
        if type(value.get('version')) is not int or value['version']!=1 or not request_id(value.get('request_id')):
            raise ValueError('bounded local camera request identity required')
        if value['type']=='camera_permission_off':
            if set(value)!={'type','version','request_id'}:raise ValueError('exact local Off required')
        elif (set(value)!={'type','version','request_id','mode','ttl_seconds','opaque_authority'}
            or value['mode'] not in MODE or type(value['ttl_seconds']) is not int or not 1<=value['ttl_seconds']<=60
            or type(value['opaque_authority']) is not str or not 1<=len(value['opaque_authority'])<=1024):
            raise ValueError('exact local expiring camera request required')
        session=getattr(getattr(self.worker,'gateway',None),'_activity_session',None)
        dispatcher=getattr(session,'camera_dispatcher',None) if session is not None else None
        if type(dispatcher) is not ExistingSessionCameraDispatcher:
            return dict(type='camera_permission_status',version=1,request_id=value.get('request_id','camera'),
                permission_current=False,clock_ready=False,capture_ready=False,enabled=False,
                reason='current authenticated session camera consumer unavailable')
        return dispatcher.handle_local_request(value)
