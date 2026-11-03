from test_https_wire import qualified_context
import unittest,json,ssl
import test_hermes_adapter as fixtures
from test_hermes_adapter import E,ROOM,decode_wire
from test_https_wire import Connection,Response,TOKEN
from hermes_adapter import HTTPSWire,HermesAdapter,Unknown
from hermes_body_tools import register_tools
import test_body_tools as body_fixtures
class Pipeline(unittest.TestCase):
 setUp=fixtures.Tests.setUp
 def test_actual_https_adapter_and_signed_room_pipeline(self):
  post,get=Connection(),Connection();get.response=Response(json.dumps({'object':'hermes.run','run_id':'run-1','session_id':'api-session-1','status':'completed','output':'Hello from Hermes'}).encode())
  connections=iter([post,get]);wire=HTTPSWire('https://127.0.0.1',TOKEN,qualified_context(),connection_factory=lambda *a,**kw:next(connections))
  adapter=HermesAdapter(self.scope,self.source,wire,self.session,E,lambda:self.frames.append(self.room.snapshot()),lambda:self.now)
  self.assertTrue(adapter.submit_text('Hi'));self.assertTrue(adapter.poll_once());self.assertEqual(adapter.result()['text'],'Hello from Hermes');self.assertEqual(decode_wire(E,self.frames[0],ROOM)['scene'],'completion')
  self.assertEqual(get.calls[1][0:2],('GET','/p/robot-hermes/v1/runs/run-1'));self.assertNotIn('Idempotency-Key',get.calls[1][3]);self.assertEqual(post.calls[-1],get.calls[-1])
class Registration(unittest.IsolatedAsyncioTestCase):
 setUp=body_fixtures.Tests.setUp
 async def test_registered_handlers_bind_selected_scope_and_exclude_unavailable_tools(self):
  class Server:
   def __init__(self):self.handlers={}
   def tool(self):
    def register(fn):self.handlers[fn.__name__]=fn;return fn
    return register
   def run(self):raise AssertionError('adapter started a listener')
  server=Server();names=register_tools(server,self.t)
  self.assertEqual(set(names),{'robot_head_position','robot_lights','robot_look'});self.assertEqual(set(server.handlers),set(names))
  self.assertTrue((await server.handlers['robot_lights'](red=5))['ok']);self.assertEqual(self.port.calls,[('self.robot.set_led_color',{'red':5,'green':0,'blue':0})])
  self.source.valid=False;self.assertFalse((await server.handlers['robot_lights'](red=5))['ok']);self.assertEqual(len(self.port.calls),1)

class Reentry(unittest.TestCase):
 setUp=fixtures.Tests.setUp
 def test_authority_observer_cannot_reenter_submit_or_result(self):
  entered=False
  def current(scope,action):
   nonlocal entered
   if not entered:entered=True;self.assertFalse(self.a.submit_text('nested'))
   return True
  self.source.current=current;self.assertFalse(self.a.submit_text('outer'));self.assertEqual(self.wire.requests,[])
 def test_reentrant_close_does_not_repeat_resource_close(self):
  calls=[]
  def close():
   calls.append('close')
   if len(calls)<2:self.a.close()
  self.wire.close=close;self.a.close();self.assertEqual(calls,['close'])

