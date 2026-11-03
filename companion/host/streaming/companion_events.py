"""Provider-specific agent state and exact-request custody. No runtime IO.

A real authorized local hook/App Server owner must supply directional keys and
scopes, feed native events, and retain response callbacks. This module provisions
no credentials, installs no hooks, opens no listener, and grants no device/audio
permissions. Existing audio pairing is deliberately insufficient authority.

Independent implementation informed by pinned donor behavior requirements; see
PROVENANCE.md. No donor code, art or network security policy is copied.
"""
from dataclasses import dataclass
from collections import deque
import hashlib
import hmac
import inspect
import json
import math
import re
import time

PROVIDERS=frozenset(('stackchan','mira','hermes','muse','claude','codex'))
_ID=re.compile(r'[A-Za-z0-9_.:-]{1,64}\Z')
_DIGEST=re.compile(r'[a-f0-9]{64}\Z')

def _id(value):return type(value) is str and _ID.fullmatch(value) is not None

def _wire(packet):
    if type(packet) is not dict:raise ValueError('object required')
    body=json.dumps(packet,sort_keys=True,separators=(',',':'),allow_nan=False,ensure_ascii=True).encode()
    if len(body)>2048:raise ValueError('bounded event required')
    return b'stackchan-provider-events-v1\0'+body

def sign_packet(key,packet):
    """For an already authorized bridge/peer; never generates or loads a key."""
    if type(key) is not bytes or len(key)<32:raise ValueError('authorized key required')
    return hmac.new(key,_wire(packet),hashlib.sha256).hexdigest()

@dataclass(frozen=True)
class RequestIdentity:
    provider:str
    binding:str
    generation:str
    task:str
    request_id:str
    operation_sha256:str

@dataclass
class _Request:
    deadline:float
    respond:object
    state:str='pending'
    summary:str=''

