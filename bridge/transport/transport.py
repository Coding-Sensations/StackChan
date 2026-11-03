"""Offline wire/policy fixtures only; no I/O, codec, OS routing or firmware driver.

Opus envelopes mirror pinned factory headers; payloads remain opaque.
SCPC PCM is a proposed exclusive serial protocol, absent from stock firmware.
"""
from dataclasses import dataclass
import struct
import zlib


@dataclass(frozen=True)
class OpusEnvelope:
    version: int
    payload: bytes
    timestamp: int = 0

    def __post_init__(self):
        if type(self.version) is not int or self.version not in (1,2,3):
            raise ValueError('unsupported factory envelope version')
        if type(self.payload) is not bytes or not 1 <= len(self.payload) <= 4096:
            raise ValueError('opaque Opus payload exceeds host fixture policy')
        if type(self.timestamp) is not int or not 0 <= self.timestamp <= 0xffffffff:
            raise ValueError('invalid timestamp')
        if self.version != 2 and self.timestamp != 0:
            raise ValueError('v1/v3 do not transmit timestamp')

    def encode(self):
        if self.version == 2:
            return struct.pack('>HHIII',2,0,0,self.timestamp,len(self.payload))+self.payload
        if self.version == 3:
            return struct.pack('>BBH',0,0,len(self.payload))+self.payload
        return self.payload

    @classmethod
    def decode(cls, version, wire):
        if type(wire) is not bytes or not 1 <= len(wire) <= 4112:
            raise ValueError('invalid envelope length')
        if version == 1:
            return cls(version,wire)
        if version == 2 and len(wire) >= 16:
            v,kind,reserved,timestamp,size=struct.unpack('>HHIII',wire[:16])
            if v==2 and kind==0 and reserved==0 and size==len(wire)-16:
                return cls(version,wire[16:],timestamp)
        if version == 3 and len(wire) >= 4:
            kind,reserved,size=struct.unpack('>BBH',wire[:4])
            if kind==0 and reserved==0 and size==len(wire)-4:
                return cls(version,wire[4:])
        raise ValueError('malformed or non-audio envelope')


_USB_HEADER=struct.Struct('>4sBBIIHH')

@dataclass(frozen=True)
class UsbPcmFrame:
    epoch: int
    sequence: int
    sample_rate: int
    direction: str
    pcm: bytes

    def __post_init__(self):
        if any(type(v) is not int or not 0 <= v <= 0xffffffff for v in (self.epoch,self.sequence)):
            raise ValueError('epoch/sequence must be uint32')
        if type(self.sample_rate) is not int or self.sample_rate not in (16000,24000):
            raise ValueError('fixture rate must be 16kHz or24kHz')
        if self.direction not in ('mic','speaker'):
            raise ValueError('invalid PCM direction')
        if type(self.pcm) is not bytes or not self.pcm or len(self.pcm)%2 or len(self.pcm)>self.sample_rate*2*40//1000:
            raise ValueError('mono PCM16 fixture packet must be at most40ms')

    def encode(self):
        header=_USB_HEADER.pack(b'SCPC',1,1 if self.direction=='mic' else 2,
                                self.epoch,self.sequence,self.sample_rate,len(self.pcm))
        body=header+self.pcm
        return body+struct.pack('>I',zlib.crc32(body))


class UsbPcmParser:
    def __init__(self):
        self._buffer=bytearray()

    @property
    def buffered_bytes(self):
        return len(self._buffer)

    def feed(self, data):
        try:
            if type(data) is not bytes or len(data)>8192 or len(self._buffer)+len(data)>8192:
                raise ValueError('bounded stream fragment exceeded')
            self._buffer.extend(data)
            frames=[]
            while len(self._buffer)>=_USB_HEADER.size:
                magic,version,kind,epoch,sequence,rate,size=_USB_HEADER.unpack(self._buffer[:_USB_HEADER.size])
                if magic!=b'SCPC' or version!=1 or kind not in (1,2) or rate not in (16000,24000) or not 1<=size<=rate*2*40//1000 or size%2:
                    raise ValueError('invalid exclusive PCM stream header; no debug-log resync')
                total=_USB_HEADER.size+size+4
                if len(self._buffer)<total:break
                body=bytes(self._buffer[:total-4])
                checksum=struct.unpack('>I',self._buffer[total-4:total])[0]
                if zlib.crc32(body)!=checksum:raise ValueError('PCM checksum mismatch')
                frames.append(UsbPcmFrame(epoch,sequence,rate,'mic' if kind==1 else 'speaker',body[_USB_HEADER.size:]))
                del self._buffer[:total]
            return frames
        except ValueError:
            self._buffer.clear()
            raise


