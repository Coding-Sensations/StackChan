import unittest
from home_lab.devices import DeviceRegistry
from home_lab.sessions import ConversationSessions
from streaming_session import StreamingSession,encode_frame,decode_frame,parse_control,require_production_tls
class Adapter:
    def __init__(self):self.playing=False
    def set_playing(self,epoch,value):self.playing=value;return True
class Bridge:
    def __init__(self):self.adapter=Adapter();self.begins=[];self.input=[];self.interrupts=[]
    def begin(self,epoch):self.begins.append(epoch)
    def receive_opus(self,epoch,payload):self.input.append((epoch,payload));return not self.adapter.playing
    def outgoing_opus(self,epoch,frames):return [b'synthetic-not-opus']
    def interrupt(self,epoch):self.input.clear();self.interrupts.append(epoch)
class Tests(unittest.TestCase):
    def setUp(self):
        self.registry=DeviceRegistry();code=self.registry.create_pairing_code()
        self.device,self.token=self.registry.redeem_pairing_code(code,device_id='synthetic-stackchan',label='Synthetic',capabilities=('microphone','speaker'))
        self.clock=[0.];self.bridge=Bridge();self.arms=[]
        self.session=StreamingSession(self.registry,ConversationSessions(),self.bridge,self.token,self.device.id,self.arms.append,clock=lambda:self.clock[0],fixture=True)
    def armed(self):
        self.session.begin('synthetic-request');e=self.session.epoch
        self.session.control({'type':'ack','epoch':e});self.session.control({'type':'consent','epoch':e,'physical':True})
        return e
    def test_wire_bounds_and_direction(self):
        wire=encode_frame(7,3,True,b'x'*4096);self.assertEqual(decode_frame(wire),(7,3,True,b'x'*4096))
        for bad in (wire[:-1],b'logs'+wire,wire+b'x',encode_frame(1,0,False,b'x')[:6]+b'\0'*4+wire[10:]):
            with self.assertRaises(ValueError):decode_frame(bad)
    def test_requires_ack_and_consent(self):
        self.session.begin('r')
        with self.assertRaises(PermissionError):self.session.control({'type':'consent','epoch':1,'physical':True})
        with self.assertRaises(PermissionError):self.session.receive(encode_frame(1,0,False,b'x'))
    def test_duplicate_preserves_epoch_and_pending(self):
        e=self.armed();self.session.receive(encode_frame(e,0,False,b'x'))
        before=list(self.bridge.input);self.session.begin('synthetic-request')
        self.assertEqual(self.bridge.input,before);self.assertEqual(self.bridge.begins,[1])
    def test_replay_mutes_and_requires_new_turn(self):
        e=self.armed();self.session.receive(encode_frame(e,0,False,b'x'))
        with self.assertRaises(ValueError):self.session.receive(encode_frame(e,0,False,b'x'))
        self.assertFalse(self.session.armed)
        with self.assertRaises(PermissionError):self.session.begin('synthetic-request')
        self.session.begin('new');self.assertEqual(self.session.epoch,2)
    def test_interruption_rejects_late_packet_and_reports_unknown_stop(self):
        e=self.armed();status=self.session.control({'type':'interrupt','epoch':e})
        self.assertFalse(status['speaker_stop_confirmed']);self.assertFalse(status['provider_cancel_confirmed'])
        with self.assertRaises(PermissionError):self.session.receive(encode_frame(e,0,False,b'x'))
        self.assertEqual(self.bridge.interrupts,[e])
    def test_revocation_and_watchdog(self):
        e=self.armed();self.clock[0]=2
        with self.assertRaises(TimeoutError):self.session.outgoing()
        self.clock[0]=0;self.registry.revoke(self.device.id)
        with self.assertRaises(PermissionError):self.session.receive(encode_frame(e,0,False,b'x'))
    def test_provider_memory_boundary(self):
        with self.assertRaises(PermissionError):StreamingSession(self.registry,ConversationSessions(),self.bridge,self.token,self.device.id,self.arms.append)
    def test_tls_and_duplicate_fields(self):
        require_production_tls('wss://companion.example/companion/audio')
        for uri in ('ws://host/companion/audio','wss://user:secret@host/companion/audio','wss://host/companion/audio?token=x'):
            with self.assertRaises(ValueError):require_production_tls(uri)
        with self.assertRaises(ValueError):parse_control('{"type":"ack","epoch":1,"epoch":2}')
    def test_playback_and_independent_sequences(self):
        e=self.armed();self.session.playing(True)
        with self.assertRaises(BufferError):self.session.receive(encode_frame(e,0,False,b'x'))
        wire=self.session.outgoing()[0];self.assertEqual(decode_frame(wire)[:3],(e,0,True))
        self.session.playing(False);self.session.receive(encode_frame(e,0,False,b'x'));self.assertEqual(self.session.mic_seq,1)
if __name__=='__main__':unittest.main()
