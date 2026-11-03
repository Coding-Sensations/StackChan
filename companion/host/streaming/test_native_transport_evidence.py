"""Sanitized terminal evidence must retain counts that distinguish stalled/bursty IO."""
import unittest
from types import SimpleNamespace
from transport_diagnostics import TransportDiagnostics,project_diagnostics
class Evidence(unittest.TestCase):
 def session(self):
  return SimpleNamespace(bridge=SimpleNamespace(native_audio={'output_callbacks':17,'output_consumed_frames':512,'output_queue_frames':5712,'error':0,'untrusted':'do not persist'}),acknowledged=True,armed=False,deadline=2,mic_seq=7,backpressure_mic_packets=5,backpressure_mic_streak=5)
 def test_terminal_observation_keeps_bounded_native_counts(self):
  d=TransportDiagnostics(clock=lambda:0);d.observe(self.session());v=d.snapshot()
  self.assertEqual(v.get('native_audio'),{'output_callbacks':17,'output_consumed_frames':512,'output_queue_frames':5712,'error':0})
  self.assertEqual(v.get('backpressure_mic_packets'),5)
  self.assertEqual(v.get('backpressure_mic_streak'),5)
 def test_projection_drops_strings_oversized_and_negative_counts(self):
  v=project_diagnostics({'native_audio':{'output_callbacks':-1,'input_frames':2**80,'input_dropped':'secret','error':-50,'output_queue_frames':3,'untrusted':'secret'},'backpressure_mic_packets':-1})
  self.assertEqual(v.get('native_audio'),{'error':-50,'output_queue_frames':3})
  self.assertNotIn('backpressure_mic_packets',v)
 def test_no_bridge_or_native_audio_is_safe(self):
  d=TransportDiagnostics(clock=lambda:0);d.observe(SimpleNamespace())
  self.assertNotIn('native_audio',d.snapshot())
if __name__=='__main__':unittest.main()

class DriverEvidence(unittest.IsolatedAsyncioTestCase):
 async def test_fault_observes_latest_native_counters_before_owner_cleanup(self):
  import asyncio
  from gateway_connection import run_connection
  from streaming_session import SustainedAudioBackpressure
  session=Evidence().session();session.bridge.native_audio={};session.clock=lambda:0
  session._authorized=lambda:None
  def receive(wire):
   session.bridge.native_audio={'output_callbacks':3,'output_consumed_frames':512,'output_queue_frames':5712,'error':0}
   raise SustainedAudioBackpressure('synthetic')
  session.receive=receive
  session.close=lambda:setattr(session.bridge,'native_audio',{})
  class Socket:
   async def recv(self):return bytes(17)
   async def close(self):pass
  d=TransportDiagnostics(clock=lambda:0)
  with self.assertRaises(SustainedAudioBackpressure):await run_connection(Socket(),session,diagnostics=d)
  self.assertEqual(d.snapshot()['native_audio']['output_callbacks'],3)

 async def test_timed_out_socket_close_reaps_exact_completed_owner(self):
  import asyncio
  from gateway_connection import OwnedSocketClose
  event=asyncio.Event()
  class Socket:
   calls=0
   async def close(self):self.calls+=1;await event.wait()
  socket=Socket();owner=OwnedSocketClose(socket)
  with self.assertRaises(TimeoutError):await owner.close(timeout=.001)
  task=owner.task;self.assertFalse(owner.confirmed)
  event.set();await task
  await owner.close()
  self.assertTrue(owner.confirmed);self.assertIs(owner.task,task);self.assertEqual(socket.calls,1)
