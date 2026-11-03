"""RAM-only calibration consumer. No grant issuer/verifier, IO, camera or network.

Receipt callbacks consume already-authenticated dispatcher evidence and actual
producer/capture provenance; defaults deny. They do not perform authentication.
"""
from dataclasses import dataclass
import math
import secrets
from threading import RLock
import time

MAX64=(1<<64)-1
DOMAIN='esp-monotonic-ms-v1'

def ident(v):
    return type(v) is str and 1<=len(v)<=128 and all(c.isascii() and (c.isalnum() or c in '-_.:') for c in v)
def u64(v,minimum=0): return type(v) is int and minimum<=v<=MAX64
def finite(v): return type(v) in (int,float) and math.isfinite(v)

@dataclass(frozen=True)
class ClockOwner:
    device_id: str
    conversation_id: str
    turn_id: str
    turn_generation: int
    camera_generation: int
    camera_epoch: int
    owner_purpose_id: str
    def __post_init__(self):
        if not (all(ident(v) for v in (self.device_id,self.conversation_id,self.turn_id,self.owner_purpose_id))
                and all(u64(v,1) for v in (self.turn_generation,self.camera_generation,self.camera_epoch))):
            raise ValueError('exact borrowed current owner tuple required')

@dataclass(frozen=True)
class Challenge:
    owner: ClockOwner
    nonce: str

@dataclass(frozen=True)
class Response:
    challenge: Challenge
    boot_id: str
    clock_domain: str
    producer_receive_ms: int
    producer_send_ms: int

@dataclass(frozen=True)
class CaptureStamp:
    owner: ClockOwner
    boot_id: str
    clock_domain: str
    sequence: int
    captured_ms: int

@dataclass(frozen=True)
class Mapping:
    owner: ClockOwner
    boot_id: str
    clock_domain: str
    producer_anchor_ms: int
    host_lower: float
    host_upper: float
    maximum_drift_per_second: float
    valid_until: float
    def capture_bounds(self,stamp,now):
        if not (type(stamp) is CaptureStamp and stamp.owner==self.owner and stamp.boot_id==self.boot_id
                and stamp.clock_domain==self.clock_domain and u64(stamp.sequence,1) and u64(stamp.captured_ms)
                and finite(now) and self.host_upper<=now<self.valid_until):
            raise PermissionError('current same-domain capture and unexpired mapping required')
        delta=stamp.captured_ms-self.producer_anchor_ms # exact integer difference before float
        if abs(delta)>2000: raise ValueError('capture outside bounded calibration')
        seconds=delta/1000; drift=abs(seconds)*self.maximum_drift_per_second
        earliest=self.host_lower+seconds-drift-.001; latest=self.host_upper+seconds+drift+.001
        if earliest<0 or latest>now or now-earliest>=.5: raise TimeoutError('stale/future capture bounds')
        return earliest,latest

