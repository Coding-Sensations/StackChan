"""JSON-lines numeric-pairing worker; startup/help/import are inert.

Only an explicitly approved, unpaused human prepare command opens USB. No
credential entry, secret stdout, microphone recording or Mac audio devices.
"""
import public_config as config
import argparse
import asyncio
from contextlib import asynccontextmanager
import hashlib
import fcntl
import json
import os
from pathlib import Path
import signal
import ssl
import subprocess
import sys
import time
import uuid
import wave

from numeric_pairing import NumericPairing,NumericGateway,PairingError,parse_command,write_numeric_once,HOST,PORT,DEVICE
from pairing_receipt import decode_receipt,READY,REJECTED,EXPIRED

ROOT=config.ROOT
CERT_HASH=config.CERT_HASH
CERT_BEGIN=config.CERT_BEGIN;CERT_END=config.CERT_END
STATES={'idle','opening','usb_ready','waiting_robot','comparing','waiting_confirmation','staging','paired','restoring','cancelled','expired','failed','closed'}

DIAGNOSTIC_REASONS={'expired','cancelled','closed','pair_connection_closed','receipt_missing','reset_detected','window_changed','rejected','setup_or_transport_failed','transport_lost','approval_required','approval_consumed','paused','audio_complete','audio_transport_failed','cleanup_unconfirmed','ready_missing','certificate_expired','certificate_changed','lan_unavailable','grant_failed','revocation_failed','audio_cleanup_failed','audio_backpressure'}

# Fixed labels only. Raw exception strings never cross the worker/UI boundary.
UI_MESSAGES=DIAGNOSTIC_REASONS | {'usb_open_may_reset','verify_robot_then_physical_pair',
    'physical_pair_required','invalid_command','input_failed','invalid_message','invalid_receipt',
    'unsupported_receipt','unpinned_files','incomplete_write','invalid_state','closed',
    'input_limit','invalid_transport','invalid_preparation','invalid_deadline',
    'physical_confirmation_required','stale_context','invalid_context','candidate_already_selected',
    'invalid_staged_receipt','invalid_entropy','audio_denied','unexpected_receipt'}

def _transport_projection(value):
    if type(value) is not dict:return None
    result={'identity_verified':False,'robot_playback_verified':False,'transport_only':True}
    for key in ('consent_received','cue_complete','mic_complete','native_conversation','conversation_complete'):
        if type(value.get(key)) is bool:result[key]=value[key]
    for key in ('cue_frames','speaker_packets','speaker_frames','decoded_mic_packets','decoded_mic_frames','suppressed_mic_packets','backpressure_mic_packets','decoded_mic_peak','decoded_mic_energy','microphone_packets','native_return_frames'):
        count=value.get(key)
        if type(count) is int and 0<=count<=0x7fffffffffffffff:result[key]=count
    if value.get('outcome') in ('no_connection','fault','complete','complete_with_cleanup_fault','interrupted'):result['outcome']=value['outcome']
    from transport_diagnostics import project_diagnostics,project_native_audio
    native_audio=project_native_audio(value.get('native_audio'))
    if native_audio:result['native_audio']=native_audio
    diagnostics=project_diagnostics(value.get('diagnostics'))
    if diagnostics:result['diagnostics']=diagnostics
    return result

def _robot_boot_projection(nonce,reason):
    if type(nonce) is int and 1<=nonce<=0xffffffff and type(reason) is int and 0<=reason<=64:
        return {'robot_boot_nonce':nonce,'robot_reset_reason':reason}
    return {}