class CompanionSession:
    """One selected provider/binding/generation with bounded, ordered messages.

    The caller owns authorization/enrollment, serialization onto this session's
    event loop, transport cleanup and the real policy-enforced response channel.
    A signed activity message can never register an executable approval callback.
    Request callbacks are local custody only; no request content or secrets appear
    in the room projection. Submission is not an authoritative acknowledgement.
    """
    def __init__(self,provider,binding,generation,event_key,decision_key,*,scopes,clock=time.monotonic):
        if provider not in PROVIDERS or not _id(binding) or not _id(generation):raise ValueError('provider identity required')
        if any(type(k) is not bytes or len(k)<32 for k in (event_key,decision_key)) or event_key==decision_key:raise ValueError('distinct directional keys required')
        if not set(scopes)<= {'activity','approval'}:raise ValueError('unsupported scope')
        self.provider,self.binding,self.generation=provider,binding,generation
        self._event_key,self._decision_key=event_key,decision_key
        self._scopes=frozenset(scopes);self._clock=clock;self._sequence={"activity":0,"approval":0};self._active=True
        self._tasks={};self._completed=[];self._requests={};self._retired=deque(maxlen=64);self._observed=False;self._unconfirmed_activity=False

    def _identity(self,identity):
        return (isinstance(identity,RequestIdentity) and
                (identity.provider,identity.binding,identity.generation)==(self.provider,self.binding,self.generation) and
                _id(identity.task) and _id(identity.request_id) and type(identity.operation_sha256) is str and
                _DIGEST.fullmatch(identity.operation_sha256) is not None)

    def _expire(self):
        now=self._clock()
        for request in self._requests.values():
            if request.state=='pending' and now>=request.deadline:
                request.state='expired';request.respond=None

    def receive(self,packet,signature):
        try:
            if not self._active or type(packet) is not dict:return False
            kind=packet.get('kind')
            fields={'version','provider','binding','generation','sequence','kind','task'}
            if kind=='decision':fields|={'request_id','operation_sha256','choice'}
            if set(packet)!=fields or packet['version']!=1 or type(packet['version']) is not int:return False
            if (packet['provider'],packet['binding'],packet['generation'])!=(self.provider,self.binding,self.generation):return False
            scope='approval' if kind=='decision' else 'activity'
            if not _id(packet['task']) or type(packet['sequence']) is not int or not self._sequence[scope]<packet['sequence']<2**63:return False
            if scope not in self._scopes:return False
            key=self._decision_key if kind=='decision' else self._event_key
            if type(signature) is not str or _DIGEST.fullmatch(signature) is None or not hmac.compare_digest(signature,sign_packet(key,packet)):return False
            self._expire()
            if kind=='decision':
                identity=RequestIdentity(self.provider,self.binding,self.generation,packet['task'],packet['request_id'],packet['operation_sha256'])
                if not self._identity(identity) or packet['choice'] not in ('approve','deny'):return False
                request=self._requests.get(identity)
                if request is None or request.state!='pending':return False
                # Retire before the external call: uncertain submission never retries.
                self._sequence[scope]=packet['sequence'];request.state='submitted'
                callback=request.respond;request.respond=None
                try:
                    if callback(packet['choice']) is not None:raise ValueError('submission must return no fabricated receipt')
                except Exception:
                    request.state='uncertain';return False
                return True
            if kind not in ('working','response_stopping','completed','error','cancelled'):return False
            task=packet['task'];previous=self._tasks.get(task)
            if previous in ('completed','error'):return False
            if previous is None and (task in self._retired or kind!='working' or len(self._tasks)>=16):return False
            if kind=='completed' and len(self._completed)>=64:return False
            self._sequence[scope]=packet['sequence'];self._tasks[task]=kind;self._observed=True
            if kind in ('completed','cancelled'):
                if kind=='completed':self._completed.append(task)
                self._tasks.pop(task,None);self._retired.append(task)
            if kind in ('completed','error','cancelled'):
                for identity,request in self._requests.items():
                    if identity.task==task and request.state=='pending':request.state='cancelled';request.respond=None
            return True
        except (ValueError,TypeError,KeyError,OverflowError):return False

    def register_request(self,identity,*,deadline,respond,summary=None):
        """Only the actual local waiting request owner calls this privileged API.

        deadline is the authority's monotonic request expiry, never a conversation
        timeout. Keep up to 128 exact transaction records for this generation;
        capacity refusal leaves the desktop's existing approval flow in control.
        """
        if not self._active or 'approval' not in self._scopes:raise PermissionError('separate approval authority required')
        if not self._identity(identity) or self._tasks.get(identity.task)!='working':raise ValueError('current request task required')
        if type(deadline) not in (int,float) or not math.isfinite(deadline) or deadline<=self._clock():raise ValueError('current request deadline required')
        if not callable(respond) or inspect.iscoroutinefunction(respond):raise ValueError('owned synchronous submission callback required')
        if any(old.request_id==identity.request_id for old in self._requests) or len(self._requests)>=128:raise ValueError('duplicate native request ID or full request custody')
        if summary is not None and (type(summary) is not str or not 1<=len(summary)<=160 or any(ord(c)<32 or ord(c)>126 for c in summary)):raise ValueError('reviewed bounded summary required')
        self._requests[identity]=_Request(deadline,respond,summary=summary or '')

    def cancel_request(self,identity):
        """Exact local issuer cancellation, never an implicit approve/deny."""
        self._expire();request=self._requests.get(identity)
        if not self._active or request is None or request.state!='pending':return False
        request.state='cancelled';request.respond=None;return True

    def replace_request(self,old,new,*,deadline,respond):
        """Explicit issuer replacement; independent parallel requests may coexist."""
        self._expire();request=self._requests.get(old)
        if not self._active or request is None or request.state!='pending':raise ValueError('current replaceable request required')
        if not self._identity(new) or old.task!=new.task:raise ValueError('same current task required')
        self.register_request(new,deadline=deadline,respond=respond)
        request.state='cancelled';request.respond=None

    def acknowledge_error(self,task):
        """Local issuer/UI acknowledges the old alert, not a claim of repair."""
        if not self._active or self._tasks.get(task)!='error':return False
        self._tasks.pop(task);self._retired.append(task);return True

    def retire_response(self,task):
        """Local owner releases an ended hook observation without claiming success."""
        if not self._active or self._tasks.get(task)!='response_stopping':return False
        self._expire()
        if any(identity.task==task and request.state in ('pending','submitted','uncertain') for identity,request in self._requests.items()):return False
        self._tasks.pop(task);self._retired.append(task);self._unconfirmed_activity=True
        return True

    def confirm_idle_receipt(self):
        """Privileged real source quiescence receipt, never an idle timer or pose."""
        self._expire()
        if not self._active or self._tasks or any(r.state in ('pending','submitted','uncertain') for r in self._requests.values()):return False
        self._unconfirmed_activity=False;self._observed=True;return True

    def acknowledge(self,identity):
        """Only an authoritative receipt from the retained local issuer calls this."""
        request=self._requests.get(identity)
        if not self._active or request is None or request.state!='submitted':raise ValueError('current submitted request required')
        request.state='acknowledged'

    def request_state(self,identity):
        self._expire();request=self._requests.get(identity)
        return request.state if request else 'absent'

    def presentation_request(self):
        self._expire()
        if not self._active or self.scene()!='decision_sign':return None
        for identity,request in self._requests.items():
            if request.state=='pending' and request.summary:
                return identity,request.summary,max(0,min(60000,int((request.deadline-self._clock())*1000))),request.deadline
        return None

    def resolve_request(self,identity):
        """Native issuer cleared the request; does not assert execution/allow."""
        request=self._requests.get(identity)
        if not self._active or request is None:return False
        if request.state=='pending':request.state='cancelled'
        elif request.state in ('submitted','uncertain'):request.state='resolved'
        else:return False
        request.respond=None;return True

    def scene(self):
        self._expire()
        if not self._active:return 'unknown'
        states={r.state for r in self._requests.values()}
        if 'uncertain' in states:return 'error'
        if states & {'pending','submitted'}:return 'decision_sign'
        if 'error' in self._tasks.values():return 'error'
        if 'working' in self._tasks.values():return 'desk_work'
        if 'response_stopping' in self._tasks.values():return 'waiting_receipt'
        if self._unconfirmed_activity:return 'unknown'
        return 'completion' if self._completed else ('idle_decoration' if self._observed else 'unknown')

    def drain_completion_records(self):
        """Publication custody only, not a device display/delivery receipt."""
        result=tuple(self._completed);self._completed.clear();return result

    def take_completion(self):
        if self.scene()!='completion':return ()
        result=tuple(self._completed);self._completed.clear()
        for task in result:self._tasks.pop(task,None);self._retired.append(task)
        return result

    def disconnect(self):
        self._active=False;self._completed.clear()
        for request in self._requests.values():
            if request.state=='pending':request.state='cancelled'
            elif request.state=='submitted':request.state='uncertain'
            request.respond=None