class MultipleTurns(unittest.TestCase):
 setUp=fixtures.Tests.setUp
 def finish(self):
  self.wire.answers.append({'run_id':'run-1','status':'started'});self.assertTrue(self.a.submit_text('first'))
  self.wire.answers.append(dict(object='hermes.run',run_id='run-1',session_id='api-session-1',status='completed',output='first reply'));self.assertTrue(self.a.poll_once())
 def test_next_explicit_turn_reuses_exact_session_with_new_operation(self):
  from dataclasses import replace
  self.finish();new=replace(self.scope,operation='operation-2',expires=80.)
  self.assertTrue(self.a.next_turn(new));self.assertEqual(self.a.result()['text'],'')
  self.wire.answers.append({'run_id':'run-2','status':'started'});self.assertTrue(self.a.submit_text('second'))
  self.wire.answers.append(dict(object='hermes.run',run_id='run-2',session_id='api-session-1',status='completed',output='second reply'));self.assertTrue(self.a.poll_once());self.assertEqual(self.a.result()['text'],'second reply')
  self.assertEqual([r[3] for r in self.wire.requests if r[0]=='POST'],['operation-1','operation-2']);self.assertEqual(decode_wire(E,self.frames[-1],ROOM)['scene'],'completion')
 def test_no_next_turn_before_terminal_or_after_unknown_or_close(self):
  from dataclasses import replace
  new=replace(self.scope,operation='operation-2')
  self.assertFalse(self.a.next_turn(new));self.wire.answers.append(Unknown('uncertain'));self.assertFalse(self.a.submit_text('first'));self.assertFalse(self.a.next_turn(new))
 def test_replacement_identity_or_reused_operation_cannot_advance(self):
  from dataclasses import replace
  self.finish()
  for field,value in [('profile','other'),('device','other'),('session','other'),('binding','other'),('generation','other'),('incarnation','other'),('provider','mira'),('kind','native')]:self.assertFalse(self.a.next_turn(replace(self.scope,**{field:value,'operation':'operation-2'})))
  self.assertFalse(self.a.next_turn(self.scope));self.assertEqual(len(self.wire.requests),2)
 def test_revoke_or_reentry_during_new_capture_does_not_replace_owner(self):
  from dataclasses import replace
  self.finish();self.source.valid=False;self.assertFalse(self.a.next_turn(replace(self.scope,operation='operation-2')));self.assertEqual(len(self.wire.requests),2)
class TurnLifecycle(unittest.TestCase):
 setUp=fixtures.Tests.setUp
 finish=MultipleTurns.finish
 def test_failed_and_cancelled_turns_require_fresh_capture_then_complete(self):
  from dataclasses import replace
  for status in ['failed','cancelled','interrupted']:
   self.setUp();self.wire.answers.append({'run_id':'run-1','status':'started'});self.assertTrue(self.a.submit_text('first'))
   self.wire.answers.append(dict(object='hermes.run',run_id='run-1',session_id='api-session-1',status=status));self.assertTrue(self.a.poll_once())
   self.assertTrue(self.a.next_turn(replace(self.scope,operation='operation-2')))
   self.wire.answers.append({'run_id':'run-2','status':'started'});self.assertTrue(self.a.submit_text('second'))
   self.wire.answers.append(dict(object='hermes.run',run_id='run-2',session_id='api-session-1',status='completed',output='ok'));self.assertTrue(self.a.poll_once())
   self.assertEqual(self.a.result()['text'],'ok')
 def test_invalid_clock_expiry_or_callback_exception_cannot_replace_capture(self):
  from dataclasses import replace
  for now,expires in [(9.,70.),(10.,10.),(10.,131.),(float('nan'),70.),(10.,float('inf'))]:
   self.setUp();self.finish();self.now=now
   self.assertFalse(self.a.next_turn(replace(self.scope,operation='operation-2',expires=expires)));self.assertEqual(self.a.scope,self.scope);self.assertEqual(len(self.wire.requests),2)
  self.setUp();self.finish()
  def fault(*a):raise RuntimeError('private synthetic source detail')
  self.source.current=fault;self.assertFalse(self.a.next_turn(replace(self.scope,operation='operation-2')));self.assertEqual(self.a.scope,self.scope)
 def test_reentrant_close_or_turn_capture_cannot_replace_owner(self):
  from dataclasses import replace
  for close in [False,True]:
   self.setUp();self.finish();new=replace(self.scope,operation='operation-2')
   def callback(*a):
    self.assertFalse(self.a.next_turn(new))
    if close:self.a.close()
    return True
   self.source.current=callback;self.assertEqual(self.a.next_turn(new),not close)
   self.assertEqual(self.a.scope,new if not close else self.scope);self.assertEqual(len(self.wire.requests),2)
 def test_turn_operation_history_is_bounded_and_close_is_terminal(self):
  from dataclasses import replace
  for index in range(1,65):
   run='run-'+str(index);self.wire.answers.append({'run_id':run,'status':'started'});self.assertTrue(self.a.submit_text('turn'))
   self.wire.answers.append(dict(object='hermes.run',run_id=run,session_id='api-session-1',status='completed',output='ok'));self.assertTrue(self.a.poll_once())
   self.assertEqual(self.a.next_turn(replace(self.scope,operation='operation-'+str(index+1))),index<64)
  self.assertEqual(len(self.a._operations),64);self.a.close();self.assertFalse(self.a.next_turn(replace(self.scope,operation='operation-66')));self.assertEqual(len(self.wire.requests),128)
if __name__=='__main__':unittest.main()
