"""WCV1 full-video route within an existing admitted TLS session. No IO/issuer.

Wire fragmentation preserves existing WSS4112 limits and SCAV audio framing.
Authority and capture-clock mapping are supplied by the exact current owner;
wire fields, audio consent, reconnect and comparison numbers cannot grant them.
"""
from dataclasses import dataclass, field
import math, struct, time
HEADER=struct.Struct('>4sBBHHQQQQIIHHH')
MAX_MESSAGE=4112
FRAME_BYTES=320*240*4
MAX_U64=(1<<64)-1

def _id(value, maximum):
    return type(value) is str and 1<=len(value)<=maximum and all(c.isascii() and (c.isalnum() or c in '-_.:') for c in value)

def _u64(value):return type(value) is int and 1<=value<=MAX_U64

@dataclass(frozen=True)
class WebcamPurpose:
    device: str
    conversation: str
    turn: str
    generation: int
    epoch: int
    purpose: str='webcam'
    def __post_init__(self):
        if not (_id(self.device,64) and _id(self.conversation,128) and _id(self.turn,128) and _u64(self.generation) and _u64(self.epoch) and self.purpose=='webcam'):
            raise ValueError('exact distinct owner-issued webcam purpose required')

@dataclass(frozen=True)
class MappedCapture:
    host_time: float
    uncertainty: float

@dataclass(frozen=True)
class VideoFrame:
    purpose: WebcamPurpose
    sequence: int
    host_captured: float
    bgra: bytes=field(repr=False)
    # Original WCV1 producer time, never reconstructed from host/arrival time.
    # Missing only for backward-compatible synthetic construction; not emittable.
    producer_capture_ns: int | None = None

class WebcamAssembler:
    """One incomplete frame and latest1. Caller serializes on existing session.

    allocate is injectable to prove header/auth/clock checks precede allocation.
    Capture origin remains distinct from packet arrival and never gets replaced.
    """
    def __init__(self, session, current_purpose, map_capture_clock, *, clock=time.monotonic, allocate=bytearray):
        self.session=session;self.current=current_purpose;self.map_clock=map_capture_clock;self.clock=clock;self.allocate=allocate
        self.purpose=None;self.retired=(0,0);self.last_sequence=0;self.latest=None;self.assembly=None;self.meta=None;self.next_offset=0;self.deadline=0;self.last_clock=-1
    def revoke(self):
        if self.purpose:self.retired=max(self.retired,(self.purpose.generation,self.purpose.epoch))
        self.purpose=None;self.latest=None;self.assembly=None;self.meta=None;self.next_offset=0;self.deadline=0
    def _now(self):
        now=self.clock()
        if type(now) not in (int,float) or not math.isfinite(now) or now<0 or now<self.last_clock:
            self.revoke();raise PermissionError('monotonic clock unavailable')
        self.last_clock=now;return now
    def _authorized(self, now):
        try:self.session._authorized() # existing registry revocation + watchdog
        except BaseException:self.revoke();raise
        p=self.purpose
        if p is None or now>=self.deadline or not self.session._active() or not self.current(self.session,p):
            self.revoke();raise PermissionError('current webcam purpose unavailable')
        # Tuple must refer to the exact existing session turn; no mint-on-reconnect.
        t=self.session.turn
        if self.session.device_id!=p.device or self.session.conversation.id!=p.conversation or t.id!=p.turn or t.generation!=p.generation:
            self.revoke();raise PermissionError('existing owner mismatch')
    def explicit_on(self,purpose,*,local_intent):
        now=self._now()
        if type(purpose) is not WebcamPurpose or local_intent is not True or self.purpose is not None or (purpose.generation,purpose.epoch)<=self.retired:
            raise PermissionError('fresh explicit owner-issued Webcam On required')
        self.purpose=purpose;self.last_sequence=0;self.deadline=now+2
        try:self._authorized(now)
        except BaseException:self.revoke();raise
    def push(self,wire):
        now=self._now()
        try:
            self._authorized(now)
            if type(wire) is not bytes or not HEADER.size+3+1<=len(wire)<=MAX_MESSAGE:raise ValueError('bounded video fragment required')
            magic,version,fmt,width,height,generation,epoch,sequence,captured_ns,total,offset,dlen,clen,tlen=HEADER.unpack_from(wire)
            if magic!=b'WCV1' or version!=1 or fmt!=1 or width!=320 or height!=240 or total!=FRAME_BYTES or not _u64(generation) or not _u64(epoch) or not _u64(sequence) or not _u64(captured_ns) or not 1<=dlen<=64 or not 1<=clen<=128 or not 1<=tlen<=128:raise ValueError('video header rejected')
            start=HEADER.size;end=start+dlen+clen+tlen
            if not end<len(wire) or offset>=FRAME_BYTES or len(wire)-end>FRAME_BYTES-offset:raise ValueError('video length/offset rejected')
            # Bounded identifier slices only. No frame allocation until identity/time checked.
            try:
                device=wire[start:start+dlen].decode('ascii');start+=dlen
                conversation=wire[start:start+clen].decode('ascii');start+=clen
                turn=wire[start:start+tlen].decode('ascii')
            except UnicodeError:raise ValueError('video identity encoding') from None
            p=WebcamPurpose(device,conversation,turn,generation,epoch)
            if p!=self.purpose:raise PermissionError('foreign video purpose')
            mapped=self.map_clock(self.session,p,captured_ns)
            if type(mapped) is not MappedCapture or type(mapped.host_time) not in (int,float) or type(mapped.uncertainty) not in (int,float) or not math.isfinite(mapped.host_time) or not math.isfinite(mapped.uncertainty) or not 0<=mapped.uncertainty<=.025 or mapped.host_time>now or now-mapped.host_time+mapped.uncertainty>.25:
                raise PermissionError('trusted fresh producer clock unavailable')
            if sequence<=self.last_sequence:raise ValueError('video replay/exhaustion')
            meta=(p,sequence,captured_ns,mapped.host_time)
            if offset==0:
                if self.meta is not None and sequence<=self.meta[1]:raise ValueError('incomplete frame replay')
                # Frame start consumes sequence even when later fragments drop/abort.
                if self.meta is not None:self.last_sequence=max(self.last_sequence,self.meta[1])
                self.assembly=self.allocate(FRAME_BYTES);self.meta=meta;self.next_offset=0
            if self.assembly is None or self.meta!=meta or offset!=self.next_offset:raise ValueError('video fragment gap/mismatch')
            self._authorized(self._now()) # cancellation during mapping/allocation
            count=len(wire)-end;self.assembly[offset:offset+count]=memoryview(wire)[end:];self.next_offset+=count
            if self.next_offset==FRAME_BYTES:
                self.latest=VideoFrame(p,sequence,mapped.host_time,bytes(self.assembly),captured_ns);self.assembly=None;self.meta=None;self.next_offset=0;self.last_sequence=sequence;self.deadline=now+2
                return True
            return False
        except BaseException:
            # Reject/drop video only; clear media without enabling audio or touching its sequence.
            if self.meta:self.last_sequence=max(self.last_sequence,self.meta[1])
            self.assembly=None;self.meta=None;self.next_offset=0
            raise
    def take_latest(self):
        now=self._now();self._authorized(now)
        frame=self.latest;self.latest=None
        if frame and now-frame.host_captured<=.25:return frame
        return None