class NumericWorker:
    def __init__(self,*,approved=False,paused,serial_factory,runtime_loader,emit,clock=time.monotonic,sleep=asyncio.sleep,consume_approval=lambda:None,audio_enabled=False,transport_mode="usb"):
        if transport_mode not in ("usb","wifi"):raise PairingError("invalid_transport")
        self.transport_mode=transport_mode
        self.audio_enabled=audio_enabled;self.audio_authorized=False;self.last_transport_result=None
        self.first_terminal_result=None;self._attempt_authorized=False
        self.robot_boot_nonce=self.robot_reset_reason=None
        self.approved=approved;self.paused=paused;self.serial_factory=serial_factory;self.runtime_loader=runtime_loader
        self.emit=emit;self.clock=clock;self.sleep=sleep;self.consume_approval=consume_approval
        self.state='idle';self.port=None;self.pairing=None;self.gateway=None;self.task=None;self._closed=False
        self._deadline=None;self._usb_buffer=b'';self.cleanup_blocked=False;self._terminal_status=None;self._restoring_published=False;self._activity_status=None
    def status(self,state,message=None):
        self.state=state;value={'state':state}
        if message:value['message']=message
        self._publish(value)
    def _publish(self,value):
        value=dict(value)
        if 'message' in value and (type(value['message']) is not str or value['message'] not in UI_MESSAGES):
            value['message']='setup_or_transport_failed'
        self.state=value['state']
        if value['state'] in ('failed','expired','cancelled','closed') and value.get('message')!='cleanup_unconfirmed' and (self.port is not None or self.gateway is not None or self.cleanup_blocked):
            self._terminal_status=value
            self._show_restoring()
        else:
            if value['state'] in ('failed','expired','cancelled','closed') and self._attempt_authorized and self.first_terminal_result is None:
                first={'state':value['state'],'audio_enabled':self.audio_authorized}
                first.update(_robot_boot_projection(self.robot_boot_nonce,self.robot_reset_reason))
                reason=value.get('message')
                if type(reason) is str and reason in DIAGNOSTIC_REASONS:first['reason']=reason
                remaining=value.get('remaining')
                if type(remaining) is int and 0<=remaining<=120:first['remaining']=remaining
                transport=_transport_projection(self.last_transport_result) if self.audio_authorized else None
                if transport is not None:first['transport']=transport
                self.first_terminal_result=first
            self.emit(value)
    def _show_restoring(self):
        if not self._restoring_published:
            self._restoring_published=True
            self.emit({'state':'restoring'})
    def _show_activity(self):
        if not self.audio_authorized or self.pairing is None or self.pairing.state!='paired':return
        transport=self.gateway.transport if self.gateway is not None else None
        activity=getattr(transport,'activity_state','physical_consent')
        if activity not in ('physical_consent','conversation'):activity='physical_consent'
        audio_remaining=getattr(transport,'audio_remaining',None) if activity=='conversation' else None
        if type(audio_remaining) is not int or not 0<=audio_remaining<=60:audio_remaining=None
        snapshot=(activity,audio_remaining)
        if snapshot==self._activity_status:return
        self._activity_status=snapshot
        value={'state':'paired','acknowledged':True,'activity':activity}
        if audio_remaining is not None:value['audio_remaining']=audio_remaining
        self._publish(value)
    def pairing_status(self,value):
        self._publish(value)
    async def command(self,command):
        if command not in ('prepare','confirm','cancel','retry'):raise PairingError('invalid_command')
        if command=='cancel':
            if self.pairing:self.pairing.cancel()
            if self.task and not self.task.done():self.task.cancel();await asyncio.gather(self.task,return_exceptions=True)
            if not self.cleanup_blocked:self.status('cancelled','cancelled')
            return
        if command=='confirm':
            if not self.pairing:raise PairingError('invalid_state')
            self.pairing.mac_confirm();return
        if self._closed or self.cleanup_blocked:raise PairingError('closed')
        allowed=('idle',) if command=='prepare' else ('failed','cancelled','expired','closed')
        if self.state not in allowed or (self.task and not self.task.done()):raise PairingError('invalid_state')
        self.task=asyncio.create_task(self._prepare())
    async def wait(self):
        if self.task:await asyncio.gather(self.task,return_exceptions=True)
    def _read(self):
        try:value=self.port.read(64)
        except Exception:raise PairingError('transport_lost') from None
        if type(value) is not bytes or len(value)>64:raise PairingError('invalid_transport')
        return value
    async def _drain(self):
        until=self.clock()+2;count=0
        while self.clock()<until:
            count+=len(self._read())
            if count>4096:raise PairingError('input_limit')
            await self.sleep(.02)
    async def _receipt(self):
        until=min(self._deadline,self.clock()+120);count=0
        while self.clock()<until:
            start=self._usb_buffer.find(b'SCPS')
            if start<0:self._usb_buffer=self._usb_buffer[-3:]
            else:
                self._usb_buffer=self._usb_buffer[start:]
                if len(self._usb_buffer)>=22:
                    frame,self._usb_buffer=self._usb_buffer[:22],self._usb_buffer[22:]
                    try:return decode_receipt(frame)
                    except ValueError:raise PairingError('invalid_receipt') from None
            chunk=self._read();count+=len(chunk);self._usb_buffer+=chunk
            if count>4096:raise PairingError('input_limit')
            await self.sleep(.02)
        raise PairingError('ready_missing')
    async def _prepare(self):
        runtime=None
        try:
            if self.approved is not True or type(self.audio_enabled) is not bool:raise PairingError('approval_required')
            if self.paused():raise PairingError('paused')
            try:expected_boot_nonce=self.consume_approval()
            except FileExistsError:raise PairingError('approval_consumed') from None
            self.audio_authorized=self.audio_enabled;self.last_transport_result=None;self._attempt_authorized=True
            self.robot_boot_nonce=self.robot_reset_reason=None
            self._deadline=self.clock()+180;self.pairing=None;self.gateway=None;self._usb_buffer=b'';self._terminal_status=None;self._restoring_published=False;self._activity_status=None
            if self.transport_mode=='usb':
                self.status('opening','usb_open_may_reset')
                self.port=self.serial_factory()  # Configured closed, DTR/RTS false.
                try:self.port.open()
                except Exception:raise PairingError('transport_lost') from None
                await self._drain()
                self.status('usb_ready','verify_robot_then_physical_pair')
                self.status('waiting_robot','physical_pair_required')
                ready=await self._receipt()
                ready_received_at=self.clock()
                if ready.status==REJECTED:raise PairingError('rejected')
                if ready.status==EXPIRED:raise PairingError('expired')
                if ready.status!=READY:raise PairingError('unsupported_receipt')
                if not _robot_boot_projection(ready.boot_nonce,ready.reset_reason):raise PairingError('invalid_receipt')
                self.robot_boot_nonce=ready.boot_nonce;self.robot_reset_reason=ready.reset_reason
                if expected_boot_nonce is not None and ready.boot_nonce!=expected_boot_nonce:raise PairingError('reset_detected')
            else:
                ready_received_at=self.clock();self._deadline=ready_received_at+120
                self.status('waiting_robot','physical_pair_required')
            # USB requires Ready; Wi-Fi requires its separately approved physical pairing window.
            if self.paused():raise PairingError('paused')
            runtime=self.runtime_loader()
            self.pairing=NumericPairing(issuer=runtime.issue,revoke=runtime.revoke,emit=self.pairing_status,clock=self.clock,audio_requested=self.audio_authorized)
            if self.transport_mode=='usb':self.pairing.prepare(uuid.uuid4().hex,ready.boot_nonce,started_at=ready_received_at,deadline_budget_seconds=118)
            else:self.pairing.prepare_wireless(expected_boot_nonce=expected_boot_nonce,started_at=ready_received_at,deadline_budget_seconds=118)
            self.pairing.tick()
            self.gateway=NumericGateway(self.pairing,runtime.transport,audio_enabled=self.audio_authorized)
            async with runtime.listener(self.gateway):
                self.pairing.tick()
                if self.transport_mode=='usb':write_numeric_once(self.port,self.pairing.numeric_frame())
                while self.clock()<self._deadline:
                    self.pairing.tick()
                    if self.pairing.state in ('failed','cancelled','expired','closed'):break
                    self._show_activity()
                    if self.transport_mode=='usb':
                        # A new Ready indicates reset/window replacement. Never retry.
                        data=self._read();self._usb_buffer+=data
                        marker=self._usb_buffer.find(b'SCPS')
                        if marker>=0 and len(self._usb_buffer)-marker>=22:
                            seen=decode_receipt(self._usb_buffer[marker:marker+22])
                            if seen.status==READY:raise PairingError('reset_detected' if seen.boot_nonce!=ready.boot_nonce else 'window_changed')
                            if seen.status==REJECTED:raise PairingError('rejected')
                            if seen.status==EXPIRED:raise PairingError('expired')
                            raise PairingError('unexpected_receipt')
                        if len(self._usb_buffer)>4096:raise PairingError('input_limit')
                    await self.sleep(.05)
                else:raise PairingError('expired')
        except asyncio.CancelledError:
            if self.pairing:self.pairing.cancel()
            raise
        except PairingError as failure:
            state='expired' if str(failure)=='expired' else 'failed'
            if self.pairing:self.pairing.cancel(state,str(failure))
            else:self.status(state,str(failure))
        except TimeoutError:
            elapsed=self.pairing is not None and self.pairing.deadline is not None and self.clock()>=self.pairing.deadline
            state='expired' if elapsed else 'failed'
            message='expired' if elapsed else 'setup_or_transport_failed'
            if self.pairing:self.pairing.cancel(state,message)
            else:self.status(state,message)
        except Exception:
            if self.pairing:self.pairing.cancel('failed','setup_or_transport_failed')
            else:self.status('failed','setup_or_transport_failed')
        finally:
            if self.port is not None or self.gateway is not None:self._show_restoring()
            cleanup_issues=set()
            try:
                if self.gateway:
                    await self.gateway.close()
                    if not self.pairing.revocation_failed:self.gateway=None
            except Exception:
                self.cleanup_blocked=True;cleanup_issues.add('audio')
            finally:
                if self.pairing and self.pairing.revocation_failed:
                    self.cleanup_blocked=True;cleanup_issues.add('grant')
                if self.port is not None:
                    try:
                        self.port.close()
                        if self.port.is_open is not False:
                            self.cleanup_blocked=True;cleanup_issues.add('usb')
                        else:self.port=None
                    except Exception:self.cleanup_blocked=True;cleanup_issues.add('usb')
                if self.audio_authorized and runtime is not None:
                    try:
                        raw_result=getattr(runtime.transport,'last_result',None)
                        self.last_transport_result=_transport_projection(raw_result)
                    except Exception:
                        raw_result=None;self.last_transport_result=None
                    diagnostics=raw_result.get('diagnostics') if type(raw_result) is dict else None
                    cleanup_count=diagnostics.get('cleanup_fault_count') if type(diagnostics) is dict else 0
                    cleanup_failed=type(raw_result) is dict and (raw_result.get('outcome')=='complete_with_cleanup_fault' or (type(cleanup_count) is int and cleanup_count>0))
                    if cleanup_failed:
                        self.cleanup_blocked=True;cleanup_issues.add('audio')
                if self._terminal_status is not None and self._terminal_status.get('message')=='audio_transport_failed':
                    diagnostics=self.last_transport_result.get('diagnostics') if type(self.last_transport_result) is dict else None
                    fault=diagnostics.get('fault') if type(diagnostics) is dict else None
                    if type(fault) is dict and fault.get('type')=='SustainedAudioBackpressure':
                        self._terminal_status['message']='audio_backpressure'
                if cleanup_issues:
                    value=dict(self._terminal_status or {'state':'failed'})
                    value['state']='failed';value['message']='cleanup_unconfirmed'
                    value['cleanup']=sorted(cleanup_issues)
                    self._terminal_status=value
                if self._terminal_status is not None and (not self.cleanup_blocked or self._terminal_status.get('message')=='cleanup_unconfirmed'):
                    value=self._terminal_status;self._terminal_status=None
                    self._publish(value)
    async def close(self):
        self._closed=True
        if self.task and not self.task.done():self.task.cancel()
        await self.wait()
        if self.gateway is not None:
            await self.gateway.close();self.gateway=None
        if self.port is not None:
            self.port.close()
            if self.port.is_open is not False:raise PairingError('cleanup_unconfirmed')
            self.port=None
        if self.pairing and self.pairing.revocation_failed:self.pairing.cancel('closed','closed')
        self.cleanup_blocked=False
        if not self.cleanup_blocked:
            if self.pairing:self.pairing.cancel('closed','closed')
            self.status('closed','closed')

