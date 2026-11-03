import unittest,json
from hermes_adapter import Scope
from hermes_body_tools import HermesBodyTools
class Source:
 def __init__(self):self.valid=True
 def body_current(self,scope,action,port):return self.valid
class Port:
 def __init__(self):self.calls=[];self.hook=lambda:None;self.stop_ok=False
 async def call_tool(self,name,arguments):
  self.calls.append((name,arguments));self.hook()
  value={'yaw':5,'pitch':10} if name=='self.robot.get_head_angles' else True
  return {'content':[{'type':'text','text':json.dumps(value)}],'isError':False}
 async def move_bounded(self,scope,args):self.calls.append(('bounded',args));self.hook();return True
 async def stop_bounded(self,scope):self.calls.append(('stop',scope.generation));return self.stop_ok
class Tests(unittest.IsolatedAsyncioTestCase):
 def setUp(self):
  self.scope=Scope('hermes','robot-hermes','device-1','api-session-1','binding-1','generation-1','socket-1','op-1',70.)
  self.source=Source();self.port=Port();self.now=10.;self.t=HermesBodyTools(self.scope,self.source,self.port,lambda:self.now)
 async def test_typed_tools_use_actual_firmware_names(self):
  r=await self.t.call('robot_head_position',{});self.assertEqual(r['position'],{'yaw':5,'pitch':10});self.assertEqual(self.port.calls,[('self.robot.get_head_angles',{})])
  r=await self.t.call('robot_lights',{'red':255,'green':20,'blue':-1});self.assertEqual(self.port.calls[-1],('self.robot.set_led_color',{'red':168,'green':20,'blue':0}));self.assertEqual(r['status'],'accepted');self.assertFalse(r['physical_completed'])
 async def test_motion_requires_bounded_owner_not_legacy_mcp(self):
  r=await self.t.call('robot_look',{'yaw':90,'pitch':85,'speed':900});self.assertTrue(r['ok']);self.assertEqual(self.port.calls,[('bounded',{'yaw':45,'pitch':65,'speed':600})]);self.assertFalse(r['physical_completed']);self.assertNotIn('self.robot.set_head_angles',repr(self.port.calls))
 async def test_motion_not_available_with_generic_mcp_client(self):
  self.port.move_bounded=None;self.assertNotIn('robot_look',self.t.names());self.assertFalse((await self.t.call('robot_look',{}))['ok']);self.assertEqual(self.port.calls,[])
 async def test_no_camera_music_dance_arbitrary_tool_or_extra_identity(self):
  for name,args in [('robot_what_do_you_see',{}),('robot_play_music',{}),('self.robot.set_head_angles',{}),('robot_lights',{'red':0,'device':'other'}),('robot_look',{'yaw':0,'allowed':True})]:self.assertFalse((await self.t.call(name,args))['ok'])
  self.assertEqual(self.port.calls,[])
 async def test_nonfinite_bool_string_numbers_rejected(self):
  for value in [True,'20',float('nan'),float('inf')]:self.assertFalse((await self.t.call('robot_look',{'yaw':value}))['ok'])
  self.assertEqual(self.port.calls,[])
 async def test_revocation_expiry_and_clock_rollback_prevent_dispatch(self):
  for case in ['revoke','expiry','rollback']:
   self.setUp()
   if case=='revoke':self.source.valid=False
   elif case=='expiry':self.now=70.
   else:self.now=9.
   self.assertFalse((await self.t.call('robot_lights',{}))['ok']);self.assertEqual(self.port.calls,[])
 async def test_revoke_during_call_reports_unknown_never_retries(self):
  self.port.hook=lambda:setattr(self.source,'valid',False);r=await self.t.call('robot_look',{});self.assertEqual(r['status'],'unknown');self.source.valid=True;self.assertFalse((await self.t.call('robot_look',{}))['ok']);self.assertEqual(len(self.port.calls),1)
 async def test_unconfirmed_motion_stop_retains_owner(self):
  await self.t.call('robot_look',{});self.assertFalse(await self.t.close());self.assertFalse((await self.t.call('robot_lights',{}))['ok']);self.port.stop_ok=True;self.assertTrue(await self.t.close());self.assertEqual([x[0] for x in self.port.calls],['bounded','stop','stop'])
 async def test_native_unavailable_does_not_reuse_text_or_audio_grant(self):
  from hermes_adapter import UnavailableAuthority
  t=HermesBodyTools(self.scope,UnavailableAuthority(),self.port,lambda:self.now);self.assertEqual(t.names(),());self.assertFalse((await t.call('robot_lights',{}))['ok']);self.assertEqual(self.port.calls,[])

class BoundedWaitTests(unittest.IsolatedAsyncioTestCase):
 setUp=Tests.setUp
 async def test_expiry_bounds_await_and_retains_cancellation_resistant_owner(self):
  import asyncio
  from dataclasses import replace
  self.scope=replace(self.scope,expires=10.01);self.t=HermesBodyTools(self.scope,self.source,self.port,lambda:self.now)
  released=asyncio.Event();entered=asyncio.Event()
  async def blocked(name,arguments):
   entered.set()
   try:await released.wait()
   except asyncio.CancelledError:await released.wait()
   return {'content':[{'type':'text','text':'true'}],'isError':False}
  self.port.call_tool=blocked;task=asyncio.create_task(self.t.call('robot_lights',{}))
  try:
   await entered.wait();await asyncio.sleep(.1)
   self.assertTrue(task.done(),'scope expiry must bound caller wait without pretending backend joined')
   self.assertEqual((await task)['status'],'unknown');self.assertFalse(await self.t.close())
   self.assertFalse((await self.t.call('robot_head_position',{}))['ok'])
  finally:
   released.set();await task;await asyncio.sleep(0)
  self.assertTrue(await self.t.close())

class StopAndSdkTests(unittest.IsolatedAsyncioTestCase):
 setUp=Tests.setUp
 async def test_current_sdk_error_flag_cannot_be_accepted(self):
  from types import SimpleNamespace
  async def error(name,arguments):return SimpleNamespace(content=[SimpleNamespace(type='text',text='true')],is_error=True)
  self.port.call_tool=error;self.assertFalse((await self.t.call('robot_lights',{}))['ok'])
 async def test_cleanup_has_finite_wait_and_retains_unjoined_stop(self):
  import asyncio
  try:self.t=HermesBodyTools(self.scope,self.source,self.port,lambda:self.now,cleanup_timeout=.01)
  except TypeError:self.fail('bounded cleanup wait is not implemented')
  await self.t.call('robot_look',{});released=asyncio.Event()
  async def stop(scope):
   try:await released.wait()
   except asyncio.CancelledError:await released.wait()
   return True
  self.port.stop_bounded=stop;task=asyncio.create_task(self.t.close())
  try:
   await asyncio.sleep(.1);self.assertTrue(task.done());self.assertFalse(await task)
  finally:released.set();await task;await asyncio.sleep(0)
  self.assertTrue(await self.t.close())

class IncompletePort(unittest.IsolatedAsyncioTestCase):
 setUp=Tests.setUp
 async def test_motion_needs_cleanup_port_before_advertising_or_dispatch(self):
  self.port.stop_bounded=None;self.assertNotIn('robot_look',self.t.names());self.assertFalse((await self.t.call('robot_look',{}))['ok']);self.assertEqual(self.port.calls,[])
if __name__=='__main__':unittest.main()
