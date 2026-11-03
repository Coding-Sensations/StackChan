# SPDX-License-Identifier: MIT
"""Hermes typed body tools over an already-owned MCP device client.

Inspired by Nous-Girl's fixed tool/argument adapters, rewritten for this firmware's
actual tool names, angle units and scoped ownership. No upstream code/art copied.
No generic passthrough, camera/Music/SD tools, token lookup, listener or startup.
The captured port is a privileged owner dependency, never constructed from RPC
fields. Source.body_current must verify exact port object/incarnation, provider,
foreground, device/session, grant and own clock. No production source is supplied.
"""
import asyncio,math
from hermes_adapter import Scope,identifier,strict_json

class HermesBodyTools:
 def __init__(self,scope,source,port,clock,*,cleanup_timeout=3.):
  if not isinstance(scope,Scope) or scope.provider!='hermes' or any(not identifier(getattr(scope,k)) for k in ['device','session','binding','generation','incarnation']):raise ValueError('exact Hermes owner required')
  if type(cleanup_timeout) not in (int,float) or not math.isfinite(cleanup_timeout) or not .001<=cleanup_timeout<=3.:raise ValueError('bounded cleanup wait required')
  now=clock()
  if type(now) not in (int,float) or not math.isfinite(now) or not now<scope.expires<=now+120:raise ValueError('finite owner budget required')
  self.scope,self.source,self.port,self.clock=scope,source,port,clock;self._last=now;self._closed=False;self._busy=False;self._motion=False;self._revision=1;self._pending=None;self._cancel_requested=False;self._stop_task=None;self._stop_cancel_requested=False;self._cleanup_timeout=cleanup_timeout;self._guarding=False;self._closing=False
 def _guard(self,action):
  if self._closed or self._guarding:return False
  revision=self._revision;self._guarding=True
  try:valid=self.source.body_current(self.scope,action,self.port);now=self.clock()
  except BaseException:return False
  finally:self._guarding=False
  if valid is not True or self._closed or revision!=self._revision or type(now) not in (int,float) or not math.isfinite(now) or not self._last<=now<self.scope.expires:return False
  self._last=now;return True
 def names(self):
  if not self._guard('body.catalog'):return ()
  names=['robot_head_position','robot_lights']
  if (callable(getattr(self.port,'move_bounded',None)) and callable(getattr(self.port,'stop_bounded',None))):names.append('robot_look')
  return tuple(names)
 async def _invoke(self,action,callback,*args,**kwargs):
  if self._pending is not None:raise ValueError('owner retained')
  async def invoke():
   if not self._guard(action):raise ValueError('scope unavailable')
   return await callback(*args,**kwargs)
  task=asyncio.create_task(invoke());self._pending=task;self._cancel_requested=False
  def observe(done):
   try:done.exception()
   except BaseException:pass
  task.add_done_callback(observe)
  try:
   remaining=self.scope.expires-self.clock()
   if not math.isfinite(remaining) or remaining<=0:raise ValueError('scope expired')
   done,_=await asyncio.wait({task},timeout=min(3.,remaining))
   if not done:raise ValueError('call completion unknown')
   self._pending=None
   return task.result()
  except BaseException:
   if not task.done() and not self._cancel_requested:task.cancel();self._cancel_requested=True
   raise
 @staticmethod
 def _numbers(args,defaults,bounds):
  if type(args) is not dict or not set(args)<=set(defaults):raise ValueError('typed arguments required')
  result={}
  for key,default in defaults.items():
   value=args.get(key,default)
   if type(value) is not int:raise ValueError('integer required')
   low,high=bounds[key];result[key]=max(low,min(high,value))
  return result
 @staticmethod
 def _value(result):
  if type(result) is dict:errors=[result[k] for k in ('isError','is_error') if k in result]
  else:errors=[getattr(result,k) for k in ('isError','is_error') if hasattr(result,k)]
  error=False if not errors else any(value is not False for value in errors)
  content=result.get('content') if type(result) is dict else getattr(result,'content',None)
  if error is not False or type(content) is not list or len(content)!=1:raise ValueError('bounded MCP result required')
  part=content[0];kind=part.get('type') if type(part) is dict else getattr(part,'type',None);text=part.get('text') if type(part) is dict else getattr(part,'text',None)
  if kind!='text' or type(text) is not str or len(text.encode())>2048:raise ValueError('bounded MCP text required')
  return strict_json(text)
 async def call(self,name,args):
  if self._closed or self._busy:return {'ok':False,'status':'unavailable'}
  try:
   if name=='robot_head_position':
    if type(args) is not dict or args:raise ValueError('no arguments')
    action='body.read';command='self.robot.get_head_angles';values={}
   elif name=='robot_lights':
    action='body.led';command='self.robot.set_led_color';values=self._numbers(args,{'red':0,'green':0,'blue':0},{k:(0,168) for k in ('red','green','blue')})
   elif name=='robot_look' and (callable(getattr(self.port,'move_bounded',None)) and callable(getattr(self.port,'stop_bounded',None))):
    action='body.motion';command=None;values=self._numbers(args,{'yaw':0,'pitch':3,'speed':150},{'yaw':(-45,45),'pitch':(3,65),'speed':(100,600)})
   else:return {'ok':False,'status':'unavailable'}
  except (ValueError,TypeError,UnicodeError):return {'ok':False,'status':'invalid_arguments'}
  if not self._guard(action):self._closed=True;self._revision+=1;return {'ok':False,'status':'unavailable'}
  self._busy=True
  try:
   if command is None:
    # Native port must admit both actual Servo owners with this exact generation,
    # roll back the first if second admission fails, and keep per-output checks.
    # False/exception may still be partial: retain cleanup instead of retrying.
    self._motion=True
    result=await self._invoke(action,self.port.move_bounded,self.scope,values)
    if result is not True:raise ValueError('motion admission unconfirmed')
   else:
    value=self._value(await self._invoke(action,self.port.call_tool,command,arguments=values))
    if name=='robot_head_position':
     if type(value) is not dict or set(value)!={'yaw','pitch'} or any(type(value[k]) is not int for k in value) or not -128<=value['yaw']<=128 or not 0<=value['pitch']<=90:raise ValueError('position unavailable')
    elif value is not True:raise ValueError('device tool refused')
   if not self._guard(action):raise ValueError('scope changed after dispatch')
   if name=='robot_head_position':return {'ok':True,'position':value,'source':'device_report'}
   return {'ok':True,'status':'accepted','physical_completed':False,'arguments':values}
  except BaseException:
   self._closed=True;self._revision+=1;return {'ok':False,'status':'unknown'}
  finally:self._busy=False
 async def close(self):
  if self._closing:return False
  self._closing=True
  try:return await self._close_owned()
  finally:self._closing=False
 async def _close_owned(self):
  self._closed=True;self._revision+=1
  if self._pending is not None:
   if not self._pending.done():
    if not self._cancel_requested:self._pending.cancel();self._cancel_requested=True
    return False
   try:self._pending.result()
   except BaseException:pass
   self._pending=None
  if self._motion:
   try:
    # Retained cleanup uses only the original captured owner, never a replacement
    # or a shared MCP socket close. True must mean actual checked stop+join;
    # Servo.stopBounded alone cannot produce this evidence.
    if self._stop_task is None:
     self._stop_task=asyncio.create_task(self.port.stop_bounded(self.scope));self._stop_cancel_requested=False
     def observe(done):
      try:done.exception()
      except BaseException:pass
     self._stop_task.add_done_callback(observe)
    task=self._stop_task
    done,_=await asyncio.wait({task},timeout=self._cleanup_timeout)
    if not done:
     if not self._stop_cancel_requested:task.cancel();self._stop_cancel_requested=True
     return False
    self._stop_task=None
    if task.result() is not True:return False
   except BaseException:
    if self._stop_task is not None and not self._stop_task.done() and not self._stop_cancel_requested:self._stop_task.cancel();self._stop_cancel_requested=True
    return False
   self._motion=False
  return True

def register_tools(server,tools):
 """Attach typed handlers to an owner-supplied existing MCP server; no start."""
 # The SDK's Pydantic dependency must preserve raw integer types; plain int
 # annotations otherwise coerce booleans/strings before our boundary sees them.
 try:from pydantic import StrictInt
 except ImportError:raise RuntimeError('MCP strict argument dependency unavailable') from None
 async def robot_head_position():
  """Read angles from the selected, authorized StackChan device."""
  return await tools.call('robot_head_position',{})
 async def robot_lights(red:StrictInt=0,green:StrictInt=0,blue:StrictInt=0):
  """Set RGB LEDs capped at168; separate LED grant required."""
  return await tools.call('robot_lights',dict(red=red,green=green,blue=blue))
 async def robot_look(yaw:StrictInt=0,pitch:StrictInt=3,speed:StrictInt=150):
  """Bounded head movement for this captured owner; no camera/autonomy."""
  return await tools.call('robot_look',dict(yaw=yaw,pitch=pitch,speed=speed))
 names=tools.names()
 for callback in (robot_head_position,robot_lights,robot_look):
  if callback.__name__ in names:server.tool()(callback)
 return names
