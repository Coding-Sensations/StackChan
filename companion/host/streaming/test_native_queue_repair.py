"""Deterministic synthetic native48k queues. No IO, sockets or audio content."""
import unittest
from types import SimpleNamespace
from native_conversation import NativeVoiceBridge,NativeConversationSession
from numeric_pairing_worker import _transport_projection

class Clock:
    now=0.
    def __call__(self):return self.now

class Adapter:
    def __init__(self,clock):
        self.clock=clock;self.epoch=0;self.muted=True;self.playing=False
        self.last=0.;self.queued=0;self.produced=0;self.pulled=0;self.dropped=0
    def begin(self,epoch):self.epoch=epoch
    def arm(self):self.muted=False;self.last=self.clock()
    def diagnostics(self):
        frames=round((self.clock()-self.last)*48000);self.last=self.clock()
        self.produced+=frames;total=self.queued+frames
        self.dropped+=max(0,total-7680);self.queued=min(7680,total)
        return dict(error=0,input_dropped=self.dropped,output_dropped=0,input_queue_frames=self.queued,input_frames=self.produced,output_consumed_frames=0)
    def pull_pcm(self,epoch,frames):
        actual=min(frames,self.queued);self.queued-=actual;self.pulled+=actual
        return bytes(actual*2)
    def robot_output_pcm(self,epoch,frames):
        return self.pull_pcm(epoch,frames).ljust(frames*2,b'\0')
    def set_playing(self,epoch,playing):self.playing=playing;return True
    def interrupt(self,epoch):self.muted=True;self.playing=False;return True
    def close(self):self.muted=True

class Tests(unittest.TestCase):
    def bridge(self):
        clock=Clock();adapter=Adapter(clock)
        component=SimpleNamespace(reset=lambda:None,close=lambda:None,process=lambda pcm:pcm[:len(pcm)//2],encode=lambda pcm:b'opus')
        bridge=NativeVoiceBridge(adapter,component,component,component,component,clock=clock)
        bridge.begin(1);adapter.arm()
        self.addCleanup(bridge.close)
        return clock,adapter,bridge
    def test_two_ms_tick_jitter_does_not_accumulate48k_queue(self):
        clock,adapter,bridge=self.bridge()
        for _ in range(200):
            clock.now+=.022
            self.assertEqual(bridge.poll_voice(1,960),(False,[]))
        self.assertEqual(adapter.dropped,0)
        self.assertEqual(adapter.queued,0)
        self.assertEqual(adapter.pulled,211200)
        self.assertEqual(bridge.return_frames,211200)
    def test_backlog_drain_remains_bounded_per_tick(self):
        clock,adapter,bridge=self.bridge();clock.now=.14
        self.assertEqual(bridge.poll_voice(1,960),(False,[]))
        self.assertLessEqual(adapter.pulled,5760)
        self.assertGreater(adapter.pulled,960)
        self.assertGreater(adapter.queued,0)
    def test_voiced_backlog_never_exceeds_two_packets_per_tick(self):
        clock,adapter,bridge=self.bridge();clock.now=.14
        original=adapter.pull_pcm
        def voiced(epoch,frames):
            data=original(epoch,frames)
            return b'\xd0\x07'*(len(data)//2)
        adapter.pull_pcm=voiced
        bridge.pending.extend(bytes(2878))
        active,packets=bridge.poll_voice(1,960)
        self.assertTrue(active);self.assertEqual(len(packets),2)
        self.assertLess(len(bridge.pending),2880)
        self.assertLessEqual(adapter.pulled,5760)
    def test_partial_and_empty_pull_counts_actual_frames_only(self):
        clock,adapter,bridge=self.bridge();clock.now=.03
        original=adapter.pull_pcm;calls=[]
        def partial(epoch,frames):
            calls.append(frames)
            return original(epoch,min(frames,320)) if len(calls)==1 else b''
        adapter.pull_pcm=partial
        self.assertEqual(bridge.poll_voice(1,960),(False,[]))
        self.assertEqual(bridge.return_frames,320)
        self.assertEqual(adapter.pulled,320)
        self.assertLessEqual(len(calls),3)
    def test_empty_queue_during_voice_hangover_does_not_resample_empty_pcm(self):
        clock,adapter,bridge=self.bridge();clock.now=.1;adapter.last=.1
        bridge.voice_until=.4;adapter.playing=True;bridge.pending.extend(b'pending')
        def reject_empty(pcm):
            if not pcm:raise ValueError('empty PCM rejected like real resampler')
            return pcm
        bridge.down=SimpleNamespace(process=reject_empty,reset=lambda:None,close=lambda:None)
        self.assertEqual(bridge.poll_voice(1,960),(True,[]))
        self.assertEqual(bridge.pending,b'pending')
        self.assertEqual(bridge.return_frames,0)
    def test_native_failure_keeps_numeric_counters_without_content(self):
        clock,adapter,bridge=self.bridge();clock.now=.2
        with self.assertRaises(RuntimeError):bridge.poll_voice(1,960)
        self.assertGreater(bridge.native_audio['input_dropped'],0)
        projection=_transport_projection({'native_audio':dict(bridge.native_audio,untrusted='speech'), 'native_conversation':True})
        self.assertGreater(projection['native_audio']['input_dropped'],0)
        self.assertNotIn('untrusted',projection['native_audio'])

if __name__=='__main__':unittest.main()