def project_hook(provider,payload):
    """Translate actual Codex/Claude native fields; never installs/runs a hook.

    Hook observations are not authenticated by their JSON fields. Only a real
    admitted local hook owner may authenticate/send the returned minimal event.
    PermissionRequest needs retained response custody, not this event projection.
    """
    if provider not in ('codex','claude') or type(payload) is not dict:return None
    session=payload.get('session_id');task=payload.get('turn_id' if provider=='codex' else 'prompt_id')
    if not _id(session) or not _id(task):return None
    name=payload.get('hook_event_name')
    if type(name) is not str:return None
    # Stop hooks run before other hooks may continue a response; they are not
    # authoritative turn-completed receipts and must not trigger celebration.
    events={'UserPromptSubmit':'working','PreToolUse':'working','Stop':'response_stopping'}
    if provider=='claude':events['StopFailure']='error'
    else:events['Interrupt']='cancelled'
    kind=events.get(name)
    if kind is None:return None
    return dict(provider=provider,session=session,task=task,kind=kind)

class ProviderSelection:
    """Switch exclusive device/event routing after hardware cleanup joins.

    joined_stop releases only this companion's transport/hardware ownership;
    it must not cancel unrelated background model tasks or change provider trust.
    """
    def __init__(self,selected):
        if selected not in PROVIDERS:raise ValueError('unknown provider')
        self.selected=selected
    def select(self,provider,joined_stop):
        if provider not in PROVIDERS:raise ValueError('unknown provider')
        if provider==self.selected:return True
        if joined_stop() is not True:return False
        self.selected=provider;return True


