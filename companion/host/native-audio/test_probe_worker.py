"""Synthetic finite-pump tests: no adapter library, audio devices or network IO."""
import importlib.util
import struct
import unittest
if importlib.util.find_spec('probe_worker'):
    import probe_worker
else:
    probe_worker = None

class Clock:
    def __init__(self): self.now=0.; self.on_tick=lambda:None
    def __call__(self): return self.now
    def sleep(self, seconds): self.now+=seconds; self.on_tick()

class FakeAdapter:
    """Bounded synthetic rings; captures return while accepting each uplink chunk."""
    def __init__(self, clock, *, short=False, drop=False, error=False, malformed=False):
        self.events=[]; self.short=short; self.drop=drop; self.error=error;self.malformed=malformed
        self.return_ring=bytearray(); self.output_queue=0
        self.counts=dict(input_callbacks=0,output_callbacks=0,input_frames=0,output_frames=0,
                         input_dropped=0,output_dropped=0,input_underruns=0,output_underruns=0,error=0)
        clock.on_tick=self.callback
    def callback(self):
        self.counts['output_callbacks']+=1
        self.counts['output_frames']+=self.output_queue
        self.output_queue=0
    def push_pcm(self, epoch, pcm):
        self.events.append(('push',epoch,len(pcm)//2))
        count=len(pcm)//2-(1 if self.short else 0)
        self.output_queue+=count
        returned=struct.pack('<256h', *([1000]*256))
        self.counts['input_callbacks']+=1;self.counts['input_frames']+=256
        room=512-len(self.return_ring)
        self.return_ring.extend(returned[:room])
        self.counts['input_dropped']+=(len(returned)-room)//2
        if self.drop:self.counts['input_dropped']+=1
        if self.error:self.counts['error']=1
        return count
    def pull_pcm(self, epoch, frames):
        self.events.append(('pull',epoch,frames))
        if self.malformed:return b'odd'
        result=bytes(self.return_ring[:frames*2]);del self.return_ring[:len(result)];return result
    def diagnostics(self):
        return dict(self.counts,output_consumed_frames=self.counts['output_frames'],
                    input_queue_frames=len(self.return_ring)//2,output_queue_frames=self.output_queue)

class ProbeWorkerTests(unittest.TestCase):
    def setUp(self): self.assertIsNotNone(probe_worker,'finite serialized probe pump is not implemented')
    def pump(self, adapter, clock, speech=None, **kwargs):
        return probe_worker.run_probe(adapter,b'\x01\0'*1440 if speech is None else speech,
                                      epoch=7,duration=.05,clock=clock,sleep=clock.sleep,**kwargs)

    def test_old_upload_then_drain_loses_return_in_bounded_fixture(self):
        clock=Clock();adapter=FakeAdapter(clock)
        for _ in range(3):adapter.push_pcm(7,b'\x01\0'*480);clock.sleep(.01)
        self.assertEqual(adapter.diagnostics()['input_dropped'],512)
        # This fixture proves the bug; the real worker must retain all 768 return frames.
        clock=Clock();adapter=FakeAdapter(clock);result=self.pump(adapter,clock)
        self.assertTrue(result.ok);self.assertEqual(result.returned_frames,768)
        self.assertEqual(result.diagnostics['input_dropped'],0)

    def test_return_is_drained_between_every_uplink_chunk(self):
        clock=Clock();adapter=FakeAdapter(clock);result=self.pump(adapter,clock)
        self.assertTrue(result.ok)
        pushes=[i for i,event in enumerate(adapter.events) if event[0]=='push']
        self.assertEqual(len(pushes),3)
        for left,right in zip(pushes,pushes[1:]):
            self.assertTrue(any(event[0]=='pull' for event in adapter.events[left+1:right]))
        self.assertEqual(result.requested_frames,1440);self.assertEqual(result.accepted_frames,1440)
        self.assertEqual(result.diagnostics['output_frames'],1440)

    def test_short_enqueue_fails_without_retry_and_preserves_actual_count(self):
        clock=Clock();adapter=FakeAdapter(clock,short=True);result=self.pump(adapter,clock)
        self.assertFalse(result.ok);self.assertEqual(result.reason,'short_enqueue')
        self.assertEqual(result.requested_frames,480);self.assertEqual(result.accepted_frames,479)
        self.assertEqual(len([e for e in adapter.events if e[0]=='push']),1)

    def test_drop_and_native_error_are_failures_even_with_return_energy(self):
        for flags,reason in [({'drop':True},'audio_dropped'),({'error':True},'native_error')]:
            with self.subTest(flags=flags):
                clock=Clock();adapter=FakeAdapter(clock,**flags);result=self.pump(adapter,clock)
                self.assertFalse(result.ok);self.assertEqual(result.reason,reason)
                self.assertEqual(len([e for e in adapter.events if e[0]=='push']),1)

    def test_return_energy_does_not_verify_persona_identity(self):
        clock=Clock();adapter=FakeAdapter(clock);result=self.pump(adapter,clock)
        self.assertTrue(result.ok);self.assertGreater(result.return_energy,0)
        self.assertFalse(result.as_dict()['identity_verified'])
        self.assertNotIn('return_pcm',result.as_dict())

    def test_speech_and_run_limits_deny_before_adapter_io(self):
        for speech,duration in [(b'\0'*960002,.05),(b'odd',.05),(b'',.05),(b'\0\0',40.01)]:
            with self.subTest(size=len(speech),duration=duration):
                clock=Clock();adapter=FakeAdapter(clock)
                result=probe_worker.run_probe(adapter,speech,epoch=7,duration=duration,clock=clock,sleep=clock.sleep)
                self.assertFalse(result.ok);self.assertEqual(adapter.events,[])

    def test_malformed_return_fails_without_fabricated_padding(self):
        clock=Clock();adapter=FakeAdapter(clock,malformed=True);result=self.pump(adapter,clock)
        self.assertFalse(result.ok);self.assertEqual(result.reason,'invalid_return')
        self.assertEqual(result.returned_frames,0)

    def test_diagnostics_are_required_and_drop_counters_never_ignored(self):
        clock=Clock();adapter=FakeAdapter(clock);adapter.counts.pop('input_dropped')
        result=self.pump(adapter,clock);self.assertFalse(result.ok)
        self.assertEqual(result.reason,'invalid_diagnostics');self.assertEqual(adapter.events,[])

    def test_finite_deadline_stops_without_automatic_lifecycle_calls(self):
        clock=Clock();adapter=FakeAdapter(clock);result=self.pump(adapter,clock)
        self.assertTrue(result.ok);self.assertLessEqual(clock.now,.06)
        self.assertTrue(all(e[0] in ('push','pull') for e in adapter.events))
        self.assertGreater(result.diagnostics['input_callbacks'],0)
        self.assertGreater(result.diagnostics['output_callbacks'],0)

    def test_native_exception_retains_final_error_diagnostics(self):
        clock=Clock();adapter=FakeAdapter(clock)
        def failed_push(epoch, pcm):
            adapter.counts['error']=-50
            adapter.counts['output_dropped']=3
            raise RuntimeError('untrusted native diagnostic details')
        adapter.push_pcm=failed_push
        result=self.pump(adapter,clock)
        self.assertFalse(result.ok)
        self.assertEqual(result.diagnostics['error'],-50)
        self.assertEqual(result.diagnostics['output_dropped'],3)
        self.assertNotIn('untrusted',repr(result.as_dict()))

    def test_process_alarm_is_finite_and_previous_handler_is_restored(self):
        import signal
        from unittest.mock import patch
        old=signal.getsignal(signal.SIGALRM)
        clock=Clock();adapter=FakeAdapter(clock)
        with patch('signal.alarm',wraps=signal.alarm) as alarm:
            result=self.pump(adapter,clock)
        self.assertTrue(result.ok)
        self.assertEqual([call.args for call in alarm.call_args_list],[(50,),(0,)])
        self.assertEqual(signal.getsignal(signal.SIGALRM),old)
        self.assertEqual(signal.getitimer(signal.ITIMER_REAL),(0.,0.))

    def test_frozen_clock_cannot_create_unbounded_pump(self):
        clock=Clock();adapter=FakeAdapter(clock)
        result=probe_worker.run_probe(adapter,b'\x01\0',epoch=7,duration=.05,
                                      clock=clock,sleep=lambda seconds:None)
        self.assertFalse(result.ok);self.assertEqual(result.reason,'clock_stalled')
        self.assertLess(len(adapter.events),9000)

    def test_result_retains_measurements_without_raw_return_audio(self):
        clock=Clock();adapter=FakeAdapter(clock);result=self.pump(adapter,clock)
        self.assertTrue(result.ok)
        self.assertEqual(result.returned_frames,768)
        self.assertEqual(result.return_peak,1000)
        self.assertEqual(result.return_energy,768000000)
        self.assertFalse(any(isinstance(value,(bytes,bytearray,memoryview)) for value in vars(result).values()))

    def test_capture_after_final_pull_is_reported_without_false_failure(self):
        clock=Clock();adapter=FakeAdapter(clock)
        original=adapter.diagnostics;arrived=False
        def capture_during_snapshot():
            nonlocal arrived
            if clock.now>=.05 and not arrived:
                arrived=True
                adapter.return_ring.extend(b'\x01\0'*7)
                adapter.counts['input_callbacks']+=1
                adapter.counts['input_frames']+=7
            return original()
        adapter.diagnostics=capture_during_snapshot
        result=self.pump(adapter,clock)
        self.assertTrue(result.ok)
        self.assertEqual(result.returned_frames,768)
        self.assertEqual(result.diagnostics['input_queue_frames'],7)
        self.assertEqual(result.diagnostics['input_dropped'],0)

    def test_unconsumed_uplink_remains_a_failure(self):
        clock=Clock();adapter=FakeAdapter(clock)
        original=adapter.diagnostics
        def pending_output():
            if clock.now>=.05:adapter.output_queue=3
            return original()
        adapter.diagnostics=pending_output
        result=self.pump(adapter,clock)
        self.assertFalse(result.ok);self.assertEqual(result.reason,'pending_uplink')
        self.assertEqual(result.diagnostics['output_queue_frames'],3)

    def test_empty_output_queue_does_not_hide_unconsumed_enqueued_frames(self):
        clock=Clock();adapter=FakeAdapter(clock)
        original=adapter.diagnostics
        def underreported_consumption():
            result=original()
            result['output_consumed_frames']=max(0,result['output_consumed_frames']-1)
            return result
        adapter.diagnostics=underreported_consumption
        result=self.pump(adapter,clock)
        self.assertFalse(result.ok);self.assertEqual(result.reason,'uplink_consumption_mismatch')
        self.assertEqual(result.accepted_frames,1440)
        self.assertEqual(result.consumed_frames,1439)

    def test_consumption_counter_is_mandatory_before_transport(self):
        clock=Clock();adapter=FakeAdapter(clock)
        original=adapter.diagnostics
        def missing_counter():
            result=original();result.pop('output_consumed_frames');return result
        adapter.diagnostics=missing_counter
        result=self.pump(adapter,clock)
        self.assertFalse(result.ok);self.assertEqual(result.reason,'invalid_diagnostics')
        self.assertEqual(adapter.events,[])

    def test_consumption_uses_initial_counter_baseline(self):
        clock=Clock();adapter=FakeAdapter(clock)
        adapter.counts['output_frames']=200
        result=self.pump(adapter,clock)
        self.assertTrue(result.ok)
        self.assertEqual(getattr(result,'consumed_frames',None),1440)
        self.assertEqual(result.diagnostics['output_consumed_frames'],1640)

if __name__=='__main__':unittest.main()