class ClockHandshake:
    def __init__(self,*,current_owner=lambda owner:False,accepted_clock_receipt=lambda response:False,
                 accepted_capture_receipt=lambda stamp:False,clock=time.monotonic,nonce=lambda:secrets.token_hex(16)):
        self.current_owner=current_owner; self.accepted_clock_receipt=accepted_clock_receipt
        self.accepted_capture_receipt=accepted_capture_receipt; self.clock=clock; self.nonce=nonce
        self.lock=RLock(); self.owner=None; self.boot=None; self.used=set(); self.pending=None
        self.anchor=None; self.mapping=None; self.last_now=None; self.capture_sequence=None; self.capture_ms=None; self.capture_stamp=None
    def invalidate(self):
        self.pending=self.anchor=self.mapping=None; self.capture_sequence=self.capture_ms=None; self.capture_stamp=None
        # Preserve nonce ledger and boot within this exact epoch to reject reuse/reset.
    def _now(self):
        n=self.clock()
        if not finite(n) or not 0<=n<=(1<<32) or (self.last_now is not None and n<self.last_now):
            self.invalidate(); raise ValueError('monotonic host clock required')
        self.last_now=n; return n
    def _current(self,owner):
        if type(owner) is not ClockOwner or self.current_owner(owner) is not True:
            self.invalidate(); raise PermissionError('existing current-owner evidence unavailable')
    def begin(self,owner):
        with self.lock:
            try:
                self._current(owner); n=self._now()
                if owner!=self.owner:
                    self.invalidate(); self.owner=owner; self.boot=None; self.used.clear()
                if self.pending is not None:
                    raise BufferError('one outstanding clock challenge only')
                if len(self.used)>=64: raise BufferError('epoch challenge budget exhausted')
                nonce=self.nonce()
                if type(nonce) is not str or len(nonce)!=32 or any(c not in '0123456789abcdef' for c in nonce) or nonce in self.used:
                    raise ValueError('fresh bounded nonce required')
                challenge=Challenge(owner,nonce); self.used.add(nonce); self.pending=(challenge,n)
                return challenge
            except Exception: self.invalidate(); raise
    def complete(self,response):
        with self.lock:
            try:
                n=self._now()
                if type(response) is not Response or self.pending is None: raise PermissionError('outstanding challenge required')
                challenge,h0=self.pending; self.pending=None
                self._current(challenge.owner)
                if response.challenge!=challenge or self.accepted_clock_receipt(response) is not True:
                    raise PermissionError('existing authenticated producer receipt required')
                if (not ident(response.boot_id) or response.clock_domain!=DOMAIN
                    or not u64(response.producer_receive_ms) or not u64(response.producer_send_ms)
                    or response.producer_send_ms<response.producer_receive_ms or n-h0>.1):
                    raise ValueError('bounded monotonic same-domain response required')
                if self.boot is not None and self.boot!=response.boot_id: raise ValueError('boot changed without new owner epoch')
                self.boot=response.boot_id
                processing=response.producer_send_ms-response.producer_receive_ms
                if processing>100: raise ValueError('producer processing bound')
                lower=h0+(processing/1000)/1.01-.001; upper=n+.001
                if lower<0 or upper<lower or upper-lower>.004: raise TimeoutError('uncertainty budget exceeded')
                anchor=(response.producer_receive_ms,response.producer_send_ms,lower,upper)
                if self.anchor is None:
                    self.anchor=anchor; self.mapping=None; return None
                p1,p2,l0,u0=self.anchor
                if response.producer_receive_ms<p2 or response.producer_send_ms<=p2: raise ValueError('producer clock reset/replay')
                elapsed_ms=response.producer_send_ms-p2
                if not 500<=elapsed_ms<=2000: raise ValueError('two separated bounded anchors required')
                elapsed=elapsed_ms/1000
                measured=max(abs(1-(lower-u0)/elapsed),abs(1-(upper-l0)/elapsed))
                if measured>.01: raise ValueError('drift proof exceeds bound')
                # Receipt callback must attest source clock stays within this bound over the lease.
                mapping=Mapping(challenge.owner,response.boot_id,DOMAIN,response.producer_send_ms,
                                lower,upper,.01,lower+2)
                self._current(challenge.owner)
                self.anchor=anchor; self.mapping=mapping; return mapping
            except Exception: self.invalidate(); raise
    def map_capture(self,stamp):
        with self.lock:
            try:
                n=self._now()
                if self.mapping is None: raise PermissionError('two-anchor mapping unavailable')
                self._current(self.mapping.owner)
                # Calibration does not validate V4L2 timestamps. Actual capture receipt is separate.
                if self.accepted_capture_receipt(stamp) is not True: raise PermissionError('physical capture provenance unavailable')
                if self.capture_sequence is not None and (stamp.sequence<=self.capture_sequence or stamp.captured_ms<self.capture_ms):
                    raise ValueError('capture replay/reset')
                bounds=self.mapping.capture_bounds(stamp,n)
                self._current(stamp.owner); self.capture_sequence=stamp.sequence; self.capture_ms=stamp.captured_ms; self.capture_stamp=stamp
                return bounds
            except Exception: self.invalidate(); raise

    def revalidate_capture(self,stamp):
        """Read-only repeat validation for fragments of the exact last accepted frame.

        The downstream assembler still rejects frame replay. This does not admit a
        new frame/sequence or renew calibration or camera authority.
        """
        with self.lock:
            try:
                n=self._now()
                if self.mapping is None or stamp!=self.capture_stamp:
                    raise PermissionError('exact last accepted capture required')
                self._current(self.mapping.owner)
                if self.accepted_capture_receipt(stamp) is not True:
                    raise PermissionError('physical capture provenance unavailable')
                bounds=self.mapping.capture_bounds(stamp,n)
                self._current(stamp.owner);return bounds
            except Exception:self.invalidate();raise