class _Runtime:
    def __init__(self,context,registry,transport):self.context=context;self.registry=registry;self.transport=transport
    def issue(self,ttl):
        # DeviceRegistry remains in memory; no durable grant database is supplied.
        _,token=self.registry.redeem_pairing_code(self.registry.create_pairing_code(),device_id=DEVICE,label='StackChan numeric transport test',capabilities=('microphone','speaker'))
        return token
    def revoke(self):self.registry.revoke(DEVICE)
    @asynccontextmanager
    async def listener(self,gateway):
        from websockets.asyncio.server import serve
        gateway.pairing.tick()
        remaining=gateway.pairing.deadline-gateway.pairing.clock()
        async with asyncio.timeout(remaining):
            async with serve(gateway.handle,HOST,PORT,ssl=self.context,process_request=gateway.process_request,max_size=4112,max_queue=2,compression=None,ping_interval=None,open_timeout=2,close_timeout=.25,server_header=None):
                yield

def _serial_factory(port):
    library=ROOT/'.tools/espressif/python_env/idf5.5_py3.13_env/lib/python3.13/site-packages'
    if str(library) not in sys.path:sys.path.insert(0,str(library))
    host=Path(__file__).resolve().parents[1]
    if str(host) not in sys.path:sys.path.insert(0,str(host))
    from numeric_pairing_serial import NumericPairingSerial
    connection=NumericPairingSerial(port=None,baudrate=115200,timeout=0,write_timeout=2)
    connection.dtr=False;connection.rts=False;connection.port=port
    return connection

