"""Wall-clock full-queue protection; no audio IO, hardware or networking."""
import unittest
import test_native_backpressure as fixtures
NativeRing=fixtures.NativeRing
from streaming_session import SustainedAudioBackpressure,encode_frame

class Clock:
 def __init__(self):self.now=0.
 def __call__(self):return self.now

class BurstTests(unittest.TestCase):
 _session=fixtures.NativeBackpressureTests._session
 def test_five_arrivals_in_one_burst_are_not_a_sustained_clock_stall(self):
  adapter,bridge,session=self._session(NativeRing(queued=NativeRing.capacity))
  clock=Clock();session.clock=clock
  try:
   for seq in range(7):
    try:session.receive(encode_frame(1,seq,False,b'fixture'))
    except SustainedAudioBackpressure:self.fail('packet burst misclassified as a sustained clock stall')
   self.assertTrue(session.armed)
   self.assertEqual(session.backpressure_mic_packets,7)
   adapter.lib.queued=0;clock.now=.06
   session.receive(encode_frame(1,7,False,b'fixture'))
   self.assertEqual(session.backpressure_mic_streak,0)
  finally:session.close();bridge.close()

 def test_real_stall_still_fails_closed_after_elapsed_bound(self):
  adapter,bridge,session=self._session(NativeRing(queued=NativeRing.capacity))
  clock=Clock();session.clock=clock
  try:
   for seq,t in enumerate([0,.06,.12,.18,.24,.299]):
    clock.now=t;session.receive(encode_frame(1,seq,False,b'fixture'))
   clock.now=.301
   with self.assertRaises(SustainedAudioBackpressure):session.receive(encode_frame(1,6,False,b'fixture'))
   self.assertFalse(session.armed)
  finally:session.close();bridge.close()

 def test_success_and_playback_reset_elapsed_stall_budget(self):
  adapter,bridge,session=self._session(NativeRing(queued=NativeRing.capacity))
  clock=Clock();session.clock=clock
  try:
   session.receive(encode_frame(1,0,False,b'fixture'));clock.now=.25
   adapter.lib.queued=0;session.receive(encode_frame(1,1,False,b'fixture'))
   adapter.lib.queued=NativeRing.capacity
   for seq in range(2,8):session.receive(encode_frame(1,seq,False,b'fixture'))
   self.assertTrue(session.armed)
   adapter.playing=True;session.receive(encode_frame(1,8,False,b'fixture'));adapter.playing=False
   clock.now=.5
   for seq in range(9,15):session.receive(encode_frame(1,seq,False,b'fixture'))
   self.assertTrue(session.armed)
  finally:session.close();bridge.close()
