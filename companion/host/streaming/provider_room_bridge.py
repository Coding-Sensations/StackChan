"""Owned native pipe → authenticated room state → exact response; no listeners.
Callers supply already authorized peer keys and actual native stream ownership.
No process launch, hook install, credential lookup or hardware action on import.
"""
import hashlib,hmac,json
from companion_events import sign_packet
ROOM=b'stackchan-provider-room-v1\0'
DECISION=b'stackchan-provider-decision-v1\0'
MAX_WIRE=2048

def _pairs(pairs):
 result={}
 for key,value in pairs:
  if key in result:raise ValueError('duplicate key')
  result[key]=value
 return result

def encode_wire(key,payload,domain):
 body=json.dumps(payload,sort_keys=True,separators=(',',':'),allow_nan=False,ensure_ascii=True).encode()
 tag=hmac.new(key,domain+body,hashlib.sha256).hexdigest().encode()
 wire=tag+b'\n'+body
 if len(wire)>MAX_WIRE:raise ValueError('bounded room wire required')
 return wire

def decode_wire(key,wire,domain):
 if type(wire) is not bytes or not 67<=len(wire)<=MAX_WIRE or wire[64:65]!=b'\n':raise ValueError('bounded authenticated frame required')
 tag,body=wire[:64],wire[65:]
 expected=hmac.new(key,domain+body,hashlib.sha256).hexdigest().encode()
 if not hmac.compare_digest(tag,expected):raise ValueError('room authentication failed')
 try:value=json.loads(body,object_pairs_hook=_pairs,parse_constant=lambda _:(_ for _ in ()).throw(ValueError('finite JSON required')))
 except (UnicodeError,json.JSONDecodeError):raise ValueError('bounded room JSON required') from None
 if type(value) is not dict:raise ValueError('room object required')
 return value

class RoomBridge:
 def __init__(self,session,event_key,decision_key,*,clock):
  if any(type(k) is not bytes or len(k)<32 for k in (event_key,decision_key)) or event_key==decision_key:raise ValueError('authorized directional keys required')
  self.session=session;self._event=event_key;self._decision=decision_key;self._clock=clock;self._sequence=0;self._decision_sequence=0;self._presented=None;self._presentation_signature=None;self._completion=False;self._completion_until=None
 def snapshot(self):
  self._sequence+=1
  if self._sequence>=2**53:raise ValueError('room generation exhausted')
  s=self.session
  if s.drain_completion_records():self._completion=True;self._completion_until=None
  scene=s.scene();now=self._clock()
  if self._completion and scene=='idle_decoration':
   if self._completion_until is None:self._completion_until=now+2.
   if now<self._completion_until:scene='completion'
   else:self._completion=False;self._completion_until=None
  elif scene not in ('completion','idle_decoration'):self._completion_until=None
  request=s.presentation_request();identity=None;summary='';ttl=0;deadline=None
  if scene=='decision_sign':
   if request is None:scene='waiting_receipt'
   else:identity,summary,ttl,deadline=request
  signature=(identity,summary,deadline) if identity is not None and ttl>0 else None
  ticket=(self._presented[0] if self._presented is not None and signature==self._presentation_signature else 'view-'+str(self._sequence)) if signature is not None else ''
  self._presented=(ticket,identity) if ticket else None;self._presentation_signature=signature
  payload=dict(version=1,provider=s.provider,binding=s.binding,generation=s.generation,sequence=self._sequence,
               scene=scene,summary=summary if ticket else '',request_id=identity.request_id if ticket else '',
               operation_sha256=identity.operation_sha256 if ticket else '',presentation_id=ticket,ttl_ms=ttl if ticket else 0)
  return encode_wire(self._event,payload,ROOM)
 def decision(self,wire):
  try:
   value=decode_wire(self._decision,wire,DECISION)
   if set(value)!={'version','provider','binding','generation','sequence','presentation_id','request_id','operation_sha256','choice'}:return False
   s=self.session
   if type(value['version']) is not int or value['version']!=1 or (value['provider'],value['binding'],value['generation'])!=(s.provider,s.binding,s.generation):return False
   seq=value['sequence']
   if type(seq) is not int or not self._decision_sequence<seq<2**53 or value['choice'] not in ('approve','deny') or self._presented is None:return False
   ticket,identity=self._presented
   if (value['presentation_id'],value['request_id'],value['operation_sha256'])!=(ticket,identity.request_id,identity.operation_sha256):return False
   self._decision_sequence=seq;self._presented=None
   inner=dict(version=1,provider=s.provider,binding=s.binding,generation=s.generation,sequence=seq,kind='decision',task=identity.task,
              request_id=identity.request_id,operation_sha256=identity.operation_sha256,choice=value['choice'])
   return s.receive(inner,sign_packet(self._decision,inner))
  except (ValueError,TypeError,KeyError):return False

