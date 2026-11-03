"""Portable device-side adapter contract; no hardware implementation or IO setup.

A trusted existing WSS adapter dispatches authenticated messages. Actual firmware
must supply a checked output driver; unsupported drain never yields a receipt.
"""
from dataclasses import dataclass
from .session_contract import Binding

@dataclass(frozen=True)
class DrainStatus:
    decoder_queued: int
    decoder_inflight: int
    output_frames: int
    hardware_drained: bool
    def __post_init__(self):
        for n,cap in ((self.decoder_queued,2),(self.decoder_inflight,1),(self.output_frames,7680)):
            if type(n) is not int or not 0<=n<=cap:raise ValueError('bounded actual driver status required')
        if type(self.hardware_drained) is not bool:raise ValueError('explicit driver drain required')

class DevicePlaybackEndpoint:
    def __init__(self,driver,hello):
        if type(hello) is not dict or type(hello.get('desk_contract')) is not int or hello['desk_contract']!=1:
            raise ValueError('v1 begin required')
        self._binding=Binding(hello.get('conversation_id'),hello.get('turn_id'),hello.get('generation'),hello.get('epoch'))
        self._driver=driver;self._playing=False;self._closed=False;self._next=0;self._final=None;self._reported=False
    def _message(self,kind,**fields):
        b=self._binding
        return dict(type=kind,version=1,epoch=b.epoch,conversation_id=b.conversation_id,
                    turn_id=b.turn_id,generation=b.generation,**fields)
    def contract_ack(self):return self._message('state_contract_ack')
    def control(self,value):
        try:
            return self._control(value)
        except Exception:
            if not self._closed:self.disconnect()
            raise
    def _control(self,value):
        if self._closed:raise PermissionError('device turn closed')
        if type(value) is not dict:raise ValueError('control object required')
        kind=value.get('type');extra={'playing':('value',),'playback_end':('final_sequence',),'hard_mute':()}.get(kind)
        if extra is None:raise ValueError('unsupported device control')
        expected=self._message(kind)
        if set(value)!=set(expected)|set(extra) or not all(type(value[k]) is type(v) and value[k]==v for k,v in expected.items()):
            raise PermissionError('foreign device control')
        if kind=='hard_mute':self.disconnect();return
        if kind=='playing':
            if type(value['value']) is not bool or self._final is not None:raise PermissionError('playback ended')
            self._playing=value['value'];return
        final=value['final_sequence']
        if type(final) is not int or final!=self._next-1 or not self._playing:raise PermissionError('correlated end required')
        self._final=final
    def receive(self,wire):
        from streaming_session import decode_frame
        if self._closed or not self._playing or self._final is not None:raise PermissionError('device playback inactive')
        try:
            epoch,sequence,speaker,payload=decode_frame(wire)
            if epoch!=self._binding.epoch or not speaker:raise PermissionError('foreign playback packet')
            if sequence!=self._next:raise ValueError('playback sequence gap/replay')
            status=self._driver.drain_status(epoch)
            if type(status) is not DrainStatus:raise ValueError('bounded actual decoder queue required')
            if status.decoder_queued>=2:raise BufferError('device decoder backpressure')
            if self._driver.enqueue(epoch,sequence,payload) is not True:raise BufferError('device decoder backpressure')
            self._next+=1
        except Exception:
            self.disconnect();raise
    def poll_drained(self):
        if self._closed or self._final is None or self._reported:return None
        try:
            status=self._driver.drain_status(self._binding.epoch)
            if status is None:return None
            if type(status) is not DrainStatus:raise ValueError('checked drain status required')
            if status.decoder_queued or status.decoder_inflight or status.output_frames or not status.hardware_drained:return None
            self._reported=True
            return self._message('playback_drained',final_sequence=self._final)
        except Exception:
            self.disconnect();raise
    def disconnect(self):
        self._closed=True;self._playing=False
        self._driver.hard_mute() # caller retains this exact driver on failure
