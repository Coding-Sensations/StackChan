import unittest,json,sys
from pathlib import Path
from companion_events import CompanionSession
from provider_room_bridge import RoomBridge,decode_wire,ROOM
from hermes_adapter import Scope,HermesAdapter,UnavailableAuthority,Refused,Unknown
E=b'fixture-event-key-only-000000000000';D=b'fixture-other-key-only-111111111111'
class Source:
 def __init__(self):self.valid=True;self.actions=[]
 def current(self,scope,action):self.actions.append(action);return self.valid
class Wire:
 def __init__(self):self.requests=[];self.answers=[];self.hook=lambda:None;self.closed=False
 def exchange(self,method,path,payload,operation,guard,*,remaining=None):
  if not guard():raise Refused('scope unavailable')
  self.requests.append((method,path,payload,operation));self.hook()
  if not guard():raise Unknown('scope changed after dispatch')
  a=self.answers.pop(0)
  if isinstance(a,Exception):raise a
  return a
 def close(self):self.closed=True
class Tests(unittest.TestCase):
 def setUp(self):
  self.now=10.;self.source=Source();self.wire=Wire();self.frames=[]
  self.scope=Scope('hermes','robot-hermes','device-1','api-session-1','binding-1','generation-1','socket-1','operation-1',70.)
  self.session=CompanionSession('hermes','binding-1','generation-1',E,D,scopes={'activity'},clock=lambda:self.now)
  self.room=RoomBridge(self.session,E,D,clock=lambda:self.now)
  self.a=HermesAdapter(self.scope,self.source,self.wire,self.session,E,lambda:self.frames.append(self.room.snapshot()),lambda:self.now)
 def submit(self):self.wire.answers.append({'run_id':'run-1','status':'started','replayed':False});self.assertTrue(self.a.submit_text('Hello Hermes'))
 def state(self,status='running',**fields):return dict(object='hermes.run',run_id='run-1',session_id='api-session-1',status=status,**fields)
 def test_exact_api_owned_session_and_oneuse_idempotency(self):
  self.submit();self.assertEqual(self.wire.requests,[('POST','/p/robot-hermes/v1/runs',{'session_id':'api-session-1','input':'Hello Hermes'},'operation-1')]);self.assertFalse(self.a.submit_text('again'));self.assertEqual(len(self.wire.requests),1);self.assertFalse(self.a.result()['native']);self.assertEqual(self.frames,[])
 def test_running_then_completed_routes_to_actual_room_without_prompt_leak(self):
  self.submit();self.wire.answers.append(self.state());self.assertTrue(self.a.poll_once());self.assertEqual(decode_wire(E,self.frames[-1],ROOM)['scene'],'desk_work')
  self.wire.answers.append(self.state('completed',output='Actual reply'));self.assertTrue(self.a.poll_once());self.assertEqual(decode_wire(E,self.frames[-1],ROOM)['scene'],'completion');self.assertEqual(self.a.result()['text'],'Actual reply');self.assertNotIn(b'Hello Hermes',self.frames[-1]);self.assertNotIn(b'Actual reply',self.frames[-1])
 def test_fast_terminal_has_only_completion_frame(self):
  self.submit();self.wire.answers.append(self.state('completed',output='fast'));self.assertTrue(self.a.poll_once());self.assertEqual(len(self.frames),1);self.assertEqual(decode_wire(E,self.frames[0],ROOM)['scene'],'completion')
 def test_wrong_session_or_run_never_projects_and_blocks(self):
  for field in ['session_id','run_id']:
   self.setUp();self.submit();bad=self.state();bad[field]='other';self.wire.answers.append(bad);self.assertFalse(self.a.poll_once());self.assertEqual(self.frames,[]);self.assertEqual(self.a.result()['stage'],'unknown');self.assertFalse(self.a.submit_text('replacement'))
 def test_no_authority_no_network(self):
  self.source.valid=False;self.assertFalse(self.a.submit_text('hello'));self.assertEqual(self.wire.requests,[])
 def test_expiry_and_clock_rollback_stop_before_network(self):
  for when in [70.,9.,float('nan')]:
   self.setUp();self.now=when;self.assertFalse(self.a.submit_text('hello'));self.assertEqual(self.wire.requests,[])
 def test_revoke_during_dispatch_is_unknown_never_retry(self):
  self.wire.hook=lambda:setattr(self.source,'valid',False);self.assertFalse(self.a.submit_text('hello'));self.assertEqual(self.a.result()['stage'],'unknown');self.source.valid=True;self.assertFalse(self.a.submit_text('again'));self.assertEqual(len(self.wire.requests),1)
 def test_http_unknown_does_not_make_new_run(self):
  self.wire.answers.append(Unknown('synthetic'));self.assertFalse(self.a.submit_text('hello'));self.assertEqual(self.a.result()['stage'],'unknown');self.assertFalse(self.a.submit_text('hello'));self.assertEqual(len(self.wire.requests),1)
 def test_unexpected_or_failed_status_never_successful(self):
  for status,want in [('failed','error'),('cancelled','cancelled'),('interrupted','cancelled'),('awaiting_approval','unavailable')]:
   self.setUp();self.submit();self.wire.answers.append(self.state(status,error='private failure'));self.assertEqual(self.a.poll_once(),status!='awaiting_approval');self.assertNotEqual(self.a.result()['stage'],'completed');self.assertNotIn('private failure',repr(self.a.result()))
 def test_close_reentrant_and_future_calls_inert(self):
  self.wire.hook=self.a.close;self.assertFalse(self.a.submit_text('hello'));self.assertFalse(self.a.poll_once());self.assertEqual(self.frames,[]);self.assertTrue(self.wire.closed)
 def test_wrong_provider_and_cross_bound_room_rejected(self):
  from dataclasses import replace
  for scope in [replace(self.scope,provider='mira'),replace(self.scope,binding='other'),replace(self.scope,generation='other'),replace(self.scope,profile='../x')]:
   with self.assertRaises(ValueError):HermesAdapter(scope,self.source,self.wire,self.session,E,lambda:None,lambda:self.now)
 def test_invalid_text_never_leaves_process(self):
  for text in ['', 'x'*1025,'line\x00break',{'input':'x'}]:
   self.setUp();self.assertFalse(self.a.submit_text(text));self.assertEqual(self.wire.requests,[])
 def test_wrong_status_shape_or_oversized_reply_fails_closed(self):
  for value in [[],self.state('completed',output='x'*4097),self.state(status=True)]:
   self.setUp();self.submit();self.wire.answers.append(value);self.assertFalse(self.a.poll_once());self.assertNotEqual(self.a.result()['stage'],'completed')

