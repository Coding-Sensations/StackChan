"""Single-candidate USB/TLS numeric pairing. Import creates no I/O or grant.

Numbers are comparison UX, not a password. TLS identity is an independent
prerequisite. Only both physical robot confirmation and local Mac confirmation
permit a volatile scoped grant; no audio authority until a correlated staged ACK.
"""
import public_config as config
import asyncio
import json
import math
import re
import secrets
import struct
import time

DEVICE=config.DEVICE
URI=config.URI
HOST=config.HOST;PORT=config.PORT
HEX=re.compile(r'[0-9a-f]{32}\Z')
def fresh_hex(value):
    return type(value) is str and HEX.fullmatch(value) is not None and value!='0'*32
class PairingError(Exception):pass

def strict_json(wire):
    if type(wire) is not str or not 1<=len(wire.encode('utf-8'))<=2048:
        raise PairingError('invalid_message')
    def pairs(values):
        result={}
        for key,value in values:
            if key in result:raise PairingError('duplicate_fields')
            result[key]=value
        return result
    try:
        value=json.loads(wire,object_pairs_hook=pairs,parse_constant=lambda _:(_ for _ in ()).throw(PairingError('invalid_message')))
    except (ValueError,RecursionError):raise PairingError('invalid_message') from None
    if type(value) is not dict:raise PairingError('invalid_message')
    return value

def parse_command(line):
    value=strict_json(line)
    if set(value)!={'command'} or value['command'] not in ('prepare','confirm','cancel','retry'):
        raise PairingError('invalid_command')
    return value['command']

def write_numeric_once(port,frame):
    if type(frame) is not bytes or len(frame)!=25:raise PairingError('invalid_frame')
    try:accepted=port.write(frame)
    except Exception:raise PairingError('transport_lost') from None
    if type(accepted) is not int or accepted!=25:raise PairingError('incomplete_write')

