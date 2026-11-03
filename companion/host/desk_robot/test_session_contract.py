import unittest
from session_contract import Binding, State, TurnContract, MemoryContext

class ContractTests(unittest.TestCase):
    def setUp(self):
        self.now=0.0
        self.binding=Binding('conversation', 'turn', 1, 1)
        self.current=True
        self.c=TurnContract(self.binding, lambda b:self.current and b==self.binding, clock=lambda:self.now)
    def listen(self):
        self.c.physical_listen(self.binding, physical=True)
    def playback(self):
        self.listen(); self.c.thinking(self.binding); self.c.start_playback(self.binding)
    def test_readonly_binding_and_foreign_access_preserve_queue_until_revocation(self):
        self.playback();self.c.enqueue(self.binding,0,b'old')
        new=Binding('conversation','new-turn',2,2)
        with self.assertRaises(AttributeError):self.c.binding=new
        with self.assertRaises(PermissionError):self.c.take(new)
        self.assertEqual(self.c.queued_bytes,3)
        self.binding=new
        self.assertFalse(self.c.capture_allowed)
        self.assertEqual(self.c.queued_bytes,0)
    def test_binding_is_inert_and_hard_muted(self):
        self.assertEqual(self.c.state,State.HARD_MUTED)
        self.assertFalse(self.c.capture_allowed)
    def test_only_physical_listen_can_activate(self):
        with self.assertRaises(PermissionError):self.c.physical_listen(self.binding,physical=False)
        self.listen(); self.assertTrue(self.c.capture_allowed)
        self.c.thinking(self.binding); self.assertFalse(self.c.capture_allowed)
    def test_foreign_and_revoked_bindings_cannot_activate(self):
        with self.assertRaises(PermissionError):self.c.physical_listen(Binding('conversation','old',1,1),physical=True)
        self.current=False
        with self.assertRaises(PermissionError):self.listen()
        self.assertEqual(self.c.state,State.HARD_MUTED)
    def test_playback_drains_only_after_end_sent_and_actual_receipt(self):
        self.playback(); self.c.enqueue(self.binding,0,b'opus'); self.c.end_playback(self.binding,0)
        with self.assertRaises(PermissionError):self.c.drained(self.binding,0)
        self.assertEqual(self.c.take(self.binding),(0,b'opus'))
        self.assertEqual(self.c.state,State.DRAINING)
        self.assertFalse(self.c.capture_allowed)
        self.assertTrue(self.c.drained(self.binding,0))
        self.assertEqual(self.c.state,State.HARD_MUTED)
        self.assertFalse(self.c.drained(self.binding,0))
    def test_wrong_drain_receipt_does_not_complete(self):
        self.playback();self.c.enqueue(self.binding,0,b'a');self.c.take(self.binding);self.c.end_playback(self.binding,0)
        with self.assertRaises(PermissionError):self.c.drained(self.binding,1)
        with self.assertRaises(PermissionError):self.c.drained(Binding('conversation','old',1,1),0)
        self.assertEqual(self.c.state,State.DRAINING)
    def test_queue_item_and_byte_bounds_surface_backpressure(self):
        self.playback()
        for i in range(8):self.c.enqueue(self.binding,i,b'x'*4096)
        with self.assertRaises(BufferError):self.c.enqueue(self.binding,8,b'x')
        self.assertEqual(self.c.queued_bytes,32768)
        self.c.take(self.binding); self.c.enqueue(self.binding,8,b'x')
    def test_gap_cancels_turn_and_discards_media(self):
        self.playback();self.c.enqueue(self.binding,0,b'a')
        with self.assertRaises(ValueError):self.c.enqueue(self.binding,2,b'b')
        self.assertEqual(self.c.queued_bytes,0);self.assertEqual(self.c.state,State.CANCELLED)
        with self.assertRaises(PermissionError):self.listen()
    def test_cancel_and_disconnect_are_terminal(self):
        for action,state in [('cancel',State.CANCELLED),('disconnect',State.DISCONNECTED)]:
            self.setUp();self.playback();self.c.enqueue(self.binding,0,b'a');getattr(self.c,action)()
            self.assertEqual(self.c.state,state);self.assertEqual(self.c.queued_bytes,0)
            with self.assertRaises(PermissionError):self.listen()
    def test_hard_mute_flushes_and_requires_fresh_physical_listen(self):
        self.playback();self.c.enqueue(self.binding,0,b'a');self.c.hard_mute()
        self.assertFalse(self.c.capture_allowed);self.assertEqual(self.c.queued_bytes,0)
        with self.assertRaises(PermissionError):self.c.drained(self.binding,0)
        # A contract represents one turn; muted partial turn cannot resurrect.
        with self.assertRaises(PermissionError):self.listen()
    def test_expiry_revocation_clear_queue_and_capture(self):
        self.playback();self.c.enqueue(self.binding,0,b'a');self.current=False
        with self.assertRaises(PermissionError):self.c.take(self.binding)
        self.assertEqual(self.c.queued_bytes,0);self.assertFalse(self.c.capture_allowed)
    def test_drain_timeout_is_incomplete_not_success(self):
        self.playback();self.c.enqueue(self.binding,0,b'a');self.c.take(self.binding);self.c.end_playback(self.binding,0)
        self.now=2.0
        with self.assertRaises(TimeoutError):self.c.drained(self.binding,0)
        self.assertEqual(self.c.state,State.CANCELLED)
    def test_empty_output_has_explicit_end_receipt(self):
        self.playback();self.c.end_playback(self.binding,-1)
        self.assertTrue(self.c.drained(self.binding,-1))
    def test_invalid_inputs_do_not_enter_queue(self):
        for args in [('x','y',True,1),('x','y',1,True),('x'*129,'y',1,1)]:
            with self.assertRaises(ValueError):Binding(*args)
        self.playback()
        for seq,payload in [(True,b'a'),(0,b''),(0,b'x'*4097),(0,'x')]:
            with self.assertRaises(ValueError):self.c.enqueue(self.binding,seq,payload)
        self.assertEqual(self.c.queued_bytes,0)
    def test_clock_failure_permanently_cancels_capture(self):
        for value in (10**400, 'raise'):
            self.setUp(); self.listen()
            def bad_clock():
                if value == 'raise': raise RuntimeError('clock unavailable')
                return value
            self.c._clock=bad_clock
            self.assertFalse(self.c.capture_allowed)
            self.assertEqual(self.c.state,State.CANCELLED)
            self.c._clock=lambda:0.0
            self.assertFalse(self.c.capture_allowed)
    def test_drain_deadline_starts_after_final_take_and_queue_has_own_bound(self):
        self.playback();self.c.enqueue(self.binding,0,b'a');self.c.end_playback(self.binding,0)
        self.now=2.5
        self.assertEqual(self.c.take(self.binding),(0,b'a'))
        self.now=4.4;self.assertTrue(self.c.drained(self.binding,0))
        self.setUp();self.playback();self.c.enqueue(self.binding,0,b'a');self.c.end_playback(self.binding,0)
        self.now=4.0
        with self.assertRaises(TimeoutError):self.c.take(self.binding)
    def test_mute_and_take_are_serialized(self):
        import threading
        from collections import deque
        entered=threading.Event();release=threading.Event();muted=threading.Event();results=[]
        class HeldQueue(deque):
            def popleft(self):
                entered.set();release.wait(2);return super().popleft()
        self.playback();self.c.enqueue(self.binding,0,b'a');self.c._queue=HeldQueue(self.c._queue)
        def take():
            try:results.append(self.c.take(self.binding))
            except Exception as e:results.append(type(e).__name__)
        def mute():self.c.hard_mute();muted.set()
        t=threading.Thread(target=take);t.start();self.assertTrue(entered.wait(1))
        m=threading.Thread(target=mute);m.start()
        try:self.assertFalse(muted.wait(.02))
        finally:release.set();t.join(2);m.join(2)
        self.assertEqual(results,[(0,b'a')]);self.assertTrue(muted.is_set())
        self.assertEqual(self.c.queued_bytes,0);self.assertFalse(self.c.capture_allowed)
    def test_terminal_reason_survives_later_revocation_and_stop(self):
        self.c.disconnect();self.current=False;self.assertFalse(self.c.capture_allowed)
        self.c.cancel();self.assertEqual(self.c.state,State.DISCONNECTED)
    def test_owner_expiry_closes_listening_without_local_renewal(self):
        self.c._is_current=lambda b:self.now<5
        self.listen();self.now=5;self.assertFalse(self.c.capture_allowed)
    def test_external_authority_validation_serializes_with_take(self):
        import threading
        from collections import deque
        entered=threading.Event();release=threading.Event();validated=threading.Event();results=[];errors=[]
        class HeldQueue(deque):
            def popleft(self):entered.set();release.wait(2);return super().popleft()
        self.playback();self.c.enqueue(self.binding,0,b'a');self.c._queue=HeldQueue(self.c._queue)
        def take():
            try:results.append(self.c.take(self.binding))
            except Exception as e:results.append(type(e).__name__)
        def validate():
            try:self.c.validate(self.binding)
            except Exception as e:errors.append(type(e).__name__)
            finally:validated.set()
        t=threading.Thread(target=take);t.start();self.assertTrue(entered.wait(1));self.current=False
        v=threading.Thread(target=validate);v.start()
        try:self.assertFalse(validated.wait(.02))
        finally:release.set();t.join(2);v.join(2)
        self.assertEqual(results,[(0,b'a')]);self.assertEqual(errors,['PermissionError']);self.assertEqual(self.c.queued_bytes,0)
    def test_nonfinite_clock_rejected(self):
        with self.assertRaises(ValueError):TurnContract(self.binding,lambda b:True,clock=lambda:float('nan'))

