"""Bounded RAM authority owner. Trusted local callbacks default absent.

No JSON ingress invokes issue_local. Deployment must supply real local UI and
current physical device confirmation, plus board-owned IDs/boot/binding. These
callbacks are an in-process trust boundary, not authentication or provenance.
"""
from dataclasses import dataclass
from threading import RLock
import secrets
import time
from camera_host_dispatcher import BorrowedCameraAuthority, MODE, boot_id, request_id
from clock_mapping import ClockOwner, finite

@dataclass
class _Record:
    session: object
    authority: BorrowedCameraAuthority
    token: str
    request: str
    consumed: bool = False
    revoked: bool = False

class LocalCameraAuthorityLedger:
    def __init__(self, *, ui_confirmation=None, device_confirmation=None,
                 cleanup_confirmation=None, clock=time.monotonic, token=None):
        self.ui_confirmation=ui_confirmation
        self.device_confirmation=device_confirmation
        self.cleanup_confirmation=cleanup_confirmation
        self.clock=clock
        self.token=token or (lambda: secrets.token_hex(32))
        self._lock=RLock(); self._records={}; self._used=set(); self._floor={}
        self._authority_ids=set(); self._last=None

    def _now(self):
        n=self.clock()
        if not finite(n) or not 0<=n<=2**32 or (self._last is not None and n<self._last):
            for r in self._records.values(): r.revoked=True
            raise PermissionError('monotonic clock invalid; cleanup retained')
        self._last=n
        return n

    @staticmethod
    def _session(session, owner):
        session._authorized()
        if session._active() is not True or (session.device_id,session.conversation.id,
            session.turn.id,session.turn.generation)!=(owner.device_id,owner.conversation_id,
            owner.turn_id,owner.turn_generation):
            raise PermissionError('exact admitted session/turn required')

    def issue_local(self, session, owner, mode, authority_id, producer_boot_id,
                    request, ttl_seconds=10):
        """Trusted local owner calls only after explicit UI and device confirmation.
        Board owner supplies authority_id; no audio grant, counter or wire flag does.
        Confirmation callbacks must return literal True for this exact tuple now.
        """
        with self._lock:
            if type(owner) is not ClockOwner or len(owner.device_id)>64 or mode not in MODE or not request_id(request):
                raise ValueError('bounded camera purpose required')
            if type(ttl_seconds) is not int or not 1<=ttl_seconds<=60:
                raise ValueError('short expiry required')
            self._session(session,owner); n=self._now()
            if owner.device_id in self._records or len(self._records)>=64 or len(self._used)>=64:
                raise PermissionError('retained cleanup or issuer budget blocks grant')
            if owner.camera_generation<=self._floor.get(owner.device_id,0):
                raise PermissionError('camera generation replay')
            if (owner.device_id,producer_boot_id,authority_id,owner.camera_generation,owner.camera_epoch) in self._authority_ids:
                raise PermissionError('board authority identity replay')
            # Validate the real owner tuple before asking the confirmation adapters.
            a=BorrowedCameraAuthority(owner,mode,authority_id,producer_boot_id,n+ttl_seconds,object())
            if not callable(self.ui_confirmation) or not callable(self.device_confirmation):
                raise PermissionError('trusted UI/device confirmation adapters absent')
            if self.ui_confirmation(session,owner,mode,request) is not True or self.device_confirmation(session,owner,mode,producer_boot_id) is not True:
                raise PermissionError('explicit current UI/device permission required')
            self._session(session,owner)
            if self._now()!=n:
                # Clock may advance; confirmation still bounded by original deadline.
                if self._last>=a.expires_at: raise PermissionError('confirmation expired')
            t=self.token()
            if type(t) is not str or len(t)!=64 or any(c not in '0123456789abcdef' for c in t) or t in self._used:
                raise PermissionError('unique bounded single-use token required')
            self._authority_ids.add((owner.device_id,producer_boot_id,authority_id,owner.camera_generation,owner.camera_epoch))
            self._used.add(t);self._floor[owner.device_id]=owner.camera_generation
            self._records[owner.device_id]=_Record(session,a,t,request)
            return t

    def consume_authority(self,session,token,mode,request):
        with self._lock:
            n=self._now(); r=self._records.get(session.device_id)
            if r is None: raise PermissionError('camera purpose absent')
            if r.session is not session or r.token!=token or r.consumed or r.revoked:
                raise PermissionError('single use exact session token required')
            r.consumed=True # valid token never reusable, including failed consumption
            self._session(session,r.authority.owner)
            if r.authority.mode!=mode or r.request!=request or n>=r.authority.expires_at:
                r.revoked=True;raise PermissionError('purpose mismatch/expired')
            return r.authority

    def current_authority(self,session,authority):
        with self._lock:
            n=self._now();r=self._records.get(session.device_id)
            if r is None or r.session is not session or r.authority is not authority: return None
            try: self._session(session,authority.owner)
            except Exception: r.revoked=True; return None
            if r.revoked or not r.consumed or n>=authority.expires_at:
                r.revoked=True;return None
            return authority

    def revoke_authority(self,session,authority):
        with self._lock:
            r=self._records.get(session.device_id)
            if r is not None and r.session is session and r.authority is authority: r.revoked=True

    def revoke_session_authority(self,session):
        """Explicit local Off cancels outstanding and consumed purposes alike.
        Exact session object prevents replacement sessions revoking retained owners.
        Cleanup is retained; no token can be consumed after this transition.
        """
        with self._lock:
            r=self._records.get(session.device_id)
            if r is not None and r.session is session: r.revoked=True

    def retained_authority(self,session):
        with self._lock:
            r=self._records.get(session.device_id)
            return r.authority if r is not None and r.session is session else None

    def release_after_checked_stop(self,session,authority,receipt):
        """Deployment owner verifies real retained Stop/ACK completion. Default deny.
        Does not certify physical silence. Expiry/reconnect cannot release custody.
        """
        with self._lock:
            r=self._records.get(session.device_id)
            if r is None or r.session is not session or r.authority is not authority or not r.revoked:
                return False
            if not callable(self.cleanup_confirmation) or self.cleanup_confirmation(session,authority,receipt) is not True:
                return False
            del self._records[session.device_id];return True

    # Permission cannot fabricate driver clock/capture provenance.
    def clock_receipt(self,*args): return None
    def capture_receipt(self,*args): return None
    def current_capture_receipt(self,*args): return None