class NumericPairing:
    def __init__(self,*,issuer,revoke,emit=lambda value:None,clock=time.monotonic,
                 nonce_factory=lambda:secrets.token_hex(16),code_factory=lambda:f'{secrets.randbelow(1000000):06d}',audio_requested=False):
        if type(audio_requested) is not bool:raise PairingError('invalid_audio_scope')
        self._audio_requested=audio_requested
        self.issuer=issuer;self.revoke=revoke;self.emit=emit;self.clock=clock
        self.nonce_factory=nonce_factory;self.code_factory=code_factory
        self.state='idle';self.code=None;self.request=None;self.boot_nonce=None
        self.device_nonce=self.host_nonce=None
        self.deadline=self.challenge_deadline=self.ack_deadline=None
        self.mac=self.robot=False;self._issued=False;self._revoked=False
        self._grant=None;self._sent=False;self.audio_allowed=False;self.revocation_failed=False
        self._wireless=False;self._expected_boot=None
    @property
    def audio_requested(self):return self._audio_requested
    def status(self,message=None):
        value={'state':self.state,'acknowledged':self.audio_allowed}
        if self.state=='comparing' and self.code is not None:value['code']=self.code
        if self.deadline is not None:value['remaining']=max(0,math.ceil(self.deadline-self.clock()))
        if message is not None:value['message']=message
        self.emit(value);return value
    def prepare(self,request,boot_nonce,*,started_at=None,deadline_budget_seconds=120):
        config.require()
        now=self.clock()
        if started_at is None:started_at=now
        if type(started_at) not in (int,float) or not math.isfinite(started_at) or not 0<=started_at<=now or type(deadline_budget_seconds) is not int or not 1<=deadline_budget_seconds<=120:
            raise PairingError('invalid_deadline')
        if self.state!='idle' or not fresh_hex(request) or type(boot_nonce) is not int or not 1<=boot_nonce<=0xffffffff:
            raise PairingError('invalid_preparation')
        deadline=started_at+deadline_budget_seconds
        if now>=deadline:raise PairingError('expired')
        self.request=request;self.boot_nonce=boot_nonce;self.deadline=deadline
        self.state='waiting_robot';self.status()
    def prepare_wireless(self,*,expected_boot_nonce=None,started_at=None,deadline_budget_seconds=120):
        if self.state!='idle' or type(deadline_budget_seconds) is not int or not 1<=deadline_budget_seconds<=120:
            raise PairingError('invalid_preparation')
        if expected_boot_nonce is not None and (type(expected_boot_nonce) is not int or not 1<=expected_boot_nonce<=0xffffffff):
            raise PairingError('invalid_preparation')
        now=self.clock()
        if started_at is None:started_at=now
        if type(started_at) not in (int,float) or not math.isfinite(started_at) or not 0<=started_at<=now:raise PairingError('invalid_deadline')
        deadline=started_at+deadline_budget_seconds
        if now>=deadline:raise PairingError('expired')
        self._wireless=True;self._expected_boot=expected_boot_nonce
        self.deadline=deadline
        self.state='waiting_robot';self.status()

    def numeric_frame(self):
        self.tick()
        if self.state!='waiting_robot' or self._wireless:raise PairingError('invalid_state')
        return struct.pack('>4sB16sI',b'SCNP',1,bytes.fromhex(self.request),self.boot_nonce)
    def tick(self):
        if self.state in ('idle','cancelled','expired','failed','closed'):return
        now=self.clock()
        if now>=self.deadline or (not self._issued and self.challenge_deadline is not None and now>=self.challenge_deadline):
            self.cancel('expired','expired');raise PairingError('expired')
        if self._issued and not self.audio_allowed and self.ack_deadline is not None and now>=self.ack_deadline:
            self.cancel('failed','receipt_missing');raise PairingError('receipt_missing')
    def _context(self,value,kind,extra):
        self.tick()
        required={'type','version','request_id','device_nonce','host_nonce',*extra}
        if type(value) is not dict or set(value)!=required or value['type']!=kind or type(value['version']) is not int or value['version']!=1:
            raise PairingError('invalid_message')
        if value['request_id']!=self.request or value['device_nonce']!=self.device_nonce or value['host_nonce']!=self.host_nonce:
            raise PairingError('stale_context')
        if any(type(value[key]) is not str or not HEX.fullmatch(value[key]) for key in ('request_id','device_nonce','host_nonce')):
            raise PairingError('invalid_context')
    def hello(self,value):
        self.tick()
        if self.state!='waiting_robot':raise PairingError('candidate_already_selected')
        fields={'type','version','request_id','device','device_nonce','boot_nonce','capabilities'}
        if type(value) is not dict or set(value)!=fields or value['type']!='pair.hello' or type(value['version']) is not int or value['version']!=1:
            raise PairingError('invalid_message')
        if value['device']!=DEVICE or not fresh_hex(value['request_id']):raise PairingError('stale_context')
        if type(value['boot_nonce']) is not int or not 1<=value['boot_nonce']<=0xffffffff:raise PairingError('reset_detected')
        if not self._wireless and (value['request_id']!=self.request or value['boot_nonce']!=self.boot_nonce):raise PairingError('reset_detected' if value['boot_nonce']!=self.boot_nonce else 'stale_context')
        if self._expected_boot is not None and value['boot_nonce']!=self._expected_boot:raise PairingError('reset_detected')
        if not fresh_hex(value['device_nonce']) or value['capabilities']!=['microphone','speaker']:
            raise PairingError('invalid_message')
        if self._wireless:self.request=value['request_id'];self.boot_nonce=value['boot_nonce']
        self.device_nonce=value['device_nonce'];self.host_nonce=self.nonce_factory();self.code=self.code_factory()
        if not fresh_hex(self.host_nonce) or type(self.code) is not str or not re.fullmatch('[0-9]{6}',self.code):raise PairingError('invalid_entropy')
        self.challenge_deadline=min(self.deadline,self.clock()+60)
        self.state='comparing';self.status()
        return dict(type='pair.challenge',version=1,request_id=self.request,device_nonce=self.device_nonce,host_nonce=self.host_nonce,code=self.code,audio_requested=self.audio_requested,expires_ms=min(60000,math.floor((self.challenge_deadline-self.clock())*1000)))
    def mac_confirm(self):
        self.tick()
        if self.state not in ('comparing','staging','paired'):raise PairingError('invalid_state')
        self.mac=True;self._both()
    def robot_confirm(self,value):
        self._context(value,'pair.confirm',{'physical'})
        if self.state not in ('comparing','staging','paired') or value['physical'] is not True:raise PairingError('physical_confirmation_required')
        self.robot=True;self._both()
    def _both(self):
        if self.mac and self.robot and not self._issued:
            ttl=math.floor(self.deadline-self.clock())
            if not 1<=ttl<=120:raise PairingError('expired')
            try:
                credential=self.issuer(ttl)
            except Exception:
                try:self.revoke();self._revoked=True
                except Exception:self.revocation_failed=True
                self.state='failed';self.status('grant_failed')
                raise PairingError('grant_failed') from None
            if type(credential) is not str or not 32<=len(credential)<=256 or any(not 33<=ord(c)<=126 for c in credential):
                try:self.revoke();self._revoked=True
                except Exception:self.revocation_failed=True
                self.state='failed';self.status('grant_failed')
                raise PairingError('grant_failed')
            self._issued=True;self._grant=dict(type='pair.grant',version=1,request_id=self.request,device_nonce=self.device_nonce,host_nonce=self.host_nonce,device=DEVICE,uri=URI,credential=credential,ttl_seconds=ttl,audio_requested=self.audio_requested)
            self.ack_deadline=self.clock()+5;self.state='staging';self.status()
    def take_grant(self):
        self.tick()
        if not self._issued or self._sent:return None
        self._sent=True;grant=self._grant;self._grant=None;return grant
    def staged(self,value):
        self._context(value,'pair.staged',{'staged'})
        if not self._sent or value['staged'] is not True or self.state not in ('staging','paired'):raise PairingError('invalid_staged_receipt')
        self.audio_allowed=True;self.state='paired';self.code=None;self.status()
    def cancel(self,state='cancelled',message='cancelled'):
        self.audio_allowed=False;self.code=None;self._grant=None
        if (self._issued or self.revocation_failed) and not self._revoked:
            try:self.revoke();self._revoked=True;self.revocation_failed=False
            except Exception:
                self.revocation_failed=True;self.state='failed';self.status('revocation_failed')
                raise PairingError('revocation_failed') from None
        self.state=state;self.status(message)