def _runtime(args, native_route=None):
    config.require()
    _validated_audio_mode(args,native_route)
    # New trust is an explicit public-profile selection, independent of remembered
    # device state. Finite approval consumption remains unchanged in _prepare.
    persistent_trust=getattr(args,'persistent_trust',False)
    if type(persistent_trust) is not bool:raise PairingError('unpinned_files')
    trust_directory=ROOT/('.tools/persistent-companion' if persistent_trust else '.tools/local-companion-tls')
    cert=trust_directory/'server.pem';key=cert.with_name('server-key.pem')
    cue=ROOT/'.tools/native-call-preparation/transport-speaker-cue.wav'
    if Path(args.cert).resolve()!=cert or Path(args.key).resolve()!=key or Path(args.cue).resolve()!=cue:raise PairingError('unpinned_files')
    if persistent_trust:
        from remembered_pairing_worker import load_trust_profile
        try:context,profile,_=load_trust_profile()
        except Exception:raise PairingError('certificate_changed') from None
        if not time.time()<profile['remember_until_unix']-130:raise PairingError('certificate_expired')
    else:
        if not CERT_BEGIN<=time.time()<CERT_END-130:raise PairingError('certificate_expired')
        if cert.stat().st_size>8192 or hashlib.sha256(cert.read_bytes()).hexdigest()!=CERT_HASH:raise PairingError('certificate_changed')
        context=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.minimum_version=ssl.TLSVersion.TLSv1_2;context.load_cert_chain(cert,key)
    if subprocess.check_output(['/usr/sbin/ipconfig','getifaddr','en1'],timeout=2,text=True).strip()!=HOST:raise PairingError('lan_unavailable')
    for path in (ROOT/'.tools/bridge-python',config.HOME_LAB_ROOT/"src",Path(__file__).resolve().parents[1]/'native-audio',ROOT/'bridge/host-adapter',ROOT/'bridge/local-endpoint'):
        if str(path) not in sys.path:sys.path.insert(0,str(path))
    from home_lab.devices import DeviceRegistry
    registry=DeviceRegistry()
    return _Runtime(context,registry,_audio_transport(args,registry,native_route))

