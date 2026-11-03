"""Inactive, volatile latest-frame policy. No camera open, disk, log or dispatch.

The existing owner must separately authorize capture and any transport scope.
A local opt-in/visual-request input is not a new credential or model permission.
"""
from dataclasses import dataclass,field
from threading import RLock
import math,time
from desk_robot.session_contract import Binding

@dataclass(frozen=True)
class LatestFrame:
    binding: Binding
    sequence: int
    captured_at: float
    payload: bytes = field(repr=False)

class LatestFrameAdapter:
    def __init__(self,is_current,*,clock=time.monotonic):
        if not callable(is_current) or not callable(clock):raise ValueError('existing owner required')
        self._current=is_current;self._clock=clock;self._lock=RLock()
        self._binding=None;self._frame=None;self._sequence=-1;self._seen=set();self._last_time=None
    @property
    def enabled(self):
        with self._lock:return self._binding is not None
    def _disable(self):
        self._binding=None;self._frame=None;self._sequence=-1;self._seen.clear()
    def disable(self):
        with self._lock:self._disable()
    def _now(self):
        try:
            now=self._clock()
            valid=(type(now) in (int,float) and math.isfinite(now) and now>=0
                   and (self._last_time is None or now>=self._last_time))
        except Exception:valid=False
        if not valid:self._disable();raise ValueError('valid monotonic time required')
        self._last_time=now;return now
    def _verify(self,binding):
        try:valid=type(binding) is Binding and self._current(binding) is True
        except Exception:valid=False
        if not valid or self._binding!=binding:
            self._disable();raise PermissionError('current opted-in camera binding required')
        return self._now()
    def enable(self,binding,*,local_opt_in):
        with self._lock:
            if local_opt_in is not True or type(binding) is not Binding:raise PermissionError('explicit local camera opt-in required')
            try:current=self._current(binding) is True
            except Exception:current=False
            if not current:self._disable();raise PermissionError('current owner required')
            self._now();self._disable();self._binding=binding
    def push(self,binding,sequence,captured_at,payload):
        with self._lock:
            now=self._verify(binding)
            try:
                if (type(sequence) is not int or not self._sequence<sequence<0xffffffff
                        or type(payload) is not bytes or not 1<=len(payload)<=65536
                        or type(captured_at) not in (int,float) or not math.isfinite(captured_at)
                        or captured_at<0 or captured_at>now):raise ValueError('bounded current frame required')
                if now-captured_at>=.5:raise TimeoutError('stale camera frame')
            except (ValueError,OverflowError,TimeoutError):self._disable();raise
            self._sequence=sequence;self._frame=LatestFrame(binding,sequence,captured_at,payload)
    def take_requested(self,binding,request_id,*,visual_request):
        with self._lock:
            now=self._verify(binding)
            if visual_request is not True:raise PermissionError('explicit visual request required')
            if (type(request_id) is not str or not 1<=len(request_id)<=128
                    or any(not c.isascii() or not(c.isalnum() or c in '-_.:') for c in request_id)):
                raise ValueError('bounded visual request required')
            if request_id in self._seen:raise PermissionError('visual request replay')
            if len(self._seen)>=16:raise BufferError('visual request budget exhausted')
            if self._frame is None:return None
            if now-self._frame.captured_at>=.5:self._disable();raise TimeoutError('stale camera frame')
            frame=self._frame;self._frame=None;self._seen.add(request_id)
            return frame
