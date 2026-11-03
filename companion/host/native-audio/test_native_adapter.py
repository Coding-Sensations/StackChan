import ctypes as C
import unittest
from native_adapter import NativeHostAdapter
class Lib:
 def __init__(self):self.accept=3;self.available=2
 def sc_push(self,handle,epoch,buf,n):return self.accept
 def sc_pull(self,handle,epoch,buf,n):
  for i in range(min(n,self.available)):buf[i]=i+1
  return self.available
 def sc_diagnostic(self,handle,key):return key+10
 def sc_counter(self,handle,direction,key):return direction*10+key
 def sc_error(self,handle):return 0
class Tests(unittest.TestCase):
 def setUp(self):
  self.a=NativeHostAdapter.__new__(NativeHostAdapter);self.a.lib=Lib();self.a.handle=C.c_void_p(1);self.a.epoch=1;self.a.muted=False;self.a.playing=False
 def test_short_enqueue_count_preserved(self):self.assertEqual(self.a.push_pcm(1,b'\0'*8),3)
 def test_actual_pull_count_only(self):self.assertEqual(self.a.pull_pcm(1,4),b'\1\0\2\0')
 def test_no_samples_is_empty(self):self.a.lib.available=0;self.assertEqual(self.a.pull_pcm(1,4),b'')
 def test_invalid_native_counts_fail(self):
  for n in[-1,5]:
   self.a.lib.accept=n
   with self.assertRaises(RuntimeError):self.a.push_pcm(1,b'\0'*8)
   self.a.lib.available=n
   with self.assertRaises(RuntimeError):self.a.pull_pcm(1,4)
 def test_stale_or_muted_rejected(self):
  with self.assertRaises(RuntimeError):self.a.push_pcm(2,b'\0'*8)
  self.a.muted=True
  with self.assertRaises(RuntimeError):self.a.pull_pcm(1,4)
 def test_diagnostics_mapping(self):
  d=self.a.diagnostics();self.assertEqual(d['output_callbacks'],10);self.assertEqual(d['output_consumed_frames'],12);self.assertEqual(d['input_callbacks'],13);self.assertEqual(d['output_dropped'],0);self.assertEqual(d['input_dropped'],10)
 def test_directional_open_is_explicit_and_starts_only_after_success(self):
  calls=[]
  class Open:
   def __call__(self,ptr):calls.append('directional');C.cast(ptr,C.POINTER(C.c_void_p))[0]=C.c_void_p(123);return 0
  self.a.lib.sc_open_directional=Open();self.a.lib.sc_start=lambda ptr:calls.append(('start',ptr.value)) or 0
  self.a.handle=C.c_void_p();self.a.open_directional_for_approved_test()
  self.assertEqual(calls,['directional',('start',123)])
  with self.assertRaises(RuntimeError):self.a.open_directional_for_approved_test()
 def test_directional_open_failure_never_starts(self):
  class Open:
   def __call__(self,ptr):return -1
  self.a.lib.sc_open_directional=Open();self.a.handle=C.c_void_p()
  self.a.lib.sc_start=lambda ptr:self.fail('must not start')
  with self.assertRaises(RuntimeError):self.a.open_directional_for_approved_test()
 def test_directional_start_failure_preserves_checked_cleanup(self):
  class Open:
   def __call__(self,ptr):C.cast(ptr,C.POINTER(C.c_void_p))[0]=C.c_void_p(123);return 0
  self.a.lib.sc_open_directional=Open();self.a.handle=C.c_void_p()
  self.a.lib.sc_start=lambda ptr:-1
  closed=[];self.a.close=lambda:closed.append(True)
  with self.assertRaises(RuntimeError):self.a.open_directional_for_approved_test()
  self.assertEqual(closed,[True])
if __name__=='__main__':unittest.main()
