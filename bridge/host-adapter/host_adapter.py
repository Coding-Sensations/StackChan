"""Synthetic host callback boundary. No CoreAudio IO, settings, model or sockets.
PCM arriving here is already converted to mono48k; Opus conversion remains in
existing tested local endpoint. This cannot authorize or start a live route.
"""
from dataclasses import dataclass
from threading import Lock

@dataclass(frozen=True)
class RoutePlan:
    robot_to_dot_bus_uid:str
    dot_to_robot_bus_uid:str
    sample_rate:int=48000
    channels:int=1
    def __post_init__(self):
        if any(type(x) is not str or not 1<=len(x)<=128 for x in (self.robot_to_dot_bus_uid,self.dot_to_robot_bus_uid)) or self.robot_to_dot_bus_uid==self.dot_to_robot_bus_uid:
            raise ValueError('two distinct explicit bus UIDs required')
        if type(self.sample_rate) is not int or self.sample_rate!=48000 or type(self.channels) is not int or self.channels!=1:raise ValueError('mono48k PCM16LE only')

class SyntheticHostAdapter:
    capacity_samples=7680 #160ms per direction; no unlimited backlog.
    def __init__(self,plan):
        if not isinstance(plan,RoutePlan):raise ValueError('route plan')
        self.plan=plan;self.epoch=0;self.muted=True;self.playing=False
        self._input=bytearray();self._output=bytearray();self._lock=Lock()
        self.metrics={'underrun_samples':0,'discarded_samples':0,'stale_callbacks':0}
    def begin(self,epoch):
        with self._lock:
            if type(epoch) is not int or not self.epoch<epoch<=0xffffffff:raise ValueError('new uint32 session epoch required')
            self.epoch=epoch;self.muted=True;self.playing=False;self._input.clear();self._output.clear()
    def _current(self,epoch):
        if type(epoch) is not int or epoch!=self.epoch or self.epoch==0:
            self.metrics['stale_callbacks']+=1;return False
        return True
    def unmute_synthetic(self,epoch):
        with self._lock:
            if not self._current(epoch):raise ValueError('stale epoch')
            self.muted=False
    def interrupt(self,epoch):
        with self._lock:
            if not self._current(epoch):return False
            self.muted=True;self.playing=False;self._input.clear();self._output.clear();return True
    def set_playing(self,epoch,playing):
        if type(playing) is not bool:raise ValueError('playing flag')
        with self._lock:
            if not self._current(epoch):return False
            self.playing=playing
            if playing:self._input.clear()
            return True
    @staticmethod
    def _validate_pcm(pcm):
        if type(pcm) is not bytes or not len(pcm) or len(pcm)%2 or len(pcm)>11520:raise ValueError('bounded PCM16LE chunk <=120ms')
    def _push(self,queue,pcm):
        queue.extend(pcm);overflow=max(0,len(queue)-self.capacity_samples*2)
        if overflow:del queue[:overflow];self.metrics['discarded_samples']+=overflow//2
    @staticmethod
    def _validate_frames(frames):
        if type(frames) is not int or not 1<=frames<=2048:raise ValueError('callback frame count')
    def _pull(self,queue,frames):
        n=min(len(queue),frames*2);pcm=bytes(queue[:n]);del queue[:n]
        missing=frames-n//2;self.metrics['underrun_samples']+=missing
        return pcm+b'\0'*(missing*2)
    def robot_pcm(self,epoch,pcm):
        self._validate_pcm(pcm)
        with self._lock:
            if not self._current(epoch) or self.muted or self.playing:return False
            self._push(self._input,pcm);return True
    def dot_input_callback(self,epoch,frames):
        self._validate_frames(frames)
        with self._lock:
            if not self._current(epoch) or self.muted:return b'\0'*(frames*2)
            return self._pull(self._input,frames)
    def dot_output_callback(self,epoch,pcm):
        self._validate_pcm(pcm)
        with self._lock:
            if not self._current(epoch) or self.muted:return False
            self._push(self._output,pcm);return True
    def robot_output_pcm(self,epoch,frames):
        self._validate_frames(frames)
        with self._lock:
            if not self._current(epoch) or self.muted:return b'\0'*(frames*2)
            return self._pull(self._output,frames)
    @property
    def buffered_samples(self):
        with self._lock:return {'robot_to_dot':len(self._input)//2,'dot_to_robot':len(self._output)//2}
    def open_device_io(self):raise NotImplementedError('Synthetic boundary only; no live CoreAudio driver or route authorization')

class CodecHostBridge:
    """Wire existing codec/resampler objects to the synthetic callback boundary.
    Worker-side methods only; never invoke Python/ctypes from a real-time driver.
    Caller owns codec lifetimes. Injected converters enable no-native fixture tests.
    """
    def __init__(self,adapter,decoder,encoder,up,down):
        self.adapter=adapter;self.decoder=decoder;self.encoder=encoder
        self.up=up;self.down=down;self.pending=bytearray();self.epoch=0
    def begin(self,epoch):
        self.adapter.begin(epoch);self.epoch=epoch;self.pending.clear()
        for component in (self.decoder,self.encoder,self.up,self.down):component.reset()
    def receive_opus(self,epoch,packet):
        if epoch!=self.epoch or self.adapter.muted or self.adapter.playing:return False
        if type(packet) is not bytes or not 1<=len(packet)<=4096:raise ValueError('Opus packet bound')
        pcm=self.decoder.decode(packet)
        if len(pcm)!=1920:raise ValueError('16kmono60ms uplink required')
        bus=self.up.process(pcm)
        return self.adapter.robot_pcm(epoch,bus) if bus else True
    def outgoing_opus(self,epoch,frames=512):
        if epoch!=self.epoch or self.adapter.muted:return []
        pcm=self.adapter.robot_output_pcm(epoch,frames)
        self.pending.extend(self.down.process(pcm))
        if len(self.pending)>11520:raise ValueError('converted output bound')
        result=[]
        while len(self.pending)>=2880:
            result.append(self.encoder.encode(bytes(self.pending[:2880])));del self.pending[:2880]
        return result
    def interrupt(self,epoch):
        if epoch!=self.epoch:return False
        self.pending.clear()
        for component in (self.decoder,self.encoder,self.up,self.down):component.reset()
        return self.adapter.interrupt(epoch)
