"""Existing native-call audio only. Import/profile validation performs no IO.
The operator confirms the original call and its two-bus route; no model, TTS,
call-start API, device-default change or inferred conversation identity exists here.
"""
import public_config as config
from dataclasses import dataclass
from pathlib import Path
from array import array
import hashlib
import time
import threading
from concurrent.futures import ThreadPoolExecutor
from host_adapter import CodecHostBridge
from remote_gateway import RemoteGateway, QualifiedProvider, ProviderCleanupError
from streaming_session import AudioBackpressure, SustainedAudioBackpressure, StreamingSession, decode_frame, encode_frame
from gateway_connection import TransportComplete
from transport_diagnostics import project_native_audio

ROOT = config.ROOT
LIBRARY = config.LIBRARY
LIBRARY_HASH = config.LIBRARY_HASH
NATIVE_AUDIO_BACKEND = config.NATIVE_AUDIO_BACKEND
NATIVE_AUDIO_TEST_SECONDS = config.NATIVE_AUDIO_TEST_SECONDS

def _test_duration():
    value = NATIVE_AUDIO_TEST_SECONDS
    if value is not None and (NATIVE_AUDIO_BACKEND != 'directional_hal_v2' or type(value) is not int or not 1 <= value <= 15):
        raise PermissionError('invalid trusted native test duration')
    return value

def _backend_route():
    if NATIVE_AUDIO_BACKEND == 'legacy_bridge_v1':
        return ('Pro Tools Audio Bridge 2-A_UID', 'Pro Tools Audio Bridge 2-B_UID')
    if NATIVE_AUDIO_BACKEND == 'directional_hal_v2':
        return ('local.mrkillbob.StackChan.hal.microphone.v2', 'local.mrkillbob.StackChan.hal.speaker.v2')
    raise PermissionError('unsupported trusted native backend')

@dataclass(frozen=True)
class NativeRoute:
    session_reference: str
    duration_seconds: int = 60

    @classmethod
    def from_dict(cls, value):
        if type(value) is not dict or not set(value) <= {'session_reference','call_confirmed','route_confirmed','forwarding_approved','duration_seconds'}:
            raise PermissionError('explicit existing-call route required')
        if not all(value.get(k) is True for k in ('call_confirmed','route_confirmed','forwarding_approved')):
            raise PermissionError('call, route and forwarding approval required')
        ref=value.get('session_reference');duration=value.get('duration_seconds',60)
        if type(ref) is not str or not 1<=len(ref)<=128 or any(ord(c)<32 for c in ref):
            raise ValueError('existing call reference required')
        if type(duration) is not int or not 15<=duration<=90:
            raise ValueError('bounded native conversation duration required')
        return cls(ref,duration)

@dataclass(frozen=True)
class HostAudioDeviceRoute:
    """OS audio endpoints. Physical Listen supplies capture consent separately."""
    input_uid: str
    output_uid: str
    duration_seconds: int | None = None

    @classmethod
    def from_dict(cls, value):
        if type(value) is not dict or set(value) != {'input_uid', 'output_uid'}:
            raise PermissionError('host audio endpoint route required')
        if (value.get('input_uid'), value.get('output_uid')) != _backend_route():
            raise PermissionError('unsupported host audio route')
        return cls(value['input_uid'], value['output_uid'], _test_duration())

