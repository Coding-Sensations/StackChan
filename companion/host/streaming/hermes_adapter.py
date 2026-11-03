# SPDX-License-Identifier: MIT
"""Explicit API-owned Hermes turns; no startup, credentials lookup or native join.

The enclosing qualified controller supplies authority, a pinned TLS context and
credentials. Scope/JSON are correlation data, not authority. API sessions never
impersonate CLI/Desktop/native sessions. Operations are serialized and one-shot;
an uncertain write cannot be retried under a replacement operation automatically.
"""
from dataclasses import dataclass
import http.client,json,math,re,ssl,time,socket,ipaddress
from urllib.parse import urlsplit
_ID=re.compile(r'[A-Za-z0-9_.:-]{1,64}\Z')
def identifier(value):return type(value) is str and _ID.fullmatch(value) is not None
@dataclass(frozen=True)
class Scope:
    provider:str;profile:str;device:str;session:str;binding:str;generation:str;incarnation:str;operation:str;expires:float
    kind:str='api_owned'
class Refused(RuntimeError):pass
class Unknown(RuntimeError):pass
class UnavailableAuthority:
    """Only supplied production source; qualified native source stays external."""
    def current(self,scope,action):return False

def strict_json(data):
    def unique(pairs):
        result={}
        for key,value in pairs:
            if key in result:raise ValueError('duplicate JSON key')
            result[key]=value
        return result
    return json.loads(data,object_pairs_hook=unique,parse_constant=lambda _:(_ for _ in ()).throw(ValueError('nonfinite JSON')))

class _Budget:
    def __init__(self,guard,clock,remaining):
        self.guard,self.clock=guard,clock
        self.last=clock()
        duration=3. if remaining is None else remaining()
        if type(self.last) not in (int,float) or not math.isfinite(self.last) or type(duration) not in (int,float) or not math.isfinite(duration) or duration<=0:raise Refused('finite request budget required')
        self.end=self.last+min(3.,duration)
    def touch(self,sock=None):
        if self.guard() is not True:raise Refused('scope unavailable')
        now=self.clock()
        if type(now) not in (int,float) or not math.isfinite(now) or now<self.last or now>=self.end:raise TimeoutError('elapsed request deadline')
        self.last=now;left=self.end-now
        if sock is not None:sock.settimeout(left)
        return left
class _DeadlineFile:
    def __init__(self,fp,sock,budget):self.fp,self.sock,self.budget=fp,sock,budget;self.lines=0
    def read1(self,n):
        self.budget.touch(self.sock);value=self.fp.read1(n);self.budget.touch(self.sock);return value
    def readline(self,n=-1):
        limit=min(n if n>=0 else 8193,8193);result=bytearray()
        while len(result)<limit:
            value=self.read1(1)
            if not value:break
            result.extend(value);self.lines+=len(value)
            if self.lines>16384:raise ValueError('bounded HTTP framing required')
            if value==b'\n':return bytes(result)
        if len(result)>=8193:raise ValueError('bounded HTTP line required')
        return bytes(result)
    def read(self,n=-1):
        if not 0<=n<=65537:raise ValueError('bounded HTTP body required')
        result=bytearray()
        while len(result)<n:
            value=self.read1(min(4096,n-len(result)))
            if not value:break
            result.extend(value)
        return bytes(result)
    def close(self):return self.fp.close()
class _DeadlineSocket:
    def __init__(self,sock,budget):self.inner,self.budget=sock,budget
    def sendall(self,data):
        view=memoryview(data);offset=0
        while offset<len(view):
            self.budget.touch(self.inner);n=self.inner.send(view[offset:offset+4096]);self.budget.touch(self.inner)
            if type(n) is not int or n<=0:raise OSError('write unconfirmed')
            offset+=n
    def __getattr__(self,name):return getattr(self.inner,name)