def encode_fragments(purpose,sequence,captured_ns,bgra):
    """Synthetic/device-contract encoder. No capture or IO."""
    if type(purpose) is not WebcamPurpose or not _u64(sequence) or not _u64(captured_ns) or type(bgra) is not bytes or len(bgra)!=FRAME_BYTES:raise ValueError('bounded full BGRA frame required')
    ids=purpose.device.encode()+purpose.conversation.encode()+purpose.turn.encode()
    size=MAX_MESSAGE-HEADER.size-len(ids)
    for offset in range(0,FRAME_BYTES,size):
        yield HEADER.pack(b'WCV1',1,1,320,240,purpose.generation,purpose.epoch,sequence,captured_ns,FRAME_BYTES,offset,len(purpose.device),len(purpose.conversation),len(purpose.turn))+ids+bgra[offset:offset+size]

@dataclass(frozen=True)
class CheckedStopReceipt:
    purpose: WebcamPurpose
    hardware_closed: bool # legacy driver observation only; not physical Off
    qualified_physical: object=None

class ExistingSessionWebcamRoute:
    """Hooks on the existing admitted native session. Default owner is missing.

    owner.issue_webcam_purpose must issue under existing authenticated authority,
    owner.is_current and map_capture_clock must validate that *same* issued tuple,
    owner.request_checked_stop must return/retain exact hardware stop custody.
    These methods do not exist in the inspected native owner: remain unavailable.
    """
    def __init__(self,session,owner=None,*,deliver=lambda frame:None,stop_evidence=None,stop_dispatcher=None,custodian=None):
        self.stop_dispatcher=stop_dispatcher;self.custodian=custodian
        self.session=session;self.owner=owner;self.deliver=deliver;self.stop_evidence=stop_evidence;self.assembler=None;self.stop_pending=False;self.pending_stop_purpose=None;self.stop_status="not_started";self.active_purpose=None;self.retired=(0,0);self.last_unavailable_status=None
    def explicit_on(self):
        self.session._authorized()
        if self.stop_pending or self.assembler is not None:raise PermissionError("camera cleanup/active purpose retains custody")
        if self.owner is None:raise PermissionError('camera purpose/clock/checked-stop owner bridge unavailable')
        if self.custodian is not None:
            if self.stop_dispatcher is None:raise PermissionError('camera cleanup dispatcher unavailable')
            self.stop_dispatcher.reserve_for_media()
        try:
            purpose=self.owner.issue_webcam_purpose(self.session,explicit_local_intent=True)
            if type(purpose) is not WebcamPurpose or (purpose.generation,purpose.epoch)<=self.retired:raise PermissionError("retired camera purpose")
            if self.stop_dispatcher is not None:self.stop_dispatcher.bind_active(purpose)
            assembler=WebcamAssembler(self.session,self.owner.is_current,self.owner.map_capture_clock,clock=self.session.clock)
            assembler.explicit_on(purpose,local_intent=True)
        except Exception:
            if self.custodian is not None:self.custodian.retain(self.session)
            if self.stop_dispatcher is not None:self.stop_dispatcher.request_stop()
            raise
        self.assembler=assembler;self.active_purpose=purpose;self.stop_status="pending";return purpose
    def camera_control(self,value):
        """Existing TLS/session correlation only, never camera authority/Stop proof."""
        self.session._authorized()
        if not self.session._active() or type(value) is not dict:return None
        kind=value.get('type');epoch=value.get('epoch');sequence=value.get('sequence');request=value.get('request_id')
        if type(epoch) is not int or not 1<=epoch<=0xffffffff or epoch!=self.session.epoch or type(sequence) is not str or not 1<=len(sequence)<=20 or not sequence.isascii() or not sequence.isdecimal() or sequence.startswith('0') or int(sequence)>MAX_U64 or not _id(request,32):return None
        if kind=='camera_status':
            expected={'type','epoch','sequence','request_id','ready','enabled','physical_off','state','reason'}
            if set(value)!=expected or any(value[k] is not False for k in ('ready','enabled','physical_off')) or value['state'] not in ('unavailable','stopping') or not _id(value['reason'],64):return None
            # Keep only bounded unavailable state/reason; no credential, token or pixels.
            self.last_unavailable_status=(value['state'],value['reason'])
            if self.assembler is not None:self.revoke()
            return None
        if kind not in ('camera_on','camera_off') or set(value) not in ({'type','epoch','sequence','request_id'},{'type','epoch','sequence','request_id','token'}):return None
        token=value.get('token')
        if token is not None and not _id(token,128):return None
        if kind=='camera_off':self.revoke()
        reason='revoked_physical_stop_unverified' if kind=='camera_off' else ('camera_token_invalid' if token is not None else 'camera_purpose_grant_missing')
        return dict(type='camera_status',epoch=epoch,sequence=sequence,request_id=request,ready=False,enabled=False,physical_off=False,state='stopping' if kind=='camera_off' else 'unavailable',reason=reason)

    def offer(self,wire):
        if self.assembler is None:return False # Never reinterpret WCV1 as audio.
        try:self.assembler.push(wire)
        except (ValueError,PermissionError,TimeoutError):return False
        return True
    def video_tick_after_audio(self):
        if self.assembler is None:return
        try:frame=self.assembler.take_latest()
        except (PermissionError,TimeoutError):self.revoke();return
        if frame is not None:self.deliver(frame) # Nonblocking, bounded existing pipe/sink adapter required.
    def revoke(self):
        assembler=self.assembler;self.assembler=None
        if assembler is None:
            if self.stop_pending and self.stop_dispatcher is not None:self.stop_dispatcher.request_stop()
            return
        p=self.active_purpose;self.active_purpose=None;assembler.revoke();self.stop_pending=True;self.stop_status="pending";self.pending_stop_purpose=p
        if p:self.retired=max(self.retired,(p.generation,p.epoch))
        # Missing/failed/uncertain receipt remains pending. Never report physical Off.
        try:
            if self.stop_dispatcher is not None:self.stop_dispatcher.request_stop()
            else:self.confirm_stop(self.owner.request_checked_stop(self.session,p))
        except Exception:self.stop_pending=True
    def confirm_stop(self,receipt):
        if type(receipt) is not CheckedStopReceipt or receipt.purpose!=self.pending_stop_purpose or self.stop_evidence is None:return False
        from camera_stop_evidence import QualifiedPhysicalStop,StopBinding
        p=receipt.purpose;expected=StopBinding(p.device,p.conversation,p.turn,p.generation,p.epoch)
        if type(receipt.qualified_physical) is not QualifiedPhysicalStop or receipt.qualified_physical.binding!=expected or not self.stop_evidence.consume(self.session,receipt.qualified_physical):return False
        self.stop_pending=False;self.stop_status="closed";self.pending_stop_purpose=None;return True

    def dispatch_stop_complete(self):
        d=self.stop_dispatcher
        if d is None or d.completed is not True or d.context is None or self.pending_stop_purpose is None:return False
        b=d.context.binding;p=self.pending_stop_purpose
        if (b.device,b.conversation,b.turn,b.generation,b.epoch)!=(p.device,p.conversation,p.turn,p.generation,p.epoch):return False
        self.stop_pending=False;self.stop_status="closed";self.pending_stop_purpose=None;return True