class NativeVoiceBridge(CodecHostBridge):
    """Drain real return PCM every20ms; gate turn-taking without storing speech."""
    def __init__(self, adapter, decoder, encoder, up, down, *, clock=time.monotonic):
        super().__init__(adapter,decoder,encoder,up,down)
        self.clock=clock;self.voice_until=0.;self.closed=False;self._close_pending=None
        self.mic_packets=self.speaker_packets=self.return_frames=0
        self.native_audio={}

    def begin(self, epoch):
        if self.closed or self._close_pending is not None:raise PermissionError("native bridge closing")
        super().begin(epoch);self.voice_until=0.

    def receive_opus(self, epoch, packet):
        if self.closed or self._close_pending is not None:raise PermissionError("native bridge closing")
        try:
            accepted=super().receive_opus(epoch,packet)
        except AudioBackpressure:
            self.native_audio=project_native_audio(self.adapter.diagnostics())
            raise
        if accepted:self.mic_packets+=1
        else:self.native_audio=project_native_audio(self.adapter.diagnostics())
        return accepted

    def poll_voice(self, epoch, frames):
        if self.closed or self._close_pending is not None or epoch!=self.epoch or self.adapter.muted:return False,[]
        diagnostics=self.adapter.diagnostics()
        self.native_audio=project_native_audio(diagnostics)
        # Loss in the robot-to-call ring is counted and packet-scoped below;
        # loss in the call-to-robot return ring or a CoreAudio error still fails closed.
        if any(diagnostics.get(k,0) for k in ('error','input_dropped')):
            raise RuntimeError('native audio callback/queue failure')
        pull=getattr(self.adapter,'pull_pcm',None)
        if callable(pull):
            queued=diagnostics.get('input_queue_frames')
            if type(queued) is not int or not 0<=queued<=7680:
                raise ValueError('bounded native return queue required')
            # Callback clock supplies the amount to drain, not a nominal20ms timer.
            # <=120ms PCM/tick, <=3 pulls and <=2 Opus packets; never enlarge ring.
            remaining=min(queued,5760);chunks=[]
            for _ in range(3):
                if not remaining:break
                request=min(remaining,2048);data=pull(epoch,request)
                if type(data) is not bytes or len(data)%2 or len(data)>request*2:
                    raise ValueError('bounded actual native PCM return required')
                if not data:break
                chunks.append(data);remaining-=len(data)//2
            pcm=b''.join(chunks)
        else:
            # Existing inert synthetic adapter contract returns padded frames.
            pcm=self.adapter.robot_output_pcm(epoch,frames)
            if type(pcm) is not bytes or len(pcm)!=frames*2:
                raise ValueError('bounded synthetic PCM return required')
        if self.adapter.muted:raise RuntimeError('native audio return failed')
        self.return_frames+=len(pcm)//2
        samples=array('h');samples.frombytes(pcm)
        if any(abs(x)>64 for x in samples):self.voice_until=self.clock()+.30
        active=self.clock()<self.voice_until
        if self.adapter.playing!=active:
            self.adapter.set_playing(epoch,active)
        if not active:
            self.pending.clear();self.down.reset();return False,[]
        if pcm:self.pending.extend(self.down.process(pcm))
        if len(self.pending)>11520:raise BufferError('native return bound exceeded')
        packets=[]
        while len(self.pending)>=2880:
            packets.append(self.encoder.encode(bytes(self.pending[:2880])))
            del self.pending[:2880]
        self.speaker_packets+=len(packets)
        return True,packets

    def close(self):
        if self.closed:return
        first_error=None
        if self._close_pending is None:
            self._close_pending=[self.adapter.close,self.decoder.close,self.encoder.close,self.up.close,self.down.close]
            if self.epoch and not self.adapter.muted:
                try:self.interrupt(self.epoch)
                except BaseException as failure:first_error=failure
        remaining=[]
        for operation in self._close_pending:
            try:operation()
            except BaseException as failure:
                remaining.append(operation)
                if first_error is None:first_error=failure
        self._close_pending=remaining
        self.pending.clear()
        self.closed=not remaining
        if first_error is not None:raise first_error