class NumericGateway:
    """One pairing socket held through its explicitly scoped temporary audio test."""
    def __init__(self,pairing,transport,*,audio_enabled=False):
        self.pairing=pairing;self.transport=transport;self.audio_enabled=audio_enabled;self._candidate=False;self._audio=False
        self.sockets=set()
    def process_request(self,connection,request):
        if request.path=='/companion/pair' and not self._candidate and self.pairing.state=='waiting_robot':return None
        if request.path=='/companion/audio' and self.audio_enabled and self.pairing.audio_requested and self.pairing.audio_allowed and not self._audio:
            return self.transport.process_request(connection,request)
        return connection.respond(403,'Pairing unavailable.\n')
    async def handle(self,socket):
        self.sockets.add(socket)
        pending=None
        try:
            if socket.request.path=='/companion/audio':
                if not self.audio_enabled or not self.pairing.audio_requested or not self.pairing.audio_allowed or self._audio:raise PairingError('audio_denied')
                self._audio=True
                await self.transport.handle(socket)
                if self.pairing.state not in ('cancelled','expired','failed','closed'):
                    result=getattr(self.transport,'last_result',None)
                    diagnostics=result.get('diagnostics') if type(result) is dict else None
                    cleanup_count=diagnostics.get('cleanup_fault_count') if type(diagnostics) is dict else 0
                    cleanup_failed=type(result) is dict and (result.get('outcome')=='complete_with_cleanup_fault' or (type(cleanup_count) is int and cleanup_count>0))
                    complete=type(result) is dict and result.get('outcome')=='complete' and not cleanup_failed and (all(result.get(key) is True for key in ('consent_received','cue_complete','mic_complete')) or (result.get('native_conversation') is True and result.get('conversation_complete') is True and result.get('consent_received') is True))
                    message='audio_cleanup_failed' if cleanup_failed else ('audio_complete' if complete else 'audio_transport_failed')
                    self.pairing.cancel('closed' if complete else 'failed',message)
                return
            if socket.request.path!='/companion/pair' or self._candidate:raise PairingError('candidate_already_selected')
            self._candidate=True
            hello=await asyncio.wait_for(socket.recv(),4)
            challenge=self.pairing.hello(strict_json(hello))
            await asyncio.wait_for(socket.send(json.dumps(challenge)),2)
            pending=asyncio.create_task(socket.recv())
            while True:
                self.pairing.tick()
                if self.pairing.state in ('cancelled','expired','failed','closed'):break
                grant=self.pairing.take_grant()
                if grant is not None:
                    await asyncio.wait_for(socket.send(json.dumps(grant)),2)
                    grant=None
                done,_=await asyncio.wait({pending},timeout=.05)
                self.pairing.tick()  # Classify an actually elapsed local deadline before late socket results.
                if done:
                    value=strict_json(pending.result())
                    if value.get('type')=='pair.confirm':self.pairing.robot_confirm(value)
                    elif value.get('type')=='pair.staged':
                        self.pairing.staged(value)
                        ready=dict(type='pair.ready',version=1,request_id=self.pairing.request,device_nonce=self.pairing.device_nonce,host_nonce=self.pairing.host_nonce,audio_requested=self.pairing.audio_requested,ready=True)
                        await asyncio.wait_for(socket.send(json.dumps(ready)),2)
                    else:raise PairingError('invalid_message')
                    pending=asyncio.create_task(socket.recv())
        except asyncio.CancelledError:
            raise
        except Exception:
            if socket.request.path=='/companion/pair' and self.pairing.state not in ('cancelled','expired','failed','closed') and not (self._audio and getattr(self.transport,'completion_started',False)):
                self.pairing.cancel('failed','pair_connection_closed')
        finally:
            if pending:
                pending.cancel();await asyncio.gather(pending,return_exceptions=True)
            if socket.request.path=='/companion/pair' and self.pairing.state not in ('cancelled','expired','failed','closed') and not (self._audio and getattr(self.transport,'completion_started',False)):
                self.pairing.cancel('failed','pair_connection_closed')
            try:await asyncio.wait_for(socket.close(),.5)
            except Exception:pass
            else:self.sockets.discard(socket)
    async def close(self):
        if self.pairing.revocation_failed or self.pairing.state not in ('cancelled','expired','failed','closed'):
            try:
                self.pairing.cancel()
            except Exception:
                if not getattr(self.pairing,'revocation_failed',False):raise
        sockets=tuple(self.sockets)
        results=await asyncio.wait_for(asyncio.gather(*(socket.close() for socket in sockets),return_exceptions=True),.75)
        failures=[]
        for socket,result in zip(sockets,results):
            if isinstance(result,BaseException):failures.append(result)
            else:self.sockets.discard(socket)
        transport_close=getattr(self.transport,'close',None)
        if callable(transport_close):
            try:await transport_close()
            except BaseException as failure:failures.append(failure)
        if failures:raise failures[0]
