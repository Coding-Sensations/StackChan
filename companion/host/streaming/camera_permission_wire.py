"""Camera-purpose exchange on the existing admitted paired TLS session.

Local worker pipe creates Mac intent. Only a current paired device reply to that
exact pending intent supplies device permission. This is purpose protocol over
existing endpoint identity; no audio grant, wire boolean, or capture qualification.
"""
from dataclasses import dataclass
from collections import deque
import secrets, math
from camera_host_dispatcher import ExistingSessionCameraDispatcher, bounded, request_id, decimal, boot_id
from camera_authority_ledger import LocalCameraAuthorityLedger
from camera_stop_dispatcher import ExistingSessionStopDispatcher
from camera_stop_evidence import RetainedStopContext,StopBinding
from clock_mapping import ClockOwner, ident

BASE={'version','request_id','nonce','device_id','conversation_id','turn_id','turn_generation','audio_epoch','mode','ttl_seconds'}
GRANT={'generation','epoch','authority_id','boot_id','expires_ms'}

@dataclass(frozen=True)
class _MacChoice:
    session: object
    proposal: dict
    deadline: float

@dataclass(frozen=True)
class _PairedDeviceChoice:
    mac: _MacChoice
    owner: ClockOwner
    authority_id: int
    boot: str

class PairedSessionCameraPermissionDispatcher(ExistingSessionCameraDispatcher):
    def __init__(self,session,custodian,*,clock=None,nonce=None):
        kwargs={} if clock is None else {'clock':clock}
        super().__init__(session,custodian,**kwargs)
        self._mac=None;self._device=None;self._wire=deque();self._notices=deque();self._proposal_ids=set();self._proposal_nonces=set();self._adopted=None;self._context=None;self._cleanup_choice=None
        self._proposal_nonce=nonce or (lambda:secrets.token_hex(16))
        # Actual owner callbacks consume private choice records produced by the
        # local command and current admitted TLS handler below. No external bool.
        # Gateway-owned custody keeps ledger and old session identities alive
        # across route replacement; creating a new session cannot reset replay.
        if not hasattr(custodian,'camera_permission_ledger'):
            custodian.camera_permission_ledger=LocalCameraAuthorityLedger(
                ui_confirmation=lambda s,*args:PairedSessionCameraPermissionDispatcher._consumer(s)._mac_current(s,*args),
                device_confirmation=lambda s,*args:PairedSessionCameraPermissionDispatcher._consumer(s)._device_current(s,*args),
                cleanup_confirmation=lambda s,*args:PairedSessionCameraPermissionDispatcher._consumer(s)._checked_cleanup(s,*args),
                clock=self.clock)
        self.backend=custodian.camera_permission_ledger
        stop=getattr(session,'camera_stop_dispatcher',None)
        self._physical_stop_owner=getattr(stop,'owner',None)
        if stop is None:stop=ExistingSessionStopDispatcher(session,self,custodian)
        else:stop.owner=self
        session.camera_stop_dispatcher=stop

    @staticmethod
    def _consumer(session):
        d=getattr(session,'camera_dispatcher',None)
        if not isinstance(d,PairedSessionCameraPermissionDispatcher) or d.session is not session:
            raise PermissionError('exact current source camera owner required')
        return d

    def _can_adopt_permission(self,mode):
        return self._context is not None and self.custodian.can_adopt_reserved(self.session,self._context,mode)

    def permits_media_reservation(self,context,mode):
        # Permission-only source hook; physical timing/capture gates stay separate.
        try:
            a=self.require_current();o=a.owner;b=context.binding
            return (mode=='webcam' and a.mode==mode and context is self._context and context.session is self.session and
                    (b.device,b.conversation,b.turn,b.generation,b.epoch,context.boot,context.authority_id)==
                    (o.device_id,o.conversation_id,o.turn_id,o.camera_generation,o.camera_epoch,a.producer_boot_id,a.authority_id))
        except Exception:return False

    def is_current_cleanup(self,context):
        a=self.backend.retained_authority(self.session)
        d=self._cleanup_choice
        return (context is self._context and context.session is self.session and d is not None and
                self.custodian.barrier_held(context) and context.authority_id==d.authority_id and context.boot==d.boot and
                (a is None or (context.authority_id==a.authority_id and context.boot==a.producer_boot_id)))

    def context_for_stop(self,session,purpose):
        return self._context if session is self.session else None

    def _physical(self,name,*args):
        method=getattr(self._physical_stop_owner,name,None)
        return method(*args) if callable(method) else None

    # Only the existing qualified physical owner can supply these receipts.
    # Paired TLS/purpose confirmation never fabricates driver/DMA qualification.
    def driver_report_origin(self,*a):return self._physical('driver_report_origin',*a)
    def qualify_physical_stop(self,*a):return self._physical('qualify_physical_stop',*a)
    def cleanup_barrier_receipt(self,*a):return self._physical('cleanup_barrier_receipt',*a)
    def verify_retained_ack_receipt(self,*a):return self._physical('verify_retained_ack_receipt',*a)

    def _checked_cleanup(self,session,authority,receipt):
        stop=session.camera_stop_dispatcher
        return session is self.session and self.backend.retained_authority(session) is authority and stop.qualified_cleanup_matches(self._context,receipt)

    def _mac_current(self,session,owner,mode,request):
        m=self._mac
        return m is not None and m.session is session and self._same_turn(m) and self._now()<m.deadline and mode==m.proposal['mode'] and request==m.proposal['request_id'] and owner==self._device.owner

    def _device_current(self,session,owner,mode,boot):
        d=self._device
        return d is not None and d.mac is self._mac and d.mac.session is session and d.owner==owner and d.boot==boot and mode==d.mac.proposal['mode'] and self._same_turn(d.mac)

    def _same_turn(self,m):
        try:
            self._session();p=m.proposal;t=self.session.turn
            return self.session is m.session and self.session.armed is True and self.session.epoch==int(p['audio_epoch']) and (self.session.device_id,self.session.conversation.id,t.id,t.generation)==(p['device_id'],p['conversation_id'],p['turn_id'],int(p['turn_generation']))
        except Exception:return False

    def _notice(self,request,reason):
        v=self._status(request)
        if not v['permission_current']:v['reason']=reason
        self.reason=reason
        if len(self._notices)>=2:self._notices.clear()
        self._notices.append(v);return v

    def pop_notice(self):return self._notices.popleft() if self._notices else None

    def notice_delivered(self,value):
        if self._notices and self._notices[0] is value:self._notices.popleft()

    def _enqueue(self,v):
        bounded(v)
        if len(self._wire)>=2:raise BufferError('bounded purpose control queue')
        self._wire.append(v)

    def handle_local_request(self,v):
        if type(v) is not dict:return None
        if v.get('type')=='camera_permission_off':
            if set(v)!={'type','version','request_id'} or type(v['version']) is not int or v['version']!=1 or not request_id(v['request_id']):raise ValueError('explicit local Off schema')
            self._cancel('explicit_local_camera_off')
            return self._notice(v['request_id'],'camera permission revoked; capture unavailable')
        if v.get('type')!='camera_permission_prepare':return super().handle_local_request(v)
        try:
            bounded(v);self._session()
            if set(v)!={'type','version','request_id','mode','ttl_seconds'} or type(v['version']) is not int or v['version']!=1 or not request_id(v['request_id']) or v['mode'] not in ('webcam','gestures') or type(v['ttl_seconds']) is not int or not 1<=v['ttl_seconds']<=60:
                raise ValueError('bounded explicit local purpose proposal')
            if self.session.armed is not True or self._mac is not None or self.authority is not None or self.backend.retained_authority(self.session) is not None or not self.custodian.can_admit(self.session,v['mode']):
                raise PermissionError('exact active session and clear retained custody required')
            if v['request_id'] in self._proposal_ids or len(self._proposal_ids)>=64:raise PermissionError('purpose proposal replay/budget')
            self._proposal_ids.add(v['request_id']);n=self._now();nonce=self._proposal_nonce()
            if not boot_id(nonce) or nonce in self._proposal_nonces:raise ValueError('fresh bounded proposal nonce')
            self._proposal_nonces.add(nonce)
            t=self.session.turn
            p=dict(type='camera_permission_proposal',version=1,request_id=v['request_id'],nonce=nonce,
                device_id=self.session.device_id,conversation_id=self.session.conversation.id,turn_id=t.id,
                turn_generation=str(t.generation),audio_epoch=str(self.session.epoch),mode=v['mode'],ttl_seconds=v['ttl_seconds'])
            self._mac=_MacChoice(self.session,p,n+v['ttl_seconds']);self._device=None
            self._enqueue(p)
            return self._notice(v['request_id'],'confirm this camera purpose on the current device; capture unavailable')
        except Exception:
            self._cancel('camera_proposal_rejected')
            return self._notice(v.get('request_id') if request_id(v.get('request_id')) else 'camera','camera proposal rejected; capture unavailable')

    def handle_control(self,v):
        if type(v) is not dict or v.get('type')!='camera_permission_confirmation':return super().handle_control(v)
        # Invoked exclusively by run_connection's current admitted socket/session.
        try:
            bounded(v);self._session();m=self._mac
            if m is None or not self._same_turn(m) or self._now()>=m.deadline:raise PermissionError('current fresh local Mac intent required')
            p=m.proposal
            if any(type(v.get(k)) is not type(p[k]) or v.get(k)!=p[k] for k in BASE):raise PermissionError('exact camera proposal correlation required')
            state=v.get('state')
            if state=='unavailable':
                if set(v)!={'type',*BASE,'state','reason'} or v['reason'] not in ('device_declined','camera_driver_unqualified','camera_owner_busy','device_permission_expired'):raise ValueError('bounded device denial')
                self._mac=self._device=None
                self._notice(p['request_id'],v['reason']);return None
            if state!='confirmed' or set(v)!={'type',*BASE,*GRANT,'state'}:raise ValueError('camera confirmation schema')
            if not boot_id(v['boot_id']):raise ValueError('current producer boot required')
            g=decimal(v['generation'],1);e=decimal(v['epoch'],1);aid=decimal(v['authority_id'],1);decimal(v['expires_ms'],1)
            if e!=self.session.epoch:raise PermissionError('exact camera/audio media epoch required')
            owner=ClockOwner(p['device_id'],p['conversation_id'],p['turn_id'],decimal(p['turn_generation'],1),g,e,p['nonce'])
            self._device=_PairedDeviceChoice(m,owner,aid,v['boot_id'])
            remaining=math.floor(m.deadline-self._now())
            if not 1<=remaining<=p['ttl_seconds']:raise PermissionError('fresh bounded expiry required')
            # Establish exact precommit cleanup custody before issuer consumption
            # or any fallible adoption. This private paired choice is not capture
            # qualification; it only permits cleanup of its acquired board lease.
            context=RetainedStopContext(self.session,StopBinding(owner.device_id,owner.conversation_id,owner.turn_id,g,e),v['boot_id'],aid,m.deadline)
            self._context=context;self._cleanup_choice=self._device
            self.session.camera_stop_dispatcher.context=context
            self.custodian.reserve_context(self.session,context)
            token=self.backend.issue_local(self.session,owner,p['mode'],aid,v['boot_id'],p['request_id'],remaining)
            status=super().handle_local_request(dict(type='camera_permission_request',version=1,request_id=p['request_id'],mode=p['mode'],ttl_seconds=remaining,opaque_authority=token))
            if not status['permission_current']:raise PermissionError('local issuer consumption denied')
            commit=dict(p,type='camera_permission_commit',remaining_seconds=remaining)
            for key in GRANT:commit[key]=v[key]
            self._enqueue(commit)
            self._notice(p['request_id'],'both camera choices confirmed; capture remains unqualified')
            self._adopted=p
            self._mac=self._device=None;return None
        except Exception:
            request=self._mac.proposal['request_id'] if self._mac is not None else 'camera'
            self._cancel('camera_confirmation_rejected');self._notice(request,'camera confirmation rejected; capture unavailable');return None

    def _cancel(self,reason):
        m=self._mac
        p=m.proposal if m is not None else self._adopted
        self._wire.clear();self._mac=self._device=None;self._adopted=None
        self.revoke(reason,revoke_outstanding=True)
        if self._context is not None:self.session.camera_stop_dispatcher.request_stop()
        # Cancellation is scoped to this purpose; no pairing or audio revocation.
        if p is not None:
            v=dict(p,type='camera_permission_cancel');self._enqueue(v)

    def connection_lost(self):
        self._wire.clear();self._mac=self._device=None
        self.revoke('paired_connection_lost',revoke_outstanding=True)
        if self._context is not None:self.session.camera_stop_dispatcher.request_stop()

    def pop_control(self):
        if self._wire:
            self._session();return self._wire.popleft()
        return super().pop_control()

    def tick(self):
        try:
            stop=self.session.camera_stop_dispatcher
            retained=self.backend.retained_authority(self.session)
            if stop.completed and (retained is None or self.backend.release_after_checked_stop(self.session,retained,stop.cleanup_completion)):
                self._context=None;self._cleanup_choice=None
            if self._mac is not None and (not self._same_turn(self._mac) or self._now()>=self._mac.deadline):
                request=self._mac.proposal['request_id'];self._cancel('camera_device_confirmation_expired');self._notice(request,'device camera confirmation expired')
            held=self.authority
            super().tick()
            if held is not None and self.authority is None:
                request=self._adopted['request_id'] if self._adopted is not None else 'camera'
                self._cancel('camera_permission_expired');self._notice(request,'camera permission expired; cleanup retained')
        except Exception:
            self._cancel('camera_purpose_clock_or_session_fault');self._notice('camera','camera permission revoked; cleanup retained')