class NativeConversationSession(StreamingSession):
    def __init__(self,*args,duration_seconds=60,**kwargs):
        super().__init__(*args,**kwargs)
        test_seconds=_test_duration()
        self.duration_seconds=(test_seconds if duration_seconds is None else min(test_seconds,duration_seconds)) if test_seconds is not None else duration_seconds
        self.started_at=None
        self.on_complete=lambda:None;self.suppressed_mic_packets=0;self.backpressure_mic_packets=0
        self.backpressure_mic_streak=0
        self._pending_consent=None
        # Batched packet arrivals are not elapsed time: require a sustained300ms queue stall.
        self.max_backpressure_mic_streak=5
        self.max_backpressure_seconds=.30
        self._backpressure_since=None
        from webcam_transport import ExistingSessionWebcamRoute
        self.webcam_route=ExistingSessionWebcamRoute(self) # No camera authority in audio grant.

    def begin(self,request_id):
        epoch=self.epoch
        answer=super().begin(request_id)
        if self.epoch!=epoch:
            self.backpressure_mic_streak=0;self._backpressure_since=None
        return answer

    def _authorized(self):
        super()._authorized()
        if self.armed:
            if self.started_at is None:self.started_at=self.clock()
            if self.duration_seconds is not None and self.clock()-self.started_at>=self.duration_seconds:
                self.on_complete()
                raise TransportComplete('bounded native conversation completed')

    def start_control(self,value):
        # Only native preparation leaves the event-loop thread. Registry/turn
        # mutation and physical-consent authority remain on this thread.
        if value.get('type')!='consent' or not callable(getattr(self.bridge,'prepare',None)):
            return self.control(value)
        self._authorized()
        if not self._active() or type(value.get('epoch')) is not int or value['epoch']!=self.epoch:
            raise PermissionError('stale consent')
        if set(value)!={'type','epoch','physical'} or value['physical'] is not True:
            raise ValueError('physical consent required')
        if not self.acknowledged:raise PermissionError('handshake acknowledgement required')
        if self.armed:return self.control(value)
        if self._pending_consent is None:
            # Matches firmware PhysicalConsent's existing 5000ms gesture bound.
            self._pending_consent=(self.epoch,self.turn.id,self.turn.generation,self.clock()+5.)
            try:self.bridge.prepare(self.epoch)
            except BaseException:self._pending_consent=None;raise
        return None

    def poll_control(self):
        pending=self._pending_consent
        if pending is None:return None
        try:
            self._authorized()
            epoch,turn,generation,until=pending
            if not self._active() or not self.acknowledged or (self.epoch,self.turn.id,self.turn.generation)!=(epoch,turn,generation):
                raise PermissionError('startup consent changed')
            if self.clock()>=until:raise TimeoutError('physical consent startup expired')
            if not self.bridge.prepared():return None
            self._authorized()
            # No await between final authority check and the nonprompt sc_arm.
            self.arm_callback(epoch)
            self.armed=True;self._pending_consent=None
            return {'type':'armed','epoch':epoch}
        except BaseException:
            self._pending_consent=None;self.bridge.cancel_prepare();raise

    def receive(self,wire):
        self._authorized();epoch,sequence,speaker,payload=decode_frame(wire)
        if not self.armed or not self._active() or epoch!=self.epoch or speaker:
            raise PermissionError('native audio direction/authority')
        if sequence!=self.mic_seq:
            self.interrupt();raise ValueError('native microphone sequence gap')
        if self.bridge.adapter.playing:
            self.suppressed_mic_packets+=1;self.mic_seq+=1
            self.backpressure_mic_streak=0;self._backpressure_since=None
            return
        try:
            super().receive(wire)
        except AudioBackpressure:
            # Drop one complete, numbered uplink packet. A short queue spike is
            # recoverable; sustained loss interrupts the call instead of leaving a
            # live-looking session with a dead microphone.
            self.backpressure_mic_packets+=1;self.mic_seq+=1
            self.backpressure_mic_streak+=1
            now=self.clock()
            if self._backpressure_since is None:self._backpressure_since=now
            if (self.backpressure_mic_streak>=self.max_backpressure_mic_streak
                    and now-self._backpressure_since>=self.max_backpressure_seconds):
                self.interrupt()
                raise SustainedAudioBackpressure('sustained native microphone queue backpressure')
        else:
            self.backpressure_mic_streak=0;self._backpressure_since=None

    def audio_tick(self,frames):
        self._authorized()
        if not self.armed or not self._active():
            self.interrupt();return False,[]
        active,packets=self.bridge.poll_voice(self.epoch,frames)
        wire=[]
        for packet in packets:
            if self.speaker_seq==0xffffffff:raise OverflowError('speaker sequence exhausted')
            wire.append(encode_frame(self.epoch,self.speaker_seq,True,packet));self.speaker_seq+=1
        return active,wire

    def interrupt(self):
        self._backpressure_since=None
        self._pending_consent=None;self.armed=False;self.acknowledged=False
        if callable(getattr(self.bridge,'cancel_prepare',None)):self.bridge.cancel_prepare()
        self.webcam_route.revoke()
        try:super().interrupt()
        finally:
            camera_stop=getattr(self,'camera_stop_dispatcher',None)
            if self.webcam_route.stop_pending or (camera_stop is not None and camera_stop.custodian.pending(self.device_id)):
                # Existing gateway retains this exact session until checked camera closure.
                raise RuntimeError("camera stop custody retained")

    def statistics(self):
        return dict(native_conversation=True,consent_received=self.started_at is not None,
                    microphone_packets=self.bridge.mic_packets,speaker_packets=self.bridge.speaker_packets,
                    native_return_frames=self.bridge.return_frames,
                    suppressed_mic_packets=self.suppressed_mic_packets,
                    backpressure_mic_packets=self.backpressure_mic_packets,
                    native_audio=dict(self.bridge.native_audio))


class _PartialCloseGroup:
    def __init__(self,resources):self.pending=list(resources)
    def close(self):
        remaining=[];first=None
        for resource in self.pending:
            try:resource.close()
            except BaseException as failure:
                remaining.append(resource)
                if first is None:first=failure
        self.pending=remaining
        if first is not None:raise first