def _validated_audio_mode(args,native_route):
    mode=getattr(args,'audio_mode','transport_test')
    if mode not in ('transport_test','native_conversation'):raise PairingError('approval_required')
    if mode=='native_conversation':
        from native_conversation import NativeRoute
        if not isinstance(native_route,NativeRoute) or native_route.duration_seconds!=60:raise PairingError('approval_required')
    elif native_route is not None:raise PairingError('approval_required')
    return mode

def _audio_transport(args,registry,native_route):
    """Select exactly the disclosed/approved mode; construction starts no IO."""
    mode=_validated_audio_mode(args,native_route)
    from home_lab.sessions import ConversationSessions
    if mode=='native_conversation':
        from native_conversation import native_gateway
        return native_gateway(registry,ConversationSessions(),native_route)
    cue=ROOT/'.tools/native-call-preparation/transport-speaker-cue.wav'
    if cue.stat().st_size>600000 or hashlib.sha256(cue.read_bytes()).hexdigest()!='8c70a511b8e9dc570c277e3286343ed52077315158485a106c494557364d22b9':raise PairingError('cue_changed')
    with wave.open(str(cue),'rb') as source:
        if (source.getnchannels(),source.getsampwidth(),source.getframerate())!=(1,2,24000) or not 1<=source.getnframes()<=240000:raise PairingError('invalid_cue')
        pcm=source.readframes(source.getnframes())
    from transport_test import TransportGateway
    return TransportGateway(registry,ConversationSessions(),pcm,transport_approved=args.approved_test is True and args.approved_audio is True)