class NativeHookOwner:
    """Source adapter for a real, separately admitted Codex/Claude hook owner.

    An installer/bridge must bind one native session and supply its authorized
    event key and transport. Merely importing this class installs nothing. Each
    PermissionRequest must come from the actual waiting hook with its own response
    pipe/callback and a unique request ID for this binding generation. Callback
    submission is not an acknowledgement and never bypasses the coding agent's
    normal approval/sandbox policy. No prompts, paths or tool input are sent to
    the room: the full exact operation is hashed locally.
    """
    def __init__(self,session,native_session,event_key,send,*,native_protocol=None):
        protocol=native_protocol or session.provider
        if protocol not in ('claude','codex') or not _id(native_session) or not callable(send):raise ValueError('native hook source required')
        if type(event_key) is not bytes or len(event_key)<32:raise ValueError('authorized event key required')
        self._session=session;self._native_session=native_session;self._key=event_key;self._send=send;self._sequence=0;self._protocol=protocol

    def observe(self,payload):
        event=project_hook(self._protocol,payload)
        if event is None or event['session']!=self._native_session:return False
        return self._publish(event['kind'],event['task'])

    def _publish(self,kind,task):
        self._sequence+=1
        packet=dict(version=1,provider=self._session.provider,binding=self._session.binding,generation=self._session.generation,sequence=self._sequence,kind=kind,task=task)
        return self._send(packet,sign_packet(self._key,packet)) is True

    def observe_app_server(self,notification):
        # Called only by the owner of an admitted App Server connection, never
        # a public JSON ingress. Does not start a server or attach an old thread.
        if self._protocol!='codex' or type(notification) is not dict:return False
        method=notification.get('method');params=notification.get('params')
        if method not in ('turn/started','turn/completed') or type(params) is not dict or params.get('threadId')!=self._native_session:return False
        turn=params.get('turn')
        if type(turn) is not dict or not _id(turn.get('id')):return False
        status=turn.get('status')
        if method=='turn/started':kind='working' if status=='inProgress' else None
        else:kind={'completed':'completed','failed':'error','interrupted':'cancelled'}.get(status) if type(status) is str else None
        return self._publish(kind,turn['id']) if kind is not None else False

    def release_response(self,task):
        """Actual hook owner retires its old observation after it has ended.

        This does not cancel a model/background task or fabricate a completion.
        Source activity remains unknown until a genuine later idle receipt.
        """
        return self._session.retire_response(task)

    def open_app_server_request(self,request,*,operation_sha256,deadline,respond,summary=None):
        """Retain an actual bound JSON-RPC request, not a hook-shaped imitation.

        The real source owner must supply the digest of the full immutable item
        (including patch/command details), not just a redacted request summary.
        respond is the native transport's callback already bound to this request
        ID. No policy/session/permission amendments are synthesized here.
        """
        if self._protocol!='codex' or type(request) is not dict:raise ValueError('owned Codex App Server request required')
        method=request.get('method');params=request.get('params');rpc_id=request.get('id')
        if method not in ('item/commandExecution/requestApproval','item/fileChange/requestApproval') or type(params) is not dict:raise ValueError('unsupported native approval')
        if params.get('threadId')!=self._native_session or not _id(params.get('turnId')) or not _id(params.get('itemId')):raise ValueError('exact bound thread/turn/item required')
        decisions=params.get('availableDecisions',['accept','decline'])
        if type(decisions) is not list or 'accept' not in decisions or 'decline' not in decisions:raise ValueError('native decisions must permit one-shot accept/decline')
        if type(rpc_id) is int and -(2**63)<=rpc_id<2**63:request_id='rpc-i-'+str(rpc_id)
        elif type(rpc_id) is str and 0<len(rpc_id.encode())<=256:request_id='rpc-s-'+hashlib.sha256(rpc_id.encode()).hexdigest()[:56]
        else:raise ValueError('native request ID required')
        if not callable(respond) or inspect.iscoroutinefunction(respond):raise ValueError('native response owner required')
        identity=RequestIdentity(self._session.provider,self._session.binding,self._session.generation,params['turnId'],request_id,operation_sha256)
        def submit(choice):return respond({'decision':'accept' if choice=='approve' else 'decline'})
        self._session.register_request(identity,deadline=deadline,respond=submit,summary=summary)
        return identity

    def open_permission(self,payload,*,request_id,deadline,respond,summary=None):
        if type(payload) is not dict or payload.get('hook_event_name')!='PermissionRequest' or payload.get('session_id')!=self._native_session:raise ValueError('actual current permission hook required')
        task=payload.get('turn_id' if self._protocol=='codex' else 'prompt_id')
        name=payload.get('tool_name');inputs=payload.get('tool_input')
        if not _id(task) or not _id(name) or type(inputs) is not dict or not callable(respond) or inspect.iscoroutinefunction(respond):raise ValueError('exact tool operation and response owner required')
        operation=json.dumps(dict(provider=self._session.provider,native_protocol=self._protocol,session=self._native_session,task=task,tool=name,inputs=inputs),sort_keys=True,separators=(',',':'),allow_nan=False).encode()
        if len(operation)>65536:raise ValueError('bounded tool operation required')
        identity=RequestIdentity(self._session.provider,self._session.binding,self._session.generation,task,request_id,hashlib.sha256(operation).hexdigest())
        def submit(choice):
            response={'hookSpecificOutput':{'hookEventName':'PermissionRequest','decision':{'behavior':'allow' if choice=='approve' else 'deny'}}}
            return respond(response)
        self._session.register_request(identity,deadline=deadline,respond=submit,summary=summary)
        return identity
