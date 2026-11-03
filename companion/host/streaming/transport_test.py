"""Independent robot transport test: not a Mira/provider/conversation attachment.

Import and --help are inert. No credential issuer, credential file, audio device,
CoreAudio/default mutation, inference, raw microphone retention or secret logger.
Caller supplies an approved generated mono24k PCM16 cue (<=10 s), existing
HomeLab authority, verified LAN TLS and fresh human approval. Production listener
is lazy and pinned, <=120 s; caller owns the outer 125 s alarm/130 s supervisor.
"""
import public_config as config
import argparse
import asyncio
from contextlib import asynccontextmanager
import ssl
import struct
import time
from gateway_admission import authorize_audio_request
from gateway_connection import run_connection, TransportComplete
from transport_diagnostics import TransportDiagnostics
from streaming_session import StreamingSession, decode_frame

HOST = config.HOST
PORT = config.PORT
DEVICE = config.DEVICE
MAX_LISTENER_SECONDS = 120
MAX_CUE_FRAMES = 24000 * 10

def _cue_valid(cue):
    if type(cue) is not bytes or not cue or len(cue) % 2 or len(cue) > MAX_CUE_FRAMES * 2:
        raise ValueError('bounded generated mono24k PCM16 cue required')

class _CountCueBridge:
    """Connection-local installed Opus codec; each decoded microphone frame is discarded."""
    def __init__(self, cue, clock, mic_duration):
        _cue_valid(cue)
        from codec import OpusCodec  # Native codec only; no audio device access.
        self.decoder = OpusCodec(16000)
        try:
            self.encoder = OpusCodec(24000)
        except Exception:
            self.decoder.close()
            raise
        self.adapter = self
        self.clock = clock
        self.mic_duration = mic_duration
        self.cue = cue
        self.epoch = 0
        self.muted = True
        self._playing = False
        self._armed = False
        self.closed = False
        self.position = 0
        self.next_packet = 0.
        self.last_audio_end = None
        self.mic_until = None
        self.ever_armed = self.cue_complete = self._mic_complete = False
        self.speaker_packets = self.speaker_frames = 0
        self.decoded_mic_packets = self.decoded_mic_frames = self.suppressed_mic_packets = 0
        self.decoded_mic_peak = self.decoded_mic_energy = 0

    @property
    def playing(self):
        if (self._playing and self.last_audio_end is not None and self.position == len(self.cue)
            and self.clock() >= self.last_audio_end + .25):
            self._playing = False
            self.cue_complete = True
            self.mic_until = self.clock() + self.mic_duration
        return self._playing

    @property
    def completed(self):
        self.playing  # Advance the bounded logical playback phase.
        if self._armed and self.cue_complete and self.mic_until is not None and self.clock() >= self.mic_until:
            self._mic_complete = True
        return self._mic_complete

    def begin(self, epoch):
        if self.epoch or self.closed:
            raise PermissionError('one cue per transport connection')
        self.epoch = epoch
        self.decoder.reset(); self.encoder.reset()

    def arm(self, epoch):
        if epoch != self.epoch or self.closed:
            raise PermissionError('stale transport consent')
        if self.ever_armed:
            return  # Duplicate consent cannot replay the cue.
        self.muted = False
        self._armed = self.ever_armed = self._playing = True
        self.next_packet = self.clock()

    def set_playing(self, epoch, value):
        return epoch == self.epoch and type(value) is bool and value == self.playing and not self.muted

    def receive_opus(self, epoch, packet):
        if epoch != self.epoch or self.muted or self.playing:
            return False
        pcm = self.decoder.decode(packet)
        if len(pcm) != 1920:
            raise ValueError('mono16k 60ms microphone frame required')
        self.decoded_mic_packets += 1
        self.decoded_mic_frames += len(pcm) // 2
        for (sample,) in struct.iter_unpack('<h', pcm):
            self.decoded_mic_peak = max(self.decoded_mic_peak, abs(sample))
            self.decoded_mic_energy += sample * sample
        # No recording/queue/export: the local decoded bytes leave scope here.
        return True

    def outgoing_opus(self, epoch, frames=960):
        if type(frames) is not int or not 1 <= frames <= 2048:
            raise ValueError('bounded codec tick required')
        if epoch != self.epoch or self.muted or not self.playing or self.position >= len(self.cue):
            return []
        now = self.clock()
        if now < self.next_packet:
            return []
        chunk = self.cue[self.position:self.position + 2880]
        self.position += len(chunk)
        # Final packet has at most one Opus frame of zero padding, no generated silence stream.
        packet = self.encoder.encode(chunk.ljust(2880, b'\0'))
        self.speaker_packets += 1; self.speaker_frames += 1440
        self.next_packet = now + .06
        self.last_audio_end = self.next_packet
        return [packet]

    def interrupt(self, epoch):
        if epoch != self.epoch:
            return False
        self.muted = True; self._playing = self._armed = False
        return True

    def close(self):
        if not self.closed:
            self.interrupt(self.epoch)
            self.decoder.close(); self.encoder.close()
            self.closed = True