def _output(value):
    # Only fixed state/message labels, comparison number and boolean/counts.
    print(json.dumps(value,separators=(',',':')),flush=True)

def _consume_approval(*,audio_enabled=False,native_route_sink=None,requested_mode="transport_test",requested_transport="usb"):
    # Exactly one human Pair click consumes the approved check, even on failure.
    approval=json.loads((ROOT/'notes/NUMERIC-PAIRING-RUNTIME-AUTHORIZATION.json').read_text())
    if approval.get('approved') is not True or approval.get('audio_allowed') is not audio_enabled or not time.time()<approval.get('valid_until_unix',0):
        raise PairingError('approval_required')
    if type(audio_enabled) is not bool or requested_transport not in ('usb','wifi') or approval.get('transport','usb')!=requested_transport:raise PairingError('approval_required')
    expected=approval.get('expected_boot_nonce')
    if ((audio_enabled and requested_transport=="usb") or expected is not None) and (type(expected) is not int or not 1<=expected<=0xffffffff):raise PairingError('approval_required')
    mode=approval.get('audio_mode','transport_test')
    if mode not in ('transport_test','native_conversation') or mode!=requested_mode:raise PairingError('approval_required')
    if mode=='native_conversation':
        if not audio_enabled or approval.get('native_conversation_allowed') is not True or native_route_sink is None:raise PairingError('approval_required')
        try:
            from native_conversation import NativeRoute
            native_route=NativeRoute.from_dict(approval.get('native_route'))
        except (PermissionError,ValueError):raise PairingError('approval_required') from None
    else:native_route=None
    marker='NUMERIC-PAIRING-AUDIO-CHECK-CONSUMED' if audio_enabled else 'NUMERIC-PAIRING-CHECK-CONSUMED'
    with (ROOT/'.tools'/marker).open('x') as receipt:
        receipt.write(json.dumps({'consumed_unix':time.time(),'pid':os.getpid(),'audio_allowed':audio_enabled,'transport':requested_transport})+'\n')
    if native_route_sink is not None:native_route_sink(native_route)
    return expected

def _append_history(metadata):
    line=(json.dumps(metadata,separators=(',',':'))+'\n').encode('utf-8')
    if len(line)>4096:return
    path=ROOT/'notes/NUMERIC-PAIRING-STATUS-HISTORY.jsonl'
    # A locked append never truncates earlier evidence; stop at either bound.
    with path.open('a+b') as history:
        fcntl.flock(history.fileno(),fcntl.LOCK_EX)
        try:
            size=os.fstat(history.fileno()).st_size
            if size+len(line)>65536:return
            history.seek(0);existing=history.read(65536)
            if existing.count(b'\n')>=128:return
            history.seek(0,os.SEEK_END);history.write(line);history.flush()
        finally:fcntl.flock(history.fileno(),fcntl.LOCK_UN)

def _status_metadata(value,*,audio_enabled=False,transport_result=None,first_terminal_result=None,robot_boot_nonce=None,robot_reset_reason=None):
    state=value.get('state')
    if state not in STATES:raise PairingError('invalid_status')
    metadata={'state':state,'observed_unix':time.time(),'worker_pid':os.getpid(),'audio_enabled':audio_enabled is True}
    metadata.update(_robot_boot_projection(robot_boot_nonce,robot_reset_reason))
    if type(value.get('acknowledged')) is bool:metadata['acknowledged']=value['acknowledged']
    if type(value.get('remaining')) is int and 0<=value['remaining']<=120:metadata['remaining']=value['remaining']
    reason=value.get('message')
    if type(reason) is str and reason in DIAGNOSTIC_REASONS:metadata['reason']=reason
    if audio_enabled is True:
        projected=_transport_projection(transport_result)
        if projected is not None:metadata['transport']=projected
    if type(first_terminal_result) is dict and first_terminal_result.get('state') in ('failed','expired','cancelled','closed'):
        first={'state':first_terminal_result['state'],'audio_enabled':first_terminal_result.get('audio_enabled') is True}
        first.update(_robot_boot_projection(first_terminal_result.get('robot_boot_nonce'),first_terminal_result.get('robot_reset_reason')))
        reason=first_terminal_result.get('reason')
        if type(reason) is str and reason in DIAGNOSTIC_REASONS:first['reason']=reason
        remaining=first_terminal_result.get('remaining')
        if type(remaining) is int and 0<=remaining<=120:first['remaining']=remaining
        projected=_transport_projection(first_terminal_result.get('transport')) if first['audio_enabled'] else None
        if projected is not None:first['transport']=projected
        metadata['first_terminal']=first
    # No comparison number, credential, raw message or network payload is saved.
    _append_history(metadata)
    path=ROOT/'notes/NUMERIC-PAIRING-LIVE-STATUS.json'
    temporary=path.with_suffix('.tmp')
    temporary.write_text(json.dumps(metadata)+'\n');temporary.replace(path)
    _output(value)