class AppServerRoomOwner:
 """Consumes an already-owned App Server pipe; returns False for desktop handling.

 summary must faithfully describe the full item/scope without private leakage.
 deadline must come from the actual owner's authority/request lifetime. Neither
 is guessed here. Missing item/summary/lifetime leaves normal native UI in control.
 """
 def __init__(self,native_owner,room_bridge,writer,*,summary,clock,deadline=None):
  self.native=native_owner;self.room=room_bridge;self.writer=writer;self.summary=summary;self.clock=clock;self.deadline=deadline
  self._items={};self._requests={}
 def read_once(self,reader):
  line=reader.readline(65537)
  if not line:self.room.session.disconnect();return False
  if len(line)>65536:raise ValueError('native line too large')
  return self.handle(json.loads(line,object_pairs_hook=_pairs))
 def handle(self,message):
  if type(message) is not dict:return False
  method=message.get('method');params=message.get('params')
  if type(params) is not dict or params.get('threadId')!=self.native._native_session:return False
  if method in ('turn/started','turn/completed'):return self.native.observe_app_server(message)
  if method=='item/started':
   item=params.get('item');turn=params.get('turnId')
   if type(item) is not dict or item.get('type') not in ('commandExecution','fileChange') or type(item.get('id')) is not str or type(turn) is not str:return False
   if len(self._items)>=16:return False
   record=json.dumps(item,sort_keys=True,separators=(',',':'),allow_nan=False)
   if len(record.encode())>65536:return False
   self._items[(turn,item['id'])]=record;return True
  if method=='item/completed':
   item=params.get('item')
   if type(item) is not dict:return False
   self._items.pop((params.get('turnId'),item.get('id')),None);return True
  if method=='serverRequest/resolved':
   identity=self._requests.pop(params.get('requestId'),None)
   return self.room.session.resolve_request(identity) if identity is not None else False
  if method not in ('item/commandExecution/requestApproval','item/fileChange/requestApproval') or self.deadline is None:return False
  item=self._items.get((params.get('turnId'),params.get('itemId')))
  if item is None or len(self._requests)>=16:return False
  item=json.loads(item);summary=self.summary(item,params)
  if summary is None:return False
  deadline=self.deadline(message)
  if deadline is None:return False
  record=json.dumps({'item':item,'request':message},sort_keys=True,separators=(',',':'),allow_nan=False).encode()
  if len(record)>65536:return False
  rpc_id=message.get('id')
  def respond(result):
   wire=json.dumps({'id':rpc_id,'result':result},separators=(',',':'),allow_nan=False).encode()+b'\n'
   if self.writer.write(wire)!=len(wire):raise OSError('native response write uncertain')
   self.writer.flush()
  identity=self.native.open_app_server_request(message,operation_sha256=hashlib.sha256(record).hexdigest(),deadline=deadline,respond=respond,summary=summary)
  self._requests[rpc_id]=identity;return True