class HTTPSWire:
    """Real stdlib HTTPS transport; no redirects, ambient proxies or retries.

    Python immutable header/library copies cannot be promised zeroized. This
    wipes its owned bytearray and drops local headers; never persists/logs them.
    A sent request may execute remotely after revoke. Report unknown, not undo.
    """
    def __init__(self,origin,credential,context,*,connection_factory=http.client.HTTPSConnection,clock=time.monotonic):
        u=urlsplit(origin)
        if (u.scheme!='https' or not u.hostname or u.username is not None or u.password is not None
            or u.path not in ('','/') or u.query or u.fragment or not u.hostname.isascii()):raise ValueError('explicit HTTPS origin required')
        try:peer=ipaddress.ip_address(u.hostname)
        except ValueError:raise ValueError('explicit numeric LAN peer required') from None
        networks=[ipaddress.ip_network(n) for n in ('127.0.0.0/8','10.0.0.0/8','172.16.0.0/12','192.168.0.0/16')]
        if peer.version!=4 or not any(peer in n for n in networks):raise ValueError('explicit numeric LAN peer required')
        port=u.port or 443
        if not 1<=port<=65535:raise ValueError('port required')
        if (not isinstance(context,ssl.SSLContext) or context.verify_mode!=ssl.CERT_REQUIRED
            or not context.check_hostname or context.minimum_version<ssl.TLSVersion.TLSv1_2):raise ValueError('owner-qualified TLS context required')
        if not isinstance(credential,(bytes,bytearray)) or not 16<=len(credential)<=256 or any(c<33 or c>126 for c in credential):raise ValueError('owner-supplied credential required')
        self._host,self._port,self._context,self._factory=u.hostname,port,context,connection_factory
        self._clock=clock;self._secret=bytearray(credential);self._pending=None;self._closed=False;self._busy=False
    def __repr__(self):return '<HermesHTTPSWire: credentials redacted>'
    def exchange(self,method,path,payload,operation,guard,*,remaining=None):
        if self._closed or self._busy or self._pending is not None:raise Refused('transport unavailable')
        if not callable(guard) or not identifier(operation):raise Refused('owned operation required')
        if not re.fullmatch(r'/p/[A-Za-z0-9_.:-]{1,64}/v1/runs(?:/[A-Za-z0-9_.:-]{1,64})?',path):raise Refused('unsupported route')
        base=path.endswith('/runs')
        if (method,payload is None,base) not in [('POST',False,True),('GET',True,False)]:raise Refused('unsupported operation')
        body=None if payload is None else json.dumps(payload,separators=(',',':'),allow_nan=False).encode()
        if body is not None and len(body)>6144:raise Refused('bounded request required')
        headers={'Authorization':'Bearer '+self._secret.decode('ascii'),'Accept':'application/json'}
        if body is not None:headers.update({'Content-Type':'application/json','Idempotency-Key':operation})
        sent=False;success=False;self._busy=True
        try:
            if guard() is not True:raise Refused('scope unavailable')
            budget=_Budget(guard,self._clock,remaining)
            self._pending=self._factory(self._host,self._port,context=self._context,timeout=budget.touch())
            # Numeric-only peer avoids an unbounded ambient DNS resolver. TCP and
            # TLS handshake each receive only the remaining elapsed budget.
            def connect_numeric(address,timeout,source_address=None):
                if address!=(self._host,self._port) or source_address is not None:raise Refused('peer changed')
                raw=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
                try:
                    budget.touch(raw);raw.connect(address);budget.touch(raw);return raw
                except BaseException:raw.close();raise
            self._pending._create_connection=connect_numeric
            class ScopedResponse(http.client.HTTPResponse):
                def __init__(self,sock,*args,**kwargs):
                    super().__init__(sock,*args,**kwargs)
                    self.fp=_DeadlineFile(self.fp,sock,budget)
            self._pending.response_class=ScopedResponse
            self._pending.connect()
            raw=getattr(self._pending,'sock',None)
            if raw is not None:self._pending.sock=_DeadlineSocket(raw,budget)
            budget.touch(raw)
            if guard() is not True:raise Refused('scope unavailable')
            sent=True;self._pending.request(method,path,body=body,headers=headers)
            if guard() is not True:raise Unknown('dispatch uncertain')
            response=self._pending.getresponse()
            if guard() is not True:raise Unknown('dispatch uncertain')
            if response.status not in (200,202):raise Unknown('request refused or unconfirmed')
            if response.getheader('Content-Type','').split(';')[0].strip().lower()!='application/json':raise Unknown('invalid response')
            length=response.getheader('Content-Length')
            if length is not None and (not length.isascii() or not length.isdecimal() or int(length)>65536):raise Unknown('invalid response')
            data=response.read(65537)
            if not guard() or len(data)>65536:raise Unknown('response unavailable')
            value=strict_json(data)
            if type(value) is not dict:raise Unknown('invalid response')
            success=True;return value
        except Refused:
            if sent:raise Unknown('dispatch uncertain') from None
            raise
        except BaseException:
            raise (Unknown('dispatch uncertain') if sent else Refused('connection unavailable')) from None
        finally:
            headers.clear();self._busy=False
            if self._pending is not None:
                try:self._pending.close()
                except BaseException:raise Unknown('transport cleanup unconfirmed') from None
                else:self._pending=None
            if success:
                try:valid=guard() is True
                except BaseException:valid=False
                if not valid:raise Unknown('scope changed during cleanup')
    def close(self):
        self._closed=True
        for i in range(len(self._secret)):self._secret[i]=0
        if self._pending is not None:
            try:self._pending.close()
            except BaseException:raise Unknown('transport cleanup unconfirmed') from None
            else:self._pending=None

