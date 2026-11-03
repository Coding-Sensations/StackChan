"""Bounded remote connection driver; no listener, TLS setup, pairing or provider.
Caller supplies an already authenticated StreamingSession and accepted socket.
This driver works without CoreAudio or a Mac; it does not establish Mira identity.
"""
import asyncio
import json
import time
from streaming_session import parse_control
from transport_diagnostics import TransportDiagnostics


class TransportComplete(Exception):
    """Expected finite audio/conversation completion; cleanup remains diagnosed."""


class OwnedSocketClose:
    """One exact close operation; timeout/cancellation never abandons its task."""
    def __init__(self,socket):self.socket=socket;self.task=None;self.confirmed=False
    async def close(self,timeout=.25,**kwargs):
        if self.confirmed:return
        if self.task is None:self.task=asyncio.create_task(self.socket.close(**kwargs))
        done,_=await asyncio.wait({self.task},timeout=timeout)
        if not done:raise TimeoutError('socket cleanup pending')
        task=self.task
        try:task.result()
        except BaseException:self.task=None;raise
        self.confirmed=True


async def run_connection(socket, session, *, duration=900.0, send_timeout=0.25, diagnostics=None, retain_cleanup=None, socket_owner=None):
    """Own one receive task, service 20 ms audio ticks, and fail closed on exit.
    Existing session authority/watchdog is checked even when the peer is silent.
    Blocking codec/provider work must be bounded by the supplied adapter.
    """
    if duration is not None and (type(duration) not in (int, float) or not 0 < duration <= 900):
        raise ValueError('bounded connection duration required')
    if type(send_timeout) not in (int, float) or not 0 < send_timeout <= 0.25:
        raise ValueError('bounded send deadline required')
    receive = None
    socket_owner=socket_owner or OwnedSocketClose(socket)
    diagnostics = diagnostics or TransportDiagnostics(getattr(session,'clock',time.monotonic))
    diagnostics.observe(session)
    primary = None
    deadline = float("inf") if duration is None else time.monotonic() + duration
    next_tick = time.monotonic()
    playback = None
    pending_video = None
    def service_video_after_audio():
        nonlocal pending_video
        route=getattr(session,"webcam_route",None)
        if route is not None:
            if pending_video is not None:route.offer(pending_video)
            route.video_tick_after_audio()
        pending_video=None
    try:
        receive = asyncio.create_task(socket.recv())
        while True:
            now = time.monotonic()
            if now >= deadline:
                diagnostics.set_phase('duration')
                raise TimeoutError('connection duration expired')
            diagnostics.set_phase('authority')
            diagnostics.observe(session)
            session._authorized()
            ready, _ = await asyncio.wait({receive}, timeout=min(max(0, next_tick-now), deadline-now))
            if ready:
                diagnostics.set_phase('receive')
                message = receive.result()
                if type(message) is str:
                    diagnostics.set_phase('control')
                    control = parse_control(message,max_bytes=2048)
                    diagnostics.control_label(control.get('type'))
                    if control.get('type') in ('camera_driver_stop','camera_stop_ack','camera_stop_challenge','camera_stop_acknowledged'):
                        dispatcher=getattr(session,'camera_stop_dispatcher',None)
                        answer=dispatcher.handle_control(control) if dispatcher is not None else None
                        route=getattr(session,'webcam_route',None)
                        if route is not None:route.dispatch_stop_complete()
                    elif control.get('type') in ('camera_clock_sample','camera_permission_request','camera_permission_off','camera_permission_confirmation'):
                        dispatcher=getattr(session,'camera_dispatcher',None)
                        answer=dispatcher.handle_control(control) if dispatcher is not None else None
                    elif control.get('type') in ('camera_on','camera_off','camera_status'):
                        route=getattr(session,'webcam_route',None)
                        answer=route.camera_control(control) if route is not None else None
                    else:
                        if len(message.encode())>1024:raise ValueError("bounded audio control required")
                        start=getattr(session,'start_control',session.control)
                        answer = start(control)
                        diagnostics.accepted_control(session,control.get('type'))
                    if answer is not None:
                        response = json.dumps(answer, separators=(',', ':'), allow_nan=False)
                        if len(response.encode()) > (2048 if answer.get("type","").startswith("camera_clock_") or answer.get("type","").startswith("camera_permission_") else 1024):
                            raise ValueError('bounded control response required')
                        diagnostics.set_phase('control_send')
                        await asyncio.wait_for(socket.send(response), send_timeout)
                        stop=getattr(session,'camera_stop_dispatcher',None)
                        if stop is not None and stop.control_sent(answer):
                            route=getattr(session,'webcam_route',None)
                            if route is not None:route.dispatch_stop_complete()
                elif type(message) is bytes and message.startswith(b"WCV1") and len(message)<=4112:
                    pending_video=message # video processed only after due audio/control work
                elif type(message) is bytes and 17 <= len(message) <= 4112:
                    diagnostics.set_phase('audio_receive')
                    session.receive(message)
                else:
                    raise ValueError('bounded audio/control message required')
                receive = asyncio.create_task(socket.recv())
            poll=getattr(session,'poll_control',None)
            if callable(poll):
                diagnostics.set_phase('native_startup')
                answer=poll()
                if answer is not None:
                    diagnostics.accepted_control(session,'consent')
                    response=json.dumps(answer,separators=(',',':'),allow_nan=False)
                    if len(response.encode())>1024:raise ValueError('bounded control response required')
                    diagnostics.set_phase('control_send')
                    await asyncio.wait_for(socket.send(response),send_timeout)
            now = time.monotonic()
            if now >= next_tick:
                if session.armed:
                    diagnostics.set_phase('playback_state')
                    packets = None
                    pump = getattr(session, 'audio_tick', None)
                    if callable(pump):
                        speaking, packets = pump(960)
                    else:
                        speaking = session.bridge.adapter.playing
                    if type(speaking) is not bool:
                        raise ValueError('explicit provider playback state required')
                    state = (session.epoch, speaking)
                    if state != playback:
                        diagnostics.set_phase('playback_state')
                        response = json.dumps(session.playing(speaking), separators=(',', ':'), allow_nan=False)
                        diagnostics.set_phase('playback_send')
                        await asyncio.wait_for(socket.send(response), send_timeout)
                        playback = state
                    if not speaking:
                        next_tick = now + 0.02
                        # Camera controls still drain below; no speaking audio packets.
                        packets=[]
                    # 960 mono48k samples = 20 ms. Codec output is bounded per tick.
                    diagnostics.set_phase('audio_pull')
                    if packets is None:packets = session.outgoing(960)
                    if type(packets) is not list or len(packets) > 2:
                        raise BufferError('outgoing packet bound exceeded')
                    for packet in packets:
                        if type(packet) is not bytes or not 17 <= len(packet) <= 4112:
                            raise ValueError('bounded outgoing audio required')
                        diagnostics.set_phase('audio_send')
                        await asyncio.wait_for(socket.send(packet), send_timeout)
                        diagnostics.speaker_sent()
                next_tick = now + 0.02
            # At most two bounded existing-session camera controls, before video.
            for name in ('camera_stop_dispatcher','camera_dispatcher'):
                dispatcher=getattr(session,name,None)
                if dispatcher is not None:
                    tick=getattr(dispatcher,'tick',None)
                    if callable(tick):tick()
                    control=dispatcher.pop_control()
                    if control is not None:
                        response=json.dumps(control,separators=(',',':'),allow_nan=False)
                        if len(response.encode())>(1024 if name=='camera_stop_dispatcher' else 2048):raise ValueError('bounded camera control response')
                        await asyncio.wait_for(socket.send(response),send_timeout)
                        sent=getattr(dispatcher,'control_sent',None)
                        if callable(sent):sent(control)
            service_video_after_audio()
    except BaseException as failure:
        primary = failure
        diagnostics.observe(session)
        diagnostics.fault(failure)
        raise
    finally:
        # Invalidate authority first; cleanup never replaces the original fault.
        cleanup = None
        route=getattr(session,"webcam_route",None)
        if route is not None:route.revoke()
        pending_video=None
        stop=getattr(session,'camera_stop_dispatcher',None)
        if stop is not None:stop.connection_lost()
        camera=getattr(session,'camera_dispatcher',None)
        if camera is not None:
            lost=getattr(camera,'connection_lost',None)
            if callable(lost):lost()
        try:
            session.close()
        except BaseException as failure:
            diagnostics.fault(failure,'session_cleanup',cleanup=True)
            cleanup = failure
            if retain_cleanup is not None:retain_cleanup('session',session)
        if receive is not None:
            receive.cancel()
            try:
                await asyncio.gather(receive, return_exceptions=True)
            except BaseException as failure:
                diagnostics.fault(failure,'receive_cleanup',cleanup=True)
                if cleanup is None:cleanup = failure
        try:
            await socket_owner.close(timeout=send_timeout)
        except BaseException as failure:
            diagnostics.fault(failure,'socket_cleanup',cleanup=True)
            if cleanup is None:cleanup = failure
            if retain_cleanup is not None:retain_cleanup('socket',socket_owner)
        if primary is None and cleanup is not None:
            raise cleanup