class ContextTests(unittest.TestCase):
    def test_context_has_byte_and_count_bound_and_original_persona(self):
        c=MemoryContext(persona='Be a concise, curious companion.')
        for i in range(25):c.append('user',str(i))
        self.assertEqual(len(c.snapshot()),20);self.assertEqual(c.snapshot()[0],('user','5'))
        for i in range(20):c.append('assistant','é'*2048)
        self.assertLessEqual(c.size_bytes,32768)
        self.assertEqual(len(c.snapshot()),8)
        c.clear();self.assertEqual(c.snapshot(),());self.assertEqual(c.size_bytes,0)
    def test_nonstring_role_cannot_escape_context_budget(self):
        class Role:
            payload=b'x'*100000
            def __eq__(self,other):return True
        c=MemoryContext()
        with self.assertRaises(ValueError):c.append(Role(),'x')
        self.assertEqual(c.snapshot(),())
    def test_invalid_context_and_persona_rejected(self):
        with self.assertRaises(ValueError):MemoryContext(persona='x'*2049)
        c=MemoryContext()
        for role,text in [('system','x'),('user','é'*2049),('user',''),('user',None)]:
            with self.assertRaises(ValueError):c.append(role,text)
        self.assertEqual(c.snapshot(),())

if __name__=='__main__':unittest.main()
