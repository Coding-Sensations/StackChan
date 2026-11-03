import hashlib,io,json,os,unittest
from companion_events import CompanionSession,NativeHookOwner
try:
 from provider_room_bridge import RoomBridge,AppServerRoomOwner,decode_wire,encode_wire
except ImportError:RoomBridge=None
E=b'event-fixture-key-not-a-credential!';D=b'decision-fixture-key-not-secret!!'
class RoomWire(unittest.TestCase):
 def setUp(self):
  self.assertIsNotNone(RoomBridge,'room/native bridge missing');self.now=10.
  self.s=CompanionSession('mira','binding','generation',E,D,scopes={'activity','approval'},clock=lambda:self.now)
  self.owner=NativeHookOwner(self.s,'thread1',E,self.s.receive,native_protocol='codex')
  self.bridge=RoomBridge(self.s,E,D,clock=lambda:self.now)
 def start(self):self.assertTrue(self.owner.observe_app_server({'method':'turn/started','params':{'threadId':'thread1','turn':{'id':'task1','status':'inProgress'}}}))
 def request(self):
  self.start();self.answers=[]
  return self.owner.open_app_server_request({'id':7,'method':'item/commandExecution/requestApproval','params':{'threadId':'thread1','turnId':'task1','itemId':'item1'}},operation_sha256='a'*64,deadline=20.,respond=self.answers.append,summary='Run fixture-only command in fixture workspace')
 def intent(self,view,sequence=1,**change):
  p={k:view[k] for k in ('provider','binding','generation','presentation_id','request_id','operation_sha256')};p.update(version=1,sequence=sequence,choice='approve');p.update(change)
  return encode_wire(D,p,b'stackchan-provider-decision-v1\0')
 def test_exact_display_ticket_and_callback_receipt(self):
  self.request();view=decode_wire(E,self.bridge.snapshot(),b'stackchan-provider-room-v1\0')
  self.assertEqual(view['provider'],'mira');self.assertEqual(view['scene'],'decision_sign');self.assertEqual(view['ttl_ms'],10000)
  self.assertTrue(self.bridge.decision(self.intent(view)));self.assertEqual(self.answers,[{'decision':'accept'}])
  self.assertFalse(self.bridge.decision(self.intent(view,2)))
  waiting=decode_wire(E,self.bridge.snapshot(),b'stackchan-provider-room-v1\0');self.assertEqual(waiting['scene'],'waiting_receipt');self.assertEqual(waiting['request_id'],'')
 def test_stale_or_foreign_or_expired_view_never_answers(self):
  self.request();old=decode_wire(E,self.bridge.snapshot(),b'stackchan-provider-room-v1\0');current=decode_wire(E,self.bridge.snapshot(),b'stackchan-provider-room-v1\0')
  self.assertEqual(old['presentation_id'],current['presentation_id']);self.assertFalse(self.bridge.decision(self.intent(current,provider='codex')))
  self.now=20.;self.assertFalse(self.bridge.decision(self.intent(current)));self.assertEqual(self.answers,[])
 def test_refresh_preserves_deliberate_press_but_replacement_rotates_ticket(self):
  identity=self.request();old=decode_wire(E,self.bridge.snapshot(),b'stackchan-provider-room-v1\0')
  self.now+=.25
  refreshed=decode_wire(E,self.bridge.snapshot(),b'stackchan-provider-room-v1\0')
  self.assertEqual(old['presentation_id'],refreshed['presentation_id']);self.assertLess(refreshed['ttl_ms'],old['ttl_ms'])
  self.assertTrue(self.bridge.decision(self.intent(old)));self.assertEqual(self.answers,[{'decision':'accept'}])
  self.s.resolve_request(identity)
  self.owner.open_app_server_request({'id':8,'method':'item/commandExecution/requestApproval','params':{'threadId':'thread1','turnId':'task1','itemId':'item2'}},operation_sha256='b'*64,deadline=21.,respond=self.answers.append,summary='Different fixture-only command')
  replacement=decode_wire(E,self.bridge.snapshot(),b'stackchan-provider-room-v1\0')
  self.assertNotEqual(old['presentation_id'],replacement['presentation_id']);self.assertFalse(self.bridge.decision(self.intent(old,2)))
 def test_unknown_world_and_no_summary_cannot_show_approve(self):
  view=decode_wire(E,self.bridge.snapshot(),b'stackchan-provider-room-v1\0');self.assertEqual(view['scene'],'unknown');self.assertEqual(view['request_id'],'')
  self.start();self.owner.open_app_server_request({'id':7,'method':'item/fileChange/requestApproval','params':{'threadId':'thread1','turnId':'task1','itemId':'item1'}},operation_sha256='a'*64,deadline=20.,respond=lambda _:None)
  view=decode_wire(E,self.bridge.snapshot(),b'stackchan-provider-room-v1\0');self.assertEqual(view['scene'],'waiting_receipt');self.assertEqual(view['request_id'],'')
 def test_authentication_duplicates_and_size(self):
  self.start();wire=self.bridge.snapshot()
  with self.assertRaises(ValueError):decode_wire(D,wire,b'stackchan-provider-room-v1\0')
  with self.assertRaises(ValueError):decode_wire(E,wire+b' '*2048,b'stackchan-provider-room-v1\0')
  with self.assertRaises(ValueError):decode_wire(E,b'0'*64+b'\n{"version":1,"version":1}',b'stackchan-provider-room-v1\0')
 def test_room_publication_drains_completions_without_ordinary_timeout(self):
  for n in range(80):
   task='turn'+str(n)
   self.assertTrue(self.owner.observe_app_server({'method':'turn/started','params':{'threadId':'thread1','turn':{'id':task,'status':'inProgress'}}}))
   self.bridge.snapshot()
   self.assertTrue(self.owner.observe_app_server({'method':'turn/completed','params':{'threadId':'thread1','turn':{'id':task,'status':'completed'}}}))
   view=decode_wire(E,self.bridge.snapshot(),b'stackchan-provider-room-v1\0');self.assertEqual(view['scene'],'completion')
   self.now+=3.;self.assertEqual(decode_wire(E,self.bridge.snapshot(),b'stackchan-provider-room-v1\0')['scene'],'idle_decoration')

 def test_actual_owned_pipe_request_uses_complete_item_digest(self):
  read_fd,write_fd=os.pipe()
  try:
   with os.fdopen(write_fd,'wb',buffering=0) as writer:
    adapter=AppServerRoomOwner(self.owner,self.bridge,writer,summary=lambda item,params:'Run fixture-only command in fixture workspace',clock=lambda:self.now,deadline=lambda request:20.)
    self.start();item={'method':'item/started','params':{'threadId':'thread1','turnId':'task1','item':{'id':'item1','type':'commandExecution','command':'fixture-only','cwd':'/fixture','status':'inProgress'}}}
    self.assertTrue(adapter.handle(item))
    request={'id':9,'method':'item/commandExecution/requestApproval','params':{'threadId':'thread1','turnId':'task1','itemId':'item1'}}
    self.assertTrue(adapter.handle(request));view=decode_wire(E,self.bridge.snapshot(),b'stackchan-provider-room-v1\0')
    self.assertTrue(self.bridge.decision(self.intent(view)))
    self.assertTrue(adapter.handle({'method':'serverRequest/resolved','params':{'threadId':'thread1','requestId':9}}))
   reply=json.loads(os.read(read_fd,1024));self.assertEqual(reply,{'id':9,'result':{'decision':'accept'}})
  finally:os.close(read_fd)
 def test_missing_item_or_summary_leaves_native_owner_in_control(self):
  writer=io.BytesIO();adapter=AppServerRoomOwner(self.owner,self.bridge,writer,summary=lambda item,params:None,clock=lambda:self.now)
  self.start();request={'id':9,'method':'item/fileChange/requestApproval','params':{'threadId':'thread1','turnId':'task1','itemId':'missing'}}
  self.assertFalse(adapter.handle(request));self.assertEqual(writer.getvalue(),b'')

if __name__=='__main__':unittest.main()