class LifecycleTests(unittest.TestCase):
 setUp=Tests.setUp
 submit=Tests.submit
 state=Tests.state
 def test_result_revalidates_expiry_and_drops_scoped_reply(self):
  self.submit();self.wire.answers.append(self.state('completed',output='private reply'));self.assertTrue(self.a.poll_once());self.source.valid=False;self.assertEqual(self.a.result()['text'],'');self.assertNotEqual(self.a.result()['stage'],'completed')
 def test_source_exception_cannot_escape_with_private_details(self):
  def fault(*args):raise RuntimeError('synthetic private owner detail')
  self.source.current=fault;self.assertFalse(self.a.submit_text('hello'));self.assertEqual(self.wire.requests,[])
 def test_illformed_unicode_is_rejected_without_dispatch(self):
  self.assertFalse(self.a.submit_text('\ud800'));self.assertEqual(self.wire.requests,[])
 def test_cleanup_failure_returns_retained_state_and_no_new_dispatch(self):
  def fault():raise Unknown('cleanup unconfirmed')
  self.wire.close=fault;self.wire.answers.append(Unknown('dispatch uncertain'));self.assertFalse(self.a.submit_text('hello'));self.assertEqual(self.a.result()['stage'],'cleanup_pending');self.assertFalse(self.a.submit_text('again'));self.assertEqual(len(self.wire.requests),1)

if __name__=='__main__':unittest.main()