class ExactMillisecondClockCenter:
    """Low-level center-only helper, NOT the WebcamAssembler return contract.

    stamp_for_ns must look up an ALREADY independently attested complete capture
    stamp in the existing authenticated session dispatcher; it defaults absent.
    No issuer, receipt verifier, capture proof, timestamp, or purpose is created.
    """
    def __init__(self,handshake,*,stamp_for_ns=lambda session,purpose,ns:None):
        self.handshake=handshake;self.stamp_for_ns=stamp_for_ns
        self.last_stamp=None;self.last_mapping=None;self.last_host_time=None
    def __call__(self,session,purpose,captured_ns):
        with self.handshake.lock:
            if not u64(captured_ns) or captured_ns%1000000:
                raise ValueError('exact UInt64 millisecond-domain nanoseconds required; no rounding')
            stamp=self.stamp_for_ns(session,purpose,captured_ns)
            if (type(stamp) is not CaptureStamp or stamp.captured_ms!=captured_ns//1000000
                or stamp.owner.owner_purpose_id!=purpose):
                raise PermissionError('existing owner attested capture lookup unavailable')
            if stamp==self.last_stamp:
                if self.handshake.mapping is not self.last_mapping:
                    raise PermissionError('mapping changed during frame assembly')
                self.handshake.revalidate_capture(stamp)
                return self.last_host_time # Exact same Double for every fragment.
            lo,hi=self.handshake.map_capture(stamp)
            self.last_stamp=stamp;self.last_mapping=self.handshake.mapping;self.last_host_time=(lo+hi)/2
            return self.last_host_time

class ExistingAssemblerClockAdapter:
    """Actual WebcamAssembler map_clock consumer returning MappedCapture.

    Imports the existing target's exact types, never copies/mints a WebcamPurpose.
    stamp_for_ns is an absent-default lookup of already attested capture metadata
    from the existing session dispatcher, including its opaque owner purpose ID.
    """
    def __init__(self,handshake,*,stamp_for_ns=lambda session,purpose,ns:None):
        self.handshake=handshake;self.lookup=stamp_for_ns
        self.last_stamp=None;self.last_mapping=None;self.last_value=None
    def __call__(self,session,purpose,captured_ns):
        from webcam_transport import WebcamPurpose,MappedCapture
        with self.handshake.lock:
            if type(purpose) is not WebcamPurpose or not u64(captured_ns,1) or captured_ns%1000000:
                raise ValueError('exact existing purpose and integral ms-domain UInt64 nanoseconds required')
            session._authorized()
            if session._active() is not True:
                raise PermissionError('current existing turn unavailable')
            stamp=self.lookup(session,purpose,captured_ns)
            if type(stamp) is not CaptureStamp:
                raise PermissionError('existing attested capture lookup unavailable')
            o=stamp.owner;t=session.turn
            if (o.device_id!=purpose.device or o.device_id!=session.device_id
                or o.conversation_id!=purpose.conversation or o.conversation_id!=session.conversation.id
                or o.turn_id!=purpose.turn or o.turn_id!=t.id or o.turn_generation!=t.generation
                or o.camera_generation!=purpose.generation or o.camera_epoch!=purpose.epoch
                or purpose.purpose!='webcam' or stamp.captured_ms!=captured_ns//1000000):
                raise PermissionError('exact current session/camera capture tuple required')
            # Opaque owner purpose ID is correlated by the existing lookup/current-owner
            # receipt, not inferred from purpose.purpose, which is only a kind label.
            if stamp==self.last_stamp:
                if self.handshake.mapping is not self.last_mapping:
                    raise PermissionError('calibration changed during frame assembly')
                self.handshake.revalidate_capture(stamp)
                return self.last_value
            lo,hi=self.handshake.map_capture(stamp)
            self.last_stamp=stamp;self.last_mapping=self.handshake.mapping
            self.last_value=MappedCapture((lo+hi)/2,(hi-lo)/2)
            return self.last_value