class TransportSession(StreamingSession):
    """Reuse HomeLab turns/wire authority with a separate explicit transport identity."""
    def __init__(self, registry, sessions, credential, device_id, cue_pcm24, *,
                 transport_approved=False, mic_duration=10., clock=time.monotonic):
        if transport_approved is not True:
            raise PermissionError('explicit human transport-test approval required')
        if type(mic_duration) not in (int, float) or not 0 < mic_duration <= 30:
            raise ValueError('bounded microphone phase required')
        _cue_valid(cue_pcm24)
        # Existing admission enforces active status, capabilities, bounded token and identity.
        authorize_audio_request(registry, '/companion/audio',
            [('Authorization', 'Bearer ' + credential), ('X-Companion-Device', device_id)])
        if device_id != DEVICE:
            raise PermissionError('verified transport-test device required')
        self.registry = registry; self.sessions = sessions; self.credential = credential; self.device_id = device_id
        self.clock = clock; self.fixture = False
        self.bridge = _CountCueBridge(cue_pcm24, clock, mic_duration)
        self.arm_callback = self.bridge.arm
        try:
            self.conversation = sessions.create(connection_id='stackchan.transport_test', device_id=device_id)
        except Exception:
            self.bridge.close()
            raise
        self.turn = None; self.epoch = self.mic_seq = self.speaker_seq = 0
        self.armed = self.acknowledged = False
        self.deadline = clock() + 2
        self.transport_deadline = clock() + MAX_LISTENER_SECONDS
        self.on_complete=lambda:None

    def _authorized(self):
        super()._authorized()
        if self.clock() >= self.transport_deadline:
            raise TimeoutError('transport-test connection deadline')
        if self.bridge.completed:
            self.on_complete()
            raise TransportComplete('bounded transport microphone phase completed')

    def begin(self, request_id):
        if self.epoch and (self.turn is None or request_id != self.turn.request_id):
            raise PermissionError('one transport test per connection')
        return super().begin(request_id)

    def hello(self):
        result = super().hello()
        result.update(transport_only=True, identity_verified=False, purpose='robot_transport_test',
                      connection_id='stackchan.transport_test')
        return result

    def receive(self, wire):
        # Firmware suppresses capture on playing:true. Count/discard any in-flight
        # mic packets from before that control while preserving sequence authority.
        self._authorized()
        epoch, sequence, speaker, _payload = decode_frame(wire)
        if not self.armed or not self._active() or epoch != self.epoch or speaker:
            raise PermissionError('audio authority/direction')
        if sequence != self.mic_seq:
            self.interrupt()
            raise ValueError('audio sequence gap/replay')
        if self.bridge.adapter.playing:
            self.bridge.suppressed_mic_packets += 1
            self.mic_seq += 1
            return
        super().receive(wire)

    def statistics(self):
        bridge = self.bridge
        return dict(transport_only=True, identity_verified=False, robot_playback_verified=False,
                    consent_received=bridge.ever_armed, cue_frames=len(bridge.cue) // 2,
                    speaker_packets=bridge.speaker_packets, speaker_frames=bridge.speaker_frames,
                    decoded_mic_packets=bridge.decoded_mic_packets, decoded_mic_frames=bridge.decoded_mic_frames,
                    suppressed_mic_packets=bridge.suppressed_mic_packets,
                    decoded_mic_peak=bridge.decoded_mic_peak, decoded_mic_energy=bridge.decoded_mic_energy,
                    cue_complete=bridge.cue_complete, mic_complete=bridge.completed)

    def close(self):
        if self.bridge.closed:
            return
        try:
            if self.bridge.completed and self.turn and self._active():
                self.sessions.finish_turn(self.turn.conversation_id, self.turn.id, 'completed')
                self.bridge.interrupt(self.epoch); self.armed = self.acknowledged = False; self.credential = ''
            else:
                super().close()
        finally:
            self.bridge.close()

