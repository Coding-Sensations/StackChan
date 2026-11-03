"""Deployable WebSocket sidecar, inert until an operator starts a listener.
Reuses HomeLab registry/sessions; no new owner endpoints, token issuer, provider
account or TLS credentials. Existing-Mira attachment adapter is not supplied.
"""
from dataclasses import dataclass, field
from gateway_admission import authorize_audio_request
from gateway_connection import run_connection, TransportComplete,OwnedSocketClose
from transport_diagnostics import TransportDiagnostics
from connection_recording import record_event
from streaming_session import StreamingSession


class ProviderCleanupError(RuntimeError):
    """Factory failure carrying the exact partial owner, never media or secrets."""
    def __init__(self,provider):
        super().__init__('provider setup cleanup unconfirmed');self.provider=provider

@dataclass
class QualifiedProvider:
    bridge: object = field(repr=False)
    arm: object = field(repr=False)
    close: object = field(repr=False)
    route_reference: str = field(repr=False)
    route_verified: bool = False
    kind: str = 'unavailable'


class RemoteGateway:
    def __init__(self, registry, sessions, provider_factory=None, *, fixture=False, session_factory=StreamingSession, webcam_owner=None, webcam_deliver=None, camera_dispatcher_factory=None):
        self.registry = registry; self.sessions = sessions
        self.provider_factory = provider_factory; self.fixture = fixture
        self._active = set()
        self._retained = {}
        self._cleaning=set()
        self._unclaimed_closers=set()
        self.session_factory=session_factory
        self.webcam_owner=webcam_owner;self.webcam_deliver=webcam_deliver
        from camera_stop_dispatcher import CameraStartCustodian
        self.camera_custodian=CameraStartCustodian();self.camera_dispatcher_factory=camera_dispatcher_factory
        self.last_result={'outcome':'no_connection','native_conversation':True}
        self.completion_started=False
        self._activity_session=None
        self.diagnostic_sink=None

    @property
    def activity_state(self):
        # Read-only observation of accepted physical consent, never a grant.
        return 'conversation' if self._activity_session is not None and self._activity_session.armed is True else 'physical_consent'

    @property
    def audio_remaining(self):
        session=self._activity_session
        if session is None or session.armed is not True:return None
        started=getattr(session,'started_at',None);duration=getattr(session,'duration_seconds',None)
        if type(started) not in (int,float) or type(duration) is not int or not 1<=duration<=60:return None
        import math
        return max(0,min(60,math.ceil(duration-(session.clock()-started))))

    def process_request(self, connection, request):
        try:
            authorize_audio_request(self.registry, request.path, list(request.headers.raw_items()))
        except PermissionError:
            record_event(self.diagnostic_sink,'http_admission','admission_denied')
            return connection.respond(403, 'Audio admission denied.\n')
        if self.provider_factory is None:
            record_event(self.diagnostic_sink,'http_admission','route_unavailable')
            return connection.respond(503, 'Qualified conversation adapter unavailable.\n')
        record_event(self.diagnostic_sink,'http_admission','upgrade_allowed')
        return None

    async def handle(self, socket):
        provider = None; session = None; device_id = None; driver_started = False
        diagnostics=TransportDiagnostics();outcome='fault';retained=[];socket_owner=OwnedSocketClose(socket)
        try:
            diagnostics.set_phase('authorization')
            grant = authorize_audio_request(self.registry, socket.request.path, list(socket.request.headers.raw_items()))
            diagnostics.set_phase('admission')
            if grant.device_id in self._retained:await self._close_device(grant.device_id)
            if grant.device_id in self._active or self.provider_factory is None:
                raise PermissionError('device busy or route unavailable')
            self._active.add(grant.device_id); device_id = grant.device_id
            diagnostics.set_phase('provider_create')
            try:provider = self.provider_factory(grant.device_id)
            except ProviderCleanupError as failure:
                retained.append(('provider',failure.provider))
                diagnostics.fault(failure,'session_cleanup',cleanup=True)
                raise
            allowed_kinds = {'synthetic_fixture'} if self.fixture else {'existing_mira', 'host_audio_devices'}
            if not isinstance(provider, QualifiedProvider) or provider.route_verified is not True or provider.kind not in allowed_kinds:
                raise PermissionError('qualified conversation adapter required')
            if type(provider.route_reference) is not str or not 1 <= len(provider.route_reference) <= 128:
                raise PermissionError('verified audio route reference required')
            diagnostics.set_phase('session_create')
            session = self.session_factory(self.registry, self.sessions, provider.bridge,
                grant.credential, grant.device_id, provider.arm,
                fixture=self.fixture, native_session_ref=provider.route_reference)
            if hasattr(session,"webcam_route"):
                from webcam_transport import ExistingSessionWebcamRoute
                from camera_stop_dispatcher import ExistingSessionStopDispatcher
                stop=ExistingSessionStopDispatcher(session,self.webcam_owner,self.camera_custodian)
                session.camera_stop_dispatcher=stop
                session.webcam_route=ExistingSessionWebcamRoute(session,self.webcam_owner,deliver=self.webcam_deliver or (lambda frame:None),stop_evidence=stop.consumer,stop_dispatcher=stop,custodian=self.camera_custodian)
            if self.camera_dispatcher_factory is not None:
                session.camera_dispatcher=self.camera_dispatcher_factory(session,self.camera_custodian)
            session.on_complete=lambda:setattr(self,'completion_started',True)
            self._activity_session=session
            record_event(self.diagnostic_sink,'websocket','session_started')
            driver_started = True
            await run_connection(socket, session, duration=None if provider.kind == "host_audio_devices" else 900.0, diagnostics=diagnostics,retain_cleanup=lambda kind,resource:retained.append((kind,resource)),socket_owner=socket_owner)
        except TransportComplete:
            diagnostics.expected_complete();outcome='complete'
        except Exception as failure:
            diagnostics.fault(failure)
            # No token, raw media or provider exception is exposed to the peer.
            if not any(owner is socket_owner for _,owner in retained):
                try:await socket_owner.close(code=1008,reason='Companion route closed')
                except BaseException as failure:
                    diagnostics.fault(failure,'socket_cleanup',cleanup=True)
                    retained.append(('socket',socket_owner))
        finally:
            # Cancellation can arrive during an admission cleanup retry, before
            # the driver owns this new socket. Retain it independently then.
            if not socket_owner.confirmed and not any(owner is socket_owner for _,owner in retained):
                try:await socket_owner.close()
                except BaseException as failure:
                    diagnostics.fault(failure,'socket_cleanup',cleanup=True)
                    retained.append(('socket',socket_owner))
            try:
                if session is not None and not driver_started:
                    try:session.close()
                    except BaseException as failure:
                        retained.append(('session',session));diagnostics.fault(failure,'session_cleanup',cleanup=True)
                if provider is not None and isinstance(provider, QualifiedProvider):
                    try:provider.close()
                    except BaseException as failure:
                        retained.append(('provider',provider));diagnostics.fault(failure,'session_cleanup',cleanup=True)
            finally:
                if self._activity_session is session:self._activity_session=None
                if device_id is not None:
                    if retained:self._retained[device_id]=list({(kind,id(owner)):(kind,owner) for kind,owner in retained}.values())
                    else:self._active.discard(device_id)
                if outcome=='complete' and diagnostics.snapshot().get('cleanup_fault_count',0):outcome='complete_with_cleanup_fault'
                reason={'complete':'transport_complete','complete_with_cleanup_fault':'cleanup_fault'}.get(outcome,'transport_fault')
                record_event(self.diagnostic_sink,'session_closed',reason,diagnostics.snapshot())
                if device_id is None:
                    self._unclaimed_closers.update(owner for kind,owner in retained if kind=='socket')
                stats=session.statistics() if session is not None and hasattr(session,'statistics') else {}
                self.last_result=dict(stats,outcome=outcome,conversation_complete=self.completion_started,diagnostics=diagnostics.snapshot())

    def explicit_webcam_on(self):
        session=self._activity_session
        if session is None or not hasattr(session,'webcam_route'):
            raise PermissionError('current authenticated webcam session unavailable')
        return session.webcam_route.explicit_on()

    def explicit_webcam_off(self):
        session=self._activity_session
        if session is None or not hasattr(session,'webcam_route'):return None
        session.webcam_route.revoke()
        return session.webcam_route.stop_status

    async def _close_device(self,device_id):
        # At most one retry owns this device; never open a replacement first.
        if device_id in self._cleaning:raise RuntimeError('device cleanup already pending')
        self._cleaning.add(device_id)
        try:
            resources=self._retained.get(device_id,[]);remaining=[];first=None
            for kind,resource in resources:
                try:
                    if kind=='socket':await resource.close()
                    else:resource.close()
                except BaseException as failure:
                    remaining.append((kind,resource))
                    if first is None:first=failure
            if remaining:self._retained[device_id]=remaining
            else:
                self._retained.pop(device_id,None);self._active.discard(device_id)
            if first is not None:raise first
        finally:self._cleaning.discard(device_id)

    async def close(self):
        """Retry exact retained owners; never reacquire or arm a resource."""
        first=None
        for device_id in tuple(self._retained):
            try:await self._close_device(device_id)
            except BaseException as failure:
                if first is None:first=failure
        for owner in tuple(self._unclaimed_closers):
            try:await owner.close()
            except BaseException as failure:
                if first is None:first=failure
            else:self._unclaimed_closers.discard(owner)
        if first is not None:raise first

    def listener(self, *, host='127.0.0.1', port=8766, ssl_context=None, trusted_tls_proxy=False):
        """Return a lazy serve context. Only entering it binds a socket.
        Production requires TLS or an explicitly verified proxy on loopback.
        Fixture mode can bind unencrypted localhost only; firmware rejects it.
        """
        if type(port) is not int or not 0 <= port <= 65535 or (port == 0 and not self.fixture):
            raise ValueError('explicit listener port required')
        if self.fixture:
            if host != '127.0.0.1' or ssl_context is not None or trusted_tls_proxy:
                raise ValueError('fixture listener is localhost only')
        elif ssl_context is None and not (trusted_tls_proxy and host == '127.0.0.1'):
            raise ValueError('verified TLS boundary required')
        elif trusted_tls_proxy and host != '127.0.0.1':
            raise ValueError('trusted proxy requires loopback bind')
        from websockets.asyncio.server import serve
        return serve(self.handle, host, port, ssl=ssl_context,
            process_request=self.process_request, max_size=4112, max_queue=2,
            compression=None, ping_interval=None, open_timeout=2,
            close_timeout=.25, server_header=None)