async def _run(args):
    route=[None]
    worker=NumericWorker(approved=args.approved_test,paused=lambda:(ROOT/'.tools/TRANSPORT-PAIRING-PAUSED').exists(),serial_factory=lambda:_serial_factory(args.port),runtime_loader=lambda:_runtime(args,route[0]),emit=lambda value:_status_metadata(value,audio_enabled=worker.audio_authorized,transport_result=worker.last_transport_result,first_terminal_result=worker.first_terminal_result,robot_boot_nonce=worker.robot_boot_nonce,robot_reset_reason=worker.robot_reset_reason),consume_approval=lambda:_consume_approval(audio_enabled=args.approved_audio,native_route_sink=lambda value:route.__setitem__(0,value),requested_mode=args.audio_mode,requested_transport=args.transport),audio_enabled=args.approved_audio,transport_mode=args.transport)
    worker.status('idle')
    loop=asyncio.get_running_loop();reader=asyncio.StreamReader(limit=2048);protocol=asyncio.StreamReaderProtocol(reader)
    pipe=None;monitor=None
    try:
        pipe,_=await loop.connect_read_pipe(lambda:protocol,sys.stdin)
        def terminate():
            if pipe:pipe.close()
        for sig in (signal.SIGTERM,signal.SIGINT):loop.add_signal_handler(sig,terminate)
        async def watch_cleanup():
            while True:
                if worker.cleanup_blocked:
                    terminate();return
                await asyncio.sleep(.05)
        monitor=asyncio.create_task(watch_cleanup())
        while True:
            line=await reader.readline()
            if not line:break
            try:await worker.command(parse_command(line.decode('utf-8')))
            except Exception:worker.status(worker.state,'invalid_command')
    except Exception:worker.status('failed','input_failed')
    finally:
        if monitor:
            monitor.cancel();await asyncio.gather(monitor,return_exceptions=True)
        await worker.close()
        if pipe:pipe.close()
        for sig in (signal.SIGTERM,signal.SIGINT):loop.remove_signal_handler(sig)

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--approved-test',action='store_true')
    parser.add_argument('--persistent-trust',action='store_true',help='Explicit one-year public trust profile; independent of remembered device state and finite audio approval.')
    parser.add_argument('--approved-audio',action='store_true',help='Separate audio approval; requires matching one-use audio authorization and --approved-test.')
    parser.add_argument('--audio-mode',choices=('transport_test','native_conversation'),default='transport_test',help='Must match the one-use authorization and displayed GUI disclosure; native never falls back to transport test.')
    parser.add_argument('--transport',choices=('usb','wifi'),default='usb',help='Independent pairing transport; Wi-Fi never constructs a serial port.')
    parser.add_argument('--port',default='/dev/cu.usbmodem31201')
    parser.add_argument('--cert',default=str(ROOT/'.tools/local-companion-tls/server.pem'))
    parser.add_argument('--key',default=str(ROOT/'.tools/local-companion-tls/server-key.pem'))
    parser.add_argument('--cue',default=str(ROOT/'.tools/native-call-preparation/transport-speaker-cue.wav'))
    args=parser.parse_args(argv)
    try:asyncio.run(_run(args))
    except BaseException:
        # Never disguise an owner-close error as a successful worker exit. The
        # controller also checks this process's exit code before confirming release.
        _output({'state':'failed','message':'cleanup_unconfirmed','cleanup':['audio']})
        return 1
    return 0
if __name__=='__main__':raise SystemExit(main())