class DuplexGate:
    """Fixture qualification gates, not hardware attestations or echo cancellation."""
    def __init__(self, adapter):
        self.adapter=adapter
        self.muted=True  # Unknown physical switch state fails closed.
        self.playing=False
        self.full_duplex=False

    def set_physical_mute(self, muted, *, verified, now):
        if type(muted) is not bool or verified is not True:
            raise PermissionError('explicit fixture physical mute state required')
        if muted:self.adapter.interrupt(now)
        self.muted=muted
        if muted:self.playing=False

    def enable_full_duplex(self, *, aec_verified, reference_verified):
        if aec_verified is not True or reference_verified is not True:
            raise PermissionError('AEC and exact playback reference qualification required')
        self.full_duplex=True

    def set_playback(self, active):
        if type(active) is not bool:raise ValueError('playback state must be boolean')
        self.playing=active

    def capture(self, frame, now):
        if self.muted or (self.playing and not self.full_duplex):return False
        return self.adapter.push_input(frame,now)

    def render(self, frame, now):
        if self.muted:return False
        accepted=self.adapter.push(frame,now)
        if accepted:self.playing=True
        return accepted

    def interrupt(self, now):
        result=self.adapter.interrupt(now)
        self.playing=False
        return result


def latency_budget(transport):
    """Engineering targets only; model/cloud latency is unknown and excluded."""
    if transport=='network-opus':
        up={'capture_frame_ms':60,'transport_ms':10,'decode_ms':5,'host_buffer_ms':10}
        down={'host_buffer_ms':10,'encode_frame_ms':60,'transport_ms':10,'jitter_ms':60,'decode_ms':5,'render_ms':10}
    elif transport=='usb-pcm':
        up={'capture_frame_ms':20,'transport_ms':5,'decode_ms':0,'host_buffer_ms':10}
        down={'host_buffer_ms':10,'encode_frame_ms':20,'transport_ms':5,'jitter_ms':20,'decode_ms':0,'render_ms':10}
    else:raise ValueError('unknown transport budget')
    return {**up,'uplink_target_ms':sum(up.values()),'downlink':down,
            'roundtrip_adapter_target_ms':sum(up.values())+sum(down.values()),
            'interruption_to_silence_target_ms':100,'model_response_ms':'unknown','measured':False}


class PcmSessionBinding:
    """Bind proposed serial epoch to one actual HomeLab fixture turn.

    Bindings are connection-local. A future firmware handshake must acknowledge
    a new epoch before audio is accepted; no handshake or serial I/O exists here.
    """
    def __init__(self, gate):
        self.gate=gate
        self.epoch=0
        self.turn_id=None

    def bind_current_turn(self):
        turn=self.gate.adapter.turn
        if turn is None or not self.gate.adapter._active():
            raise PermissionError('active HomeLab fixture turn required')
        if turn.id!=self.turn_id:
            if self.epoch==0xffffffff:raise OverflowError('start a new connection before epoch wrap')
            self.epoch+=1
            self.turn_id=turn.id
        return self.epoch

    def accept(self, packet, now):
        from adapter import AudioFrame
        bridge=self.gate.adapter
        if (packet.epoch!=self.epoch or self.turn_id is None or bridge.turn is None
                or bridge.turn.id!=self.turn_id or not bridge._active()):return False
        if packet.sample_rate!=16000:
            raise NotImplementedError('qualified resampling required before16kHz HomeLab PCM fixtures')
        frame=AudioFrame(device_id=bridge.device.id,conversation_id=bridge.turn.conversation_id,
            turn_id=bridge.turn.id,generation=bridge.turn.generation,sequence=packet.sequence,pcm=packet.pcm)
        return self.gate.capture(frame,now) if packet.direction=='mic' else self.gate.render(frame,now)


def host_route_plan(mic_bus, playback_bus):
    """Declarative public-OS audio route. Existing endpoints are unqualified.

    No device UID selection, CoreAudio callback, default change or activation.
    Two separate loopback buses prevent directly routing speaker audio to mic.
    """
    if any(type(v) is not str or not v.strip() or len(v)>128 for v in (mic_bus,playback_bus)) or mic_bus==playback_bus:
        raise ValueError('two distinct bounded loopback endpoint names required')
    return {'session':'existing dot in native Voice; user-operated','executable':False,
        'mic_path':{'bus':mic_bus,'host_adapter':'output','native_voice':'input'},
        'playback_path':{'bus':playback_bus,'native_voice':'output','host_adapter':'input'},
        'device_sample_rate_target':48000,'channels':[1,2],
        'compatibility':'unverified; qualify endpoint loopback and native-client device selection',
        'changes_required':'explicit routing/capture/playback approval; no changes made'}