class TransportGateway:
    """Separate lazy LAN-only transport harness; never weakens RemoteGateway's Mira gate."""
    def __init__(self, registry, sessions, cue_pcm24, *, transport_approved=False,
                 mic_duration=10., clock=time.monotonic):
        _cue_valid(cue_pcm24)
        if type(mic_duration) not in (int, float) or not 0 < mic_duration <= 30:
            raise ValueError('bounded microphone phase required')
        self.registry = registry; self.sessions = sessions; self.cue = cue_pcm24
        self.transport_approved = transport_approved; self.mic_duration = mic_duration; self.clock = clock
        self.last_result = dict(transport_only=True, identity_verified=False, robot_playback_verified=False,
            consent_received=False, cue_frames=len(cue_pcm24)//2, speaker_packets=0, speaker_frames=0,
            decoded_mic_packets=0, decoded_mic_frames=0, suppressed_mic_packets=0,
            decoded_mic_peak=0, decoded_mic_energy=0, cue_complete=False, mic_complete=False,
            outcome='no_connection')
        self.last_result['diagnostics'] = TransportDiagnostics(clock).snapshot()
        self._used = False
        self.completion_started=False

    def process_request(self, connection, request):
        try:
            grant = authorize_audio_request(self.registry, request.path, list(request.headers.raw_items()))
            if grant.device_id != DEVICE:
                raise PermissionError('wrong transport-test device')
        except Exception:
            return connection.respond(403, 'Transport admission denied.\n')
        if self.transport_approved is not True or self._used:
            return connection.respond(503, 'Transport test unavailable.\n')
        return None

    async def handle(self, socket):
        session = None; started = False; outcome = 'fault'
        diagnostics = TransportDiagnostics(self.clock)
        try:
            diagnostics.set_phase('admission')
            grant = authorize_audio_request(self.registry, socket.request.path, list(socket.request.headers.raw_items()))
            if self.transport_approved is not True or self._used:
                raise PermissionError('human transport approval required')
            diagnostics.set_phase('session_create')
            session = TransportSession(self.registry, self.sessions, grant.credential, grant.device_id, self.cue,
                transport_approved=True, mic_duration=self.mic_duration, clock=self.clock)
            self._used = True
            session.on_complete=lambda:setattr(self,'completion_started',True)
            started = True
            await run_connection(socket, session, duration=MAX_LISTENER_SECONDS, diagnostics=diagnostics)
        except TransportComplete:
            diagnostics.expected_complete()
            outcome = 'complete'
        except asyncio.CancelledError as failure:
            diagnostics.fault(failure)
            outcome = 'interrupted'
            raise
        except Exception as failure:
            diagnostics.fault(failure)
        finally:
            # The driver already closed started sessions, preserving its primary
            # fault. Pre-session cleanup is recorded separately and never raised.
            if not started:
                try:
                    await asyncio.wait_for(socket.close(code=1008, reason='Transport test closed'),.25)
                except BaseException as failure:
                    diagnostics.fault(failure,'socket_cleanup',cleanup=True)
            result = dict(self.last_result)
            if session is not None:
                try:result.update(session.statistics())
                except BaseException as failure:
                    diagnostics.fault(failure,'statistics',cleanup=True)
                    outcome = 'fault'
            if diagnostics.snapshot().get('cleanup_fault_count',0):
                outcome = 'complete_with_cleanup_fault' if outcome == 'complete' else 'fault'
            result.update(outcome=outcome,diagnostics=diagnostics.snapshot())
            self.last_result = result

    def listener(self, *, ssl_context, host=HOST, port=PORT, duration=MAX_LISTENER_SECONDS):
        config.require()
        if self.transport_approved is not True:
            raise PermissionError('fresh human transport-test approval required')
        if (host != HOST or type(port) is not int or port != PORT or
            not isinstance(ssl_context, ssl.SSLContext) or ssl_context.protocol != ssl.PROTOCOL_TLS_SERVER or
            type(duration) not in (int, float) or not 0 < duration <= MAX_LISTENER_SECONDS):
            raise ValueError('pinned LAN listener, verified server TLS and <=120s duration required')
        @asynccontextmanager
        async def limited():
            from websockets.asyncio.server import serve
            # Entry, not import/factory/--help, is the explicit listener boundary.
            async with asyncio.timeout(duration):
                async with serve(self.handle, host, port, ssl=ssl_context, process_request=self.process_request,
                    max_size=4112, max_queue=2, compression=None, ping_interval=None,
                    open_timeout=2, close_timeout=.25, server_header=None) as server:
                    yield server
        return limited()

def main(argv=None):
    parser = argparse.ArgumentParser(description='Prepare-only robot transport test API; no listener or credentials are started.')
    parser.parse_args(argv)
    print('Transport-test API prepared only. A separate human-approved runner supplies TLS, cue and authority.')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
