import unittest
from desk_robot.session_contract import Binding
from camera_adapter import LatestFrameAdapter

class Tests(unittest.TestCase):
    def setUp(self):
        self.now=0.;self.current=True;self.b=Binding('c','t',1,1)
        self.c=LatestFrameAdapter(lambda b:self.current and b==self.b,clock=lambda:self.now)
    def enable(self):self.c.enable(self.b,local_opt_in=True)
    def test_default_off_and_opt_in_is_not_device_activation(self):
        self.assertFalse(self.c.enabled)
        with self.assertRaises(PermissionError):self.c.push(self.b,0,0.,b'synthetic')
        with self.assertRaises(PermissionError):self.c.enable(self.b,local_opt_in=False)
        self.assertFalse(self.c.enabled)
    def test_latest_only_and_explicit_request_consumes_frame(self):
        self.enable();self.c.push(self.b,0,0.,b'old');self.c.push(self.b,1,0.,b'new')
        with self.assertRaises(PermissionError):self.c.take_requested(self.b,'r',visual_request=False)
        frame=self.c.take_requested(self.b,'r',visual_request=True)
        self.assertEqual(frame.payload,b'new');self.assertIsNone(self.c.take_requested(self.b,'r2',visual_request=True))
        self.assertNotIn('new',repr(frame))
    def test_stale_future_and_invalid_clock_fail_closed(self):
        self.enable();self.c.push(self.b,0,0.,b'a');self.now=.5
        with self.assertRaises(TimeoutError):self.c.take_requested(self.b,'r',visual_request=True)
        self.setUp();self.enable()
        with self.assertRaises(ValueError):self.c.push(self.b,0,1.,b'a')
        self.assertFalse(self.c.enabled)
        self.setUp();self.enable();self.now=float('nan')
        with self.assertRaises(ValueError):self.c.push(self.b,0,0.,b'a')
        self.assertFalse(self.c.enabled)
    def test_disable_revocation_and_foreign_binding_clear(self):
        self.enable();self.c.push(self.b,0,0.,b'a');self.c.disable();self.assertFalse(self.c.enabled)
        with self.assertRaises(PermissionError):self.c.take_requested(self.b,'r',visual_request=True)
        self.enable();self.c.push(self.b,0,0.,b'a');self.current=False
        with self.assertRaises(PermissionError):self.c.take_requested(self.b,'r',visual_request=True)
        self.assertFalse(self.c.enabled)
        self.setUp();self.enable()
        with self.assertRaises(PermissionError):self.c.push(Binding('c','old',1,1),0,0.,b'a')
    def test_size_sequence_and_request_replay_bounds(self):
        self.enable()
        with self.assertRaises(ValueError):self.c.push(self.b,0,0.,b'x'*65537)
        self.assertFalse(self.c.enabled)
        self.enable();self.c.push(self.b,0,0.,b'a')
        self.c.take_requested(self.b,'r',visual_request=True);self.c.push(self.b,1,0.,b'a')
        with self.assertRaises(PermissionError):self.c.take_requested(self.b,'r',visual_request=True)
        self.setUp();self.enable();self.c.push(self.b,0,0.,b'a')
        with self.assertRaises(ValueError):self.c.push(self.b,0,0.,b'a')
        self.assertFalse(self.c.enabled)
    def test_new_binding_requires_new_opt_in_not_reconnect(self):
        self.enable();self.c.push(self.b,0,0.,b'a');self.c.disable();self.b=Binding('c','new',2,2)
        with self.assertRaises(PermissionError):self.c.push(self.b,0,0.,b'a')
        self.assertFalse(self.c.enabled)

if __name__=='__main__':unittest.main()
