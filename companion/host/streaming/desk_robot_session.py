"""Opt-in v1 session factory for the existing authenticated audio gateway.

Not selected by the live native-call factory. Peer must explicitly acknowledge
this dialect before physical Listen. No credential/provider/listener is created.
"""
from desk_robot.session_contract import Binding,State,TurnContract
from streaming_session import StreamingSession,decode_frame
from gateway_connection import TransportComplete

class DeskRobotSession(StreamingSession):
    def __init__(self,*args,**kwargs):
        self.contract=None;self._dialect_ack=False;self._drain_complete=False
        self.on_complete=lambda:None
        super().__init__(*args,**kwargs)

    def _authorized(self):
        if self._drain_complete:raise TransportComplete('correlated playback drain complete')
        try:
            super()._authorized()
            if self.contract:self.contract.validate(self.contract.binding)
        except (PermissionError,TimeoutError,ValueError):
            self.interrupt();raise

    def begin(self,request_id):
        answer=super().begin(request_id)
        binding=Binding(self.conversation.id,self.turn.id,self.turn.generation,self.epoch)
        if self.contract is None or self.contract.binding!=binding:
            self.contract=TurnContract(binding,lambda b:self._active() and self.contract.binding==b,clock=self.clock)
            self._dialect_ack=False
        return self.hello()

    def hello(self):
        answer=super().hello()
        answer.update(desk_contract=1,conversation_id=self.conversation.id)
        return answer

    def _message(self,kind,**fields):
        b=self.contract.binding
        return dict(type=kind,version=1,epoch=b.epoch,conversation_id=b.conversation_id,
                    turn_id=b.turn_id,generation=b.generation,**fields)

    def _correlated(self,value,kind,extra=()):
        expected=self._message(kind)
        if type(value) is not dict or set(value)!=set(expected)|set(extra):return False
        return all(type(value[k]) is type(v) and value[k]==v for k,v in expected.items())

    def control(self,value):
        self._authorized()
        if type(value) is not dict:raise ValueError('bounded control object required')
        kind=value.get('type')
        if kind=='state_contract_ack':
            if self.contract is None or not self.acknowledged or not self._correlated(value,kind):
                raise PermissionError('current v1 handshake required')
            self._dialect_ack=True;return self._message('state_contract_ready')
        if kind=='consent':
            if (not self._dialect_ack or self.contract is None
                    or self.contract.state!=State.HARD_MUTED or self.armed):
                raise PermissionError('fresh v1 physical Listen required')
            answer=super().control(value)
            self.contract.physical_listen(self.contract.binding,physical=True)
            return answer
        if kind=='playback_drained':
            if not self.contract or not self.armed or not self._correlated(value,kind,('final_sequence',)):
                raise PermissionError('current authenticated drain receipt required')
            if not self.contract.drained(self.contract.binding,value['final_sequence']):
                raise PermissionError('duplicate drain receipt')
            # Stop this exact bridge before committing completion. Failure keeps
            # the turn incomplete and existing gateway cleanup retains its owner.
            self.armed=False;self.acknowledged=False
            self.bridge.interrupt(self.epoch)
            self.sessions.finish_turn(self.conversation.id,self.turn.id,'completed')
            self._drain_complete=True;self.on_complete()
            return self._message('hard_mute')
        return super().control(value)

    def receive(self,wire):
        self._authorized()
        if not self.contract or not self.contract.capture_allowed:
            raise PermissionError('half-duplex microphone closed')
        return super().receive(wire)

    def outgoing(self,frames=512):
        self._authorized()
        if not self.armed:return []
        if not self.contract or self.contract.state!=State.SPEAKING:return []
        try:
            packets=super().outgoing(frames)
            if type(packets) is not list or len(packets)>2:raise BufferError('bounded audio tick required')
            for wire in packets:
                epoch,sequence,speaker,payload=decode_frame(wire)
                if epoch!=self.epoch or speaker is not True:raise PermissionError('playback direction')
                self.contract.enqueue(self.contract.binding,sequence,payload)
            for _ in packets:self.contract.take(self.contract.binding)
            return packets
        except Exception:
            self.interrupt();raise

    def playing(self,value):
        self._authorized()
        if type(value) is not bool or not self.armed:raise PermissionError('playback authority')
        b=self.contract.binding
        if value:
            if self.contract.state==State.LISTENING:
                self.contract.thinking(b);self.contract.start_playback(b)
            if self.contract.state!=State.SPEAKING:raise PermissionError('playback already ended')
            super().playing(True)
            return self._message('playing',value=True)
        if self.contract.state in (State.SPEAKING,State.DRAINING):
            if self.contract.state==State.SPEAKING:self.contract.end_playback(b,self.speaker_seq-1)
            # Do not send legacy playing=false: that would reopen mic capture
            # before the device reports decoded queue AND actual output drain.
            return self._message('playback_end',final_sequence=self.speaker_seq-1)
        answer=super().playing(False)
        return self._message('playing',value=answer['value'])

    def interrupt(self):
        if self.contract:self.contract.cancel()
        self.armed=False;self.acknowledged=False
        super().interrupt()
