"""Authenticated audio/control boundary. No listener, IO, inference or token issue.
HomeLab owns credential/device and Turn/request authority. CodecHostBridge owns
codec state. A supplied arm callback must already have physical-test approval.
"""
import json,struct,time
WIRE=struct.Struct('>4sBBIIH')

class AudioBackpressure(BufferError):
    """A bounded native uplink queue declined a complete microphone packet."""

class SustainedAudioBackpressure(BufferError):
    """Several consecutive microphone packets could not enter the native queue."""
def encode_frame(epoch,sequence,speaker,payload):
    if type(epoch) is not int or not 1<=epoch<=0xffffffff or type(sequence) is not int or not 0<=sequence<0xffffffff or type(speaker) is not bool or type(payload) is not bytes or not 1<=len(payload)<=4096: raise ValueError('bounded audio frame')
    return WIRE.pack(b'SCAV',1,2 if speaker else 1,epoch,sequence,len(payload))+payload
def decode_frame(wire):
    if type(wire) is not bytes or not 17<=len(wire)<=4112:raise ValueError('frame length')
    magic,version,kind,epoch,sequence,length=WIRE.unpack(wire[:16])
    if magic!=b'SCAV' or version!=1 or kind not in (1,2) or not epoch or sequence==0xffffffff or length!=len(wire)-16:raise ValueError('frame header')
    return epoch,sequence,kind==2,wire[16:]
def parse_control(text,*,max_bytes=1024):
    if type(max_bytes) is not int or not 1<=max_bytes<=2048:raise ValueError("bounded control parser limit")
    if type(text) is not str or not 1<=len(text.encode())<=max_bytes:raise ValueError('bounded control')
    def unique(pairs):
        d={}
        for key,value in pairs:
            if key in d:raise ValueError('duplicate field')
            d[key]=value
        return d
    value=json.loads(text,object_pairs_hook=unique)
    if type(value) is not dict:raise ValueError('control object')
    return value