class HermesAdapter:
    """One explicitly selected API-owned turn → existing authenticated room.

    Source.current is a privileged owner port, never an RPC allowed flag. No
    supplied source authorizes production use. Keep unknown owners until their
    cleanup is resolved; do not construct a replacement to retry their work.
    """
    def __init__(self,scope,source,wire,session,event_key,writer,clock=time.monotonic):
        if (not isinstance(scope,Scope) or scope.provider!='hermes' or scope.kind!='api_owned'
            or any(not identifier(getattr(scope,k)) for k in ['profile','device','session','binding','generation','incarnation','operation'])
            or (session.provider,session.binding,session.generation)!=('hermes',scope.binding,scope.generation)
            or type(event_key) is not bytes or len(event_key)<32 or not callable(writer)):raise ValueError('exact Hermes API-owned route required')
        now=clock()
        if type(now) not in (int,float) or not math.isfinite(now) or type(scope.expires) not in (int,float) or not math.isfinite(scope.expires) or not now<scope.expires<=now+120:raise ValueError('finite owner lifetime required')
        self.scope,self.source,self.wire,self.session=scope,source,wire,session
        self._key,self._writer,self._clock=event_key,writer,clock;self._last=now;self._closed=False;self._revision=1;self._spent=False;self._busy=False;self._run=None;self._known=False;self._sequence=0;self._stage='prepared';self._text='';self._guarding=False;self._closing=False;self._operations={scope.operation}
    def _guard(self,action):
        if self._closed or self._guarding:return False
        revision=self._revision;self._guarding=True
        try:
            ok=self.source.current(self.scope,action)
            now=self._clock()
        except BaseException:return False
        finally:self._guarding=False
        if (ok is not True or self._closed or revision!=self._revision or type(now) not in (int,float)
            or not math.isfinite(now) or not self._last<=now<self.scope.expires):return False
        self._last=now;return True
    def _fault(self,stage):
        self._stage=stage;self._text='';self.close(keep_stage=True)
    def _exchange(self,method,path,payload,action):
        if not self._guard(action):raise Refused('scope unavailable')
        return self.wire.exchange(method,path,payload,self.scope.operation,lambda:self._guard(action),remaining=lambda:self.scope.expires-self._clock())
    def submit_text(self,text):
        if self._closed or self._busy or self._spent or type(text) is not str or '\x00' in text:return False
        try:valid=1<=len(text.encode())<=1024
        except UnicodeError:return False
        if not valid:return False
        if not self._guard('text_submit'):self._fault('unavailable');return False
        self._spent=True;self._busy=True
        try:
            response=self._exchange('POST','/p/'+self.scope.profile+'/v1/runs',{'session_id':self.scope.session,'input':text},'text_submit')
            if not self._guard('text_submit') or type(response) is not dict or not identifier(response.get('run_id')) or response.get('status') not in ('started','queued','running'):raise Unknown('admission unconfirmed')
            self._run=response['run_id'];self._stage='submitted';return True
        except Refused:self._fault('unavailable');return False
        except BaseException:self._fault('unknown');return False
        finally:self._busy=False
    def _observe(self,kind):
        from companion_events import sign_packet
        self._sequence+=1
        packet=dict(version=1,provider='hermes',binding=self.scope.binding,generation=self.scope.generation,sequence=self._sequence,kind=kind,task=self._run)
        return self.session.receive(packet,sign_packet(self._key,packet)) is True
    def poll_once(self):
        if self._closed or self._busy or self._run is None or self._stage in ('completed','error','cancelled'):return False
        self._busy=True
        try:
            response=self._exchange('GET','/p/'+self.scope.profile+'/v1/runs/'+self._run,None,'activity')
            if (not self._guard('activity') or type(response) is not dict or response.get('object')!='hermes.run'
                or response.get('run_id')!=self._run or response.get('session_id')!=self.scope.session):raise Unknown('run correlation unavailable')
            status=response.get('status')
            if type(status) is not str:raise Unknown('invalid status')
            kind={'queued':'working','running':'working','completed':'completed','failed':'error','cancelled':'cancelled','interrupted':'cancelled'}.get(status)
            if kind is None:self._fault('unavailable');return False
            text=response.get('output','') if kind=='completed' else ''
            if type(text) is not str or len(text.encode())>4096:raise Unknown('bounded reply required')
            # Existing CompanionSession requires a known task. Bootstrap its local
            # state from this confirmed run; publish only the final snapshot, so a
            # fast terminal receipt never emits invented in-progress activity.
            if not self._known:
                if not self._observe('working'):raise Unknown('room binding unavailable')
                self._known=True
            if kind!='working' and not self._observe(kind):raise Unknown('room binding unavailable')
            if not self._guard('activity'):raise Unknown('scope changed')
            self._stage={'working':'running','completed':'completed','error':'error','cancelled':'cancelled'}[kind];self._text=text
            self._writer()
            if not self._guard('activity'):raise Unknown('scope changed')
            return True
        except Refused:self._fault('unavailable');return False
        except BaseException:self._fault('unknown');return False
        finally:self._busy=False
    def close(self,keep_stage=False):
        if self._closing:return False
        self._closing=True
        try:
            self._closed=True;self._revision+=1;self.session.disconnect()
            if not keep_stage:self._stage='closed'
            self._text=''
            try:self.wire.close()
            except BaseException:self._stage='cleanup_pending';return False
            return True
        finally:self._closing=False
    def next_turn(self,scope):
        """Explicit fresh turn capture, same exact API route; no network/auto retry.

        A new native owner grant and operation are required. Terminal-only
        advancement keeps the signed room sequence and API session continuous.
        Unknown/closed owners cannot be renewed or silently replaced here.
        """
        if self._closed or self._busy or self._guarding or self._stage not in ('completed','error','cancelled') or len(self._operations)>=64:return False
        if (not isinstance(scope,Scope) or any(getattr(scope,k)!=getattr(self.scope,k) for k in ['provider','profile','device','session','binding','generation','incarnation','kind'])
            or not identifier(scope.operation) or scope.operation in self._operations):return False
        revision=self._revision;self._busy=self._guarding=True
        try:
            before=self._clock()
            valid=self.source.current(scope,'text_prepare')
            after=self._clock()
            if (valid is not True or self._closed or revision!=self._revision
                or any(type(n) not in (int,float) or not math.isfinite(n) for n in [before,after,scope.expires])
                or not self._last<=before<=after<scope.expires<=before+120):return False
            if self._stage=='error' and not self.session.acknowledge_error(self._run):return False
            self.scope=scope;self._last=after;self._operations.add(scope.operation)
            self._spent=False;self._run=None;self._known=False;self._stage='prepared';self._text=''
            return True
        except BaseException:return False
        finally:self._busy=self._guarding=False
    def result(self):
        if not self._closed and not self._guard('activity'):self._fault('unavailable')
        return {'stage':self._stage,'native':False,'text':self._text}