class NativeStartupPending(RuntimeError):
    """Exact native owner is still preparing; retain and retry checked close."""


class DeferredNativeBridge:
    """Connect/handshake are inert. Only accepted epoch-bound consent opens IO."""
    def __init__(self,builder,*,prepare_builder=None):
        self.prepare_builder=prepare_builder;self._startup=None;self._startup_cancel=None
        self.builder=builder;self.epoch=0;self.inner=None;self.partial=None
        self.armed=False;self.closed=False;self.adapter=self
        self._counts=dict(mic_packets=0,speaker_packets=0,return_frames=0)
        self._audio={}
    @property
    def muted(self):return not self.armed or self.inner is None or self.inner.adapter.muted
    @property
    def playing(self):return self.armed and self.inner is not None and self.inner.adapter.playing
    @property
    def mic_packets(self):return self._count('mic_packets')
    @property
    def speaker_packets(self):return self._count('speaker_packets')
    @property
    def return_frames(self):return self._count('return_frames')
    @property
    def native_audio(self):return self.inner.native_audio if self.inner else self._audio
    def _count(self,name):return self._counts[name]+(getattr(self.inner,name) if self.inner else 0)
    def begin(self,epoch):
        if self.closed or type(epoch) is not int or not self.epoch<epoch<=0xffffffff:raise PermissionError('new native epoch required')
        self._release()
        self.epoch=epoch
    def prepare(self,epoch):
        if self.closed or type(epoch) is not int or epoch!=self.epoch or not epoch or self.partial is not None:
            raise PermissionError('native preparation unavailable')
        if self._startup is not None:return
        if self.inner is not None:raise PermissionError('native cleanup pending')
        cancelled=threading.Event();self._startup_cancel=cancelled
        builder=self.builder;prepare_builder=self.prepare_builder
        def build_muted():
            if cancelled.is_set():return None
            owner=prepare_builder(cancelled.is_set) if prepare_builder is not None else builder()
            if cancelled.is_set():
                try:owner.close()
                except BaseException:
                    raise ProviderCleanupError(QualifiedProvider(owner,None,owner.close,'retained-native-owner',False)) from None
                return None
            return owner
        executor=ThreadPoolExecutor(max_workers=1,thread_name_prefix='companion-native-prepare')
        try:self._startup=executor.submit(build_muted)
        finally:executor.shutdown(wait=False)

    def startup_done(self):return self._startup is None or self._startup.done()
    def cancel_prepare(self):
        if self._startup_cancel is not None:self._startup_cancel.set()
        self.armed=False
    def _take_startup(self):
        future=self._startup
        if future is None:return None
        if not future.done():raise NativeStartupPending('native startup pending')
        self._startup=None
        try:return future.result()
        except ProviderCleanupError as failure:self.partial=failure.provider;raise
    def prepared(self):
        if self._startup is None:return False
        if not self._startup.done():return False
        if self._startup.exception() is not None:self._take_startup()
        if self._startup_cancel.is_set():raise PermissionError('native preparation cancelled')
        return True

    def arm(self,epoch):
        if self.closed or type(epoch) is not int or epoch!=self.epoch or not epoch or self.partial is not None:raise PermissionError('native consent unavailable')
        if self.armed:return # Duplicate consent cannot restart IO or the budget.
        if self.inner is not None:raise PermissionError('native cleanup pending')
        try:
            if self._startup is not None:
                if not self.prepared():raise NativeStartupPending('native startup pending')
                self.inner=self._take_startup()
            else:self.inner=self.builder()
            self.inner.begin(epoch)
            self.inner.adapter.arm_for_approved_test(epoch)
            self.armed=True
        except ProviderCleanupError as failure:
            self.partial=failure.provider
            raise
        except BaseException:
            try:self._release()
            except BaseException:pass # Exact owner stays retained for gateway cleanup.
            raise
    def _release(self):
        self.cancel_prepare()
        if self._startup is not None:
            if not self._startup.done():raise NativeStartupPending('native startup pending')
            try:owner=self._take_startup()
            except ProviderCleanupError:owner=None # exact partial retained below
            except BaseException:owner=None # failed builder owns no unreported resource
            if owner is not None:self.inner=owner
        if self.partial is not None:
            self.partial.close();self.partial=None
        if self.inner is not None:
            owner=self.inner;first=None
            try:
                if not owner.adapter.muted:owner.interrupt(self.epoch)
            except BaseException as failure:first=failure
            try:owner.close()
            except BaseException:
                raise # Retain the same owner, including partially closed resources.
            for name in self._counts:self._counts[name]+=getattr(owner,name)
            self._audio=dict(owner.native_audio);self.inner=None
            if first is not None:raise first
    def interrupt(self,epoch):
        if epoch!=self.epoch:return False
        self._release();return True
    def close(self):
        self.closed=True;self._release()
    def _live(self,epoch):
        if not self.armed or self.inner is None or epoch!=self.epoch:raise PermissionError('native capture not armed')
        return self.inner
    def _audio_call(self,method,epoch,*args):
        owner=self._live(epoch)
        try:return getattr(owner,method)(epoch,*args)
        except AudioBackpressure:
            raise # A dropped packet is bounded flow control; the session owns its streak limit.
        except BaseException:
            try:self._release()
            except BaseException:pass
            raise
    def receive_opus(self,epoch,packet):return self._audio_call('receive_opus',epoch,packet)
    def outgoing_opus(self,epoch,frames=512):return self._audio_call('outgoing_opus',epoch,frames)
    def poll_voice(self,epoch,frames):
        if self.muted or epoch!=self.epoch:return False,[]
        return self._audio_call('poll_voice',epoch,frames)
    def set_playing(self,epoch,playing):
        owner=self._live(epoch)
        try:return owner.adapter.set_playing(epoch,playing)
        except BaseException:
            try:self._release()
            except BaseException:pass
            raise