class StreamingSession:
    def __init__(self,registry,sessions,bridge,credential,device_id,arm,*,clock=time.monotonic,fixture=False,native_session_ref=None):
        if type(credential) is not str or not 1<=len(credential)<=256 or registry.authenticate(credential)!=device_id:raise PermissionError('device authentication failed')
        device=registry.get(device_id)
        if not device or device.status!='active' or not {'microphone','speaker'}<=set(device.capabilities):raise PermissionError('audio capabilities not granted')
        if not fixture and (type(native_session_ref) is not str or not 1<=len(native_session_ref)<=128):raise PermissionError('explicit existing native Mira session reference required')
        self.registry=registry;self.sessions=sessions;self.bridge=bridge;self.credential=credential;self.device_id=device_id
        self.arm_callback=arm;self.clock=clock;self.fixture=fixture
        self.conversation=sessions.create(connection_id='mira.native_voice.fixture' if fixture else 'mira.native_voice.transport',device_id=device_id)
        if not fixture:self.conversation=sessions.attach_external_session(self.conversation.id,native_session_ref)
        self.turn=None;self.epoch=0;self.mic_seq=0;self.speaker_seq=0;self.armed=False;self.acknowledged=False;self.deadline=clock()+2
    def _authorized(self):
        if self.registry.authenticate(self.credential)!=self.device_id:raise PermissionError('device revoked')
        if self.clock()>=self.deadline:raise TimeoutError('heartbeat expired')
    def _active(self):
        return self.turn is not None and self.sessions.accepts_output(self.turn.conversation_id,self.turn.id,self.turn.generation)
    def begin(self,request_id):
        self._authorized()
        if type(request_id) is not str or not 1<=len(request_id)<=128:raise ValueError('request ID')
        old=self.sessions.get_request(request_id)
        if old:
            if not self.turn or old.id!=self.turn.id or not self._active():raise PermissionError('stale/foreign request replay')
            return self.hello() # deduplication does not clear queued audio
        if self.epoch==0xffffffff:raise OverflowError('reconnect before epoch wrap')
        if self.turn and self._active():self.interrupt()
        self.turn=self.sessions.begin_turn(self.conversation.id,request_id=request_id)
        self.epoch+=1;self.mic_seq=self.speaker_seq=0;self.armed=False;self.acknowledged=False
        self.bridge.begin(self.epoch);return self.hello()
    def hello(self):
        return {'type':'begin','epoch':self.epoch,'turn_id':self.turn.id,'generation':self.turn.generation,'conversation_id':self.conversation.id,'turn_generation':str(self.turn.generation),'audio':{'uplink_rate':16000,'downlink_rate':24000,'channels':1,'frame_ms':60},'fixture':self.fixture}
    def control(self,value):
        self._authorized()
        kind=value.get('type')
        if kind=='begin' and set(value)=={'type','request_id'}:return self.begin(value['request_id'])
        if not self._active() or type(value.get('epoch')) is not int or value['epoch']!=self.epoch:raise PermissionError('stale control')
        if kind=='ack' and set(value)=={'type','epoch'}:self.acknowledged=True;return {'type':'acknowledged','epoch':self.epoch}
        if kind=='consent' and set(value)=={'type','epoch','physical'} and value['physical'] is True:
            if not self.acknowledged:raise PermissionError('handshake acknowledgement required')
            self.arm_callback(self.epoch);self.armed=True;return {'type':'armed','epoch':self.epoch}
        if kind=='heartbeat' and set(value)=={'type','epoch'}:self.deadline=self.clock()+2;return {'type':'heartbeat','epoch':self.epoch}
        if kind=='interrupt' and set(value)=={'type','epoch'}:
            self.interrupt();return {'type':'interrupt','epoch':self.epoch,'speaker_stop_confirmed':False,'provider_cancel_confirmed':False}
        raise ValueError('unsupported control')
    def receive(self,wire):
        self._authorized()
        epoch,sequence,speaker,payload=decode_frame(wire)
        if not self.armed or not self._active() or epoch!=self.epoch or speaker:raise PermissionError('audio authority/direction')
        if sequence!=self.mic_seq:self.interrupt();raise ValueError('audio sequence gap/replay')
        if not self.bridge.receive_opus(epoch,payload):raise BufferError('capture suppressed/backpressure')
        self.mic_seq+=1
    def outgoing(self,frames=512):
        self._authorized()
        if not self.armed or not self._active():return []
        result=[]
        for packet in self.bridge.outgoing_opus(self.epoch,frames):
            if self.speaker_seq==0xffffffff:self.interrupt();raise OverflowError('audio sequence wrap')
            result.append(encode_frame(self.epoch,self.speaker_seq,True,packet));self.speaker_seq+=1
        return result
    def playing(self,value):
        self._authorized()
        if type(value) is not bool or not self.armed or not self._active():raise PermissionError('playback authority')
        if not self.bridge.adapter.set_playing(self.epoch,value):raise PermissionError('stale playback')
        return {'type':'playing','epoch':self.epoch,'value':value}
    def interrupt(self):
        if self.epoch and getattr(self.bridge,'closed',False) is not True:self.bridge.interrupt(self.epoch)
        self.armed=False;self.acknowledged=False
        if self.turn and self._active():
            self.sessions.interrupt_turn(self.turn.conversation_id,self.turn.id)
            self.sessions.mark_interrupt_uncertain(self.turn.conversation_id,self.turn.id)
    def close(self):
        try:self.interrupt()
        finally:self.armed=False;self.acknowledged=False;self.credential=''

def require_production_tls(uri):
    from urllib.parse import urlsplit
    value=urlsplit(uri)
    if value.scheme!='wss' or not value.hostname or value.username or value.password or value.query or value.fragment or value.path!='/companion/audio':raise ValueError('verified WSS endpoint required; credentials only in header')
    return value
