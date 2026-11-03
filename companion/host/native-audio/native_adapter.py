"""Worker-only binding. Constructing/loading this module never opens device IO.
Explicit open_for_approved_test() is separate from arm_for_approved_test().
Use CodecHostBridge for already-tested Opus/resampling; no Python callback runs
on a CoreAudio thread. One serialized worker/control owner required.
"""
import ctypes as C
from streaming_session import AudioBackpressure

TO_VOICE = 'Pro Tools Audio Bridge 2-A_UID'
FROM_VOICE = 'Pro Tools Audio Bridge 2-B_UID'
UPLINK_QUEUE_CAPACITY = 7680

class NativeHostAdapter:
    def __init__(self, library):
        self.lib=C.CDLL(str(library)); self.handle=C.c_void_p()
        self.epoch=0; self.muted=True; self.playing=False
        self.uplink_backpressure_dropped=0
        self.close_status=0;self.retained_units=0;self.close_errors={}
        specs={
            'sc_open':([C.c_char_p,C.c_char_p,C.POINTER(C.c_void_p)],C.c_int),
            'sc_start':([C.c_void_p],C.c_int),
            'sc_begin':([C.c_void_p,C.c_uint32],C.c_int),
            'sc_arm':([C.c_void_p,C.c_uint32],C.c_int),
            'sc_mute':([C.c_void_p,C.c_uint32],C.c_int),
            'sc_playing':([C.c_void_p,C.c_uint32,C.c_int],C.c_int),
            'sc_push':([C.c_void_p,C.c_uint32,C.POINTER(C.c_int16),C.c_uint32],C.c_int),
            'sc_pull':([C.c_void_p,C.c_uint32,C.POINTER(C.c_int16),C.c_uint32],C.c_int),
            'sc_error':([C.c_void_p],C.c_int),
            'sc_counter':([C.c_void_p,C.c_int,C.c_int],C.c_uint64),
            'sc_diagnostic':([C.c_void_p,C.c_int],C.c_uint64),
            'sc_close':([C.c_void_p],None),
            'sc_close_checked':([C.POINTER(C.c_void_p),C.POINTER(C.c_uint32)],C.c_int),
            'sc_close_unit_status':([C.c_void_p,C.c_int,C.c_int],C.c_int),
        }
        for name,(args,result) in specs.items():
            function=getattr(self.lib,name); function.argtypes=args; function.restype=result
    def _require(self):
        if not self.handle.value: raise RuntimeError('device IO has not been explicitly opened')
    def _check(self,status):
        if status: raise RuntimeError(f'CoreAudio bridge status {status}')
    def open_for_approved_test(self,*,cancelled=None):
        def check_cancelled():
            if cancelled is not None and cancelled():raise RuntimeError('native startup cancelled')
        check_cancelled()
        if self.handle.value: raise RuntimeError('already open')
        self._check(self.lib.sc_open(TO_VOICE.encode(),FROM_VOICE.encode(),C.byref(self.handle)))
        try:
            check_cancelled()
            self._check(self.lib.sc_start(self.handle))
            check_cancelled()
        except BaseException: self.close(); raise
    def open_directional_for_approved_test(self,*,cancelled=None):
        """Explicit HAL opt-in, selected only by a trusted directional worker binding.

        Uses hidden Feed output -> public Microphone input, public Speaker output
        -> hidden Return input. Installing a driver and opening IO need approval.
        """
        def check_cancelled():
            if cancelled is not None and cancelled():raise RuntimeError('native startup cancelled')
        check_cancelled()
        if self.handle.value: raise RuntimeError('already open')
        function = self.lib.sc_open_directional
        function.argtypes = [C.POINTER(C.c_void_p)]; function.restype = C.c_int
        self._check(function(C.byref(self.handle)))
        try:
            check_cancelled()
            self._check(self.lib.sc_start(self.handle))
            check_cancelled()
        except BaseException: self.close(); raise
    def begin(self,epoch):
        self._require()
        if type(epoch) is not int or not self.epoch<epoch<=0xffffffff: raise ValueError('new uint32 epoch required')
        self._check(self.lib.sc_begin(self.handle,epoch))
        self.epoch=epoch; self.muted=True; self.playing=False
    def arm_for_approved_test(self,epoch):
        self._require()
        if epoch!=self.epoch: raise ValueError('stale epoch')
        self._check(self.lib.sc_arm(self.handle,epoch)); self.muted=False
    def set_playing(self,epoch,playing):
        self._require()
        if type(playing) is not bool: raise ValueError('playing flag')
        if epoch!=self.epoch: return False
        self._check(self.lib.sc_playing(self.handle,epoch,int(playing))); self.playing=playing; return True
    def push_pcm(self,epoch,pcm):
        self._require()
        if type(pcm) is not bytes or not pcm or len(pcm)%2 or len(pcm)>4096:raise ValueError('bounded PCM16 chunk <=2048 frames')
        if epoch!=self.epoch or self.muted or self.playing:raise RuntimeError('uplink authority unavailable')
        samples=(C.c_int16*(len(pcm)//2)).from_buffer_copy(pcm)
        count=self.lib.sc_push(self.handle,epoch,samples,len(samples))
        if count<0 or count>len(samples):raise RuntimeError('native uplink rejected')
        return count
    def pull_pcm(self,epoch,frames):
        self._require()
        if type(frames) is not int or not 1<=frames<=2048:raise ValueError('frame bound')
        if epoch!=self.epoch or self.muted:raise RuntimeError('downlink authority unavailable')
        samples=(C.c_int16*frames)();count=self.lib.sc_pull(self.handle,epoch,samples,frames)
        if count<0 or count>frames:raise RuntimeError('native downlink rejected')
        return bytes(samples)[:count*2]
    def diagnostics(self):
        self._require()
        keys={'output_callbacks':0,'output_frames':1,'output_consumed_frames':2,'input_callbacks':3,'input_frames':4,'output_queue_frames':5,'input_queue_frames':6}
        result={key:self.lib.sc_diagnostic(self.handle,value) for key,value in keys.items()}
        # Both sources count mono48k PCM sample frames in robot_to_voice.
        native_output_dropped=self.lib.sc_counter(self.handle,0,0)
        software_output_dropped=getattr(self,'uplink_backpressure_dropped',0)
        result.update(error=self.lib.sc_error(self.handle),
                      output_dropped=min(0x7fffffffffffffff,native_output_dropped+software_output_dropped),
                      input_dropped=self.lib.sc_counter(self.handle,1,0),
                      output_underruns=self.lib.sc_counter(self.handle,0,2),
                      input_underruns=self.lib.sc_counter(self.handle,1,2))
        return result
    def robot_pcm(self,epoch,pcm):
        self._require()
        if type(pcm) is not bytes or not pcm or len(pcm)%2 or len(pcm)>11520: raise ValueError('bounded PCM16 chunk <=120ms')
        if epoch!=self.epoch or self.muted or self.playing: return False
        frames=len(pcm)//2
        queued=self.lib.sc_diagnostic(self.handle,5)
        if type(queued) is not int or not 0<=queued<=UPLINK_QUEUE_CAPACITY:
            raise RuntimeError('bounded native uplink queue unavailable')
        # One render callback consumes this SPSC ring; it cannot increase the
        # observed occupancy between the snapshot and push. Reject whole framesets
        # before sc_push so a full ring never accepts a partial PCM packet.
        if frames>UPLINK_QUEUE_CAPACITY-queued:
            self.uplink_backpressure_dropped=min(0x7fffffffffffffff,
                self.uplink_backpressure_dropped+frames)
            raise AudioBackpressure('bounded native uplink queue full')
        for start in range(0,len(pcm),4096):
            chunk=pcm[start:start+4096]
            samples=(C.c_int16*(len(chunk)//2)).from_buffer_copy(chunk)
            if self.lib.sc_push(self.handle,epoch,samples,len(samples))!=len(samples):
                raise RuntimeError('native uplink queue changed during bounded push')
        return True
    def robot_output_pcm(self,epoch,frames):
        self._require()
        if type(frames) is not int or not 1<=frames<=2048: raise ValueError('frame bound')
        samples=(C.c_int16*frames)()
        if epoch==self.epoch and not self.muted:
            count=self.lib.sc_pull(self.handle,epoch,samples,frames)
            if count<0: self.muted=True
        return bytes(samples)
    def interrupt(self,epoch):
        self._require()
        if epoch!=self.epoch: return False
        self._check(self.lib.sc_mute(self.handle,epoch)); self.muted=True; self.playing=False; return True
    def stop_and_confirm_closed(self):
        self.muted=True;self.playing=False
        if not self.handle.value:return True
        retained=C.c_uint32()
        self.close_status=self.lib.sc_close_checked(C.byref(self.handle),C.byref(retained))
        self.retained_units=retained.value
        self.close_errors={} if not self.handle.value else {f'{direction}_{phase}':self.lib.sc_close_unit_status(self.handle,direction,phase) for direction in (0,1) for phase in (0,1,2)}
        if self.close_status==0 and self.handle.value:
            raise RuntimeError('checked close succeeded without clearing native handle')
        return self.close_status==0 and not self.handle.value
    def close(self):
        if not self.stop_and_confirm_closed():
            raise RuntimeError(f'CoreAudio cleanup incomplete: status {self.close_status}, retained mask {self.retained_units}')