def _build_native_bridge(cancelled=None):
    """Native library, codec allocation and CoreAudio startup happen at Listen."""
    config.require_native()
    _backend_route()  # reject unknown trusted selections before library/codec access
    if hashlib.sha256(LIBRARY.read_bytes()).hexdigest()!=LIBRARY_HASH:
        raise RuntimeError('reviewed native audio library changed')
    from native_adapter import NativeHostAdapter
    from codec import OpusCodec,Resampler
    adapter=NativeHostAdapter(LIBRARY);components=[];bridge=None
    try:
        for create in (lambda:OpusCodec(16000),lambda:OpusCodec(24000),lambda:Resampler(16000,48000),lambda:Resampler(48000,24000)):
            components.append(create())
        bridge=NativeVoiceBridge(adapter,*components)
        if NATIVE_AUDIO_BACKEND == 'directional_hal_v2':
            if cancelled is None:adapter.open_directional_for_approved_test()
            else:adapter.open_directional_for_approved_test(cancelled=cancelled)
        else:
            if cancelled is None:adapter.open_for_approved_test()
            else:adapter.open_for_approved_test(cancelled=cancelled)
        return bridge
    except BaseException:
        owner=bridge if bridge is not None else _PartialCloseGroup([adapter,*components])
        try:owner.close()
        except BaseException:
            partial=QualifiedProvider(owner,None,owner.close,'retained-native-owner',route_verified=False)
            raise ProviderCleanupError(partial) from None
        raise


def make_provider(route):
    """Connect creates only an inert physical-consent-bound lifecycle owner."""
    if not isinstance(route,(NativeRoute,HostAudioDeviceRoute)):raise PermissionError('confirmed audio route required')
    bridge=DeferredNativeBridge(_build_native_bridge,prepare_builder=lambda cancelled:_build_native_bridge(cancelled=cancelled))
    if isinstance(route,HostAudioDeviceRoute):
        return QualifiedProvider(bridge,bridge.arm,bridge.close,
                                 'host-audio-device-route',route_verified=True,kind='host_audio_devices')
    return QualifiedProvider(bridge,bridge.arm,bridge.close,
                             route.session_reference,route_verified=True,kind='existing_mira')


def native_gateway(registry,sessions,route):
    def session_factory(*args,**kwargs):
        return NativeConversationSession(*args,duration_seconds=route.duration_seconds,**kwargs)
    return RemoteGateway(registry,sessions,lambda _:make_provider(route),session_factory=session_factory)

def host_audio_device_gateway(registry, sessions, route):
    if not isinstance(route, HostAudioDeviceRoute):
        raise PermissionError('host audio device route required')
    def session_factory(*args, **kwargs):
        return NativeConversationSession(*args, duration_seconds=route.duration_seconds, **kwargs)
    from camera_permission_wire import PairedSessionCameraPermissionDispatcher
    return RemoteGateway(registry, sessions, lambda _: make_provider(route), session_factory=session_factory,
        camera_dispatcher_factory=PairedSessionCameraPermissionDispatcher)
