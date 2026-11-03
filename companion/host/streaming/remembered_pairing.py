"""Exact remembered StackChan authority. Import is inert; secrets are TLS-local."""
import public_config as config
import asyncio
import json
import math
from hashlib import sha256
import re
import secrets
import time
from home_lab.devices import DeviceRegistry
from home_lab.storage import SQLiteStateStore
from numeric_pairing import strict_json, fresh_hex

DEVICE=config.DEVICE
URI=config.URI
CAPABILITIES=('microphone','speaker')

class RememberedRegistry:
    def __init__(self,directory,ca_sha256,remember_until_unix,*,clock=time.time):
        config.require()
        if ca_sha256!=config.CA_HASH or remember_until_unix!=config.EXPIRY:raise ValueError('unpinned trust')
        if type(ca_sha256) is not str or not re.fullmatch('[a-f0-9]{64}',ca_sha256):raise ValueError('invalid trust')
        if type(remember_until_unix) is not int or not 1<=remember_until_unix<=0xffffffff:raise ValueError('invalid expiry')
        self.clock=clock;self.ca=ca_sha256;self.expiry=remember_until_unix
        self.store=SQLiteStateStore(directory)
        self.registry=DeviceRegistry(self.store)
        # Dedicated extension uses the SAME durable transaction and private database.
        self.db=self.store._connection
        with self.store._lock,self.db:
            self.db.execute('CREATE TABLE IF NOT EXISTS stackchan_pair (device TEXT PRIMARY KEY, ca TEXT NOT NULL, uri TEXT NOT NULL, expiry INTEGER NOT NULL, state TEXT NOT NULL, context TEXT)')
            row=self._row()
        if row and (row['ca'],row['uri'],row['expiry'])!=(self.ca,URI,self.expiry):
            self.db.close();raise ValueError('trust changed; Forget using original profile required')
        if row and row['state']=='pending':self.forget()
    def _row(self):return self.db.execute('SELECT * FROM stackchan_pair WHERE device=?',(DEVICE,)).fetchone()
    def status(self):
        row=self._row()
        if not row:return 'unpaired'
        return 'expired' if self.clock()>=self.expiry else row['state']
    def get(self,device_id):return self.registry.get(device_id) if device_id==DEVICE else None
    def authenticate(self,credential):
        if type(credential) is not str or not 32<=len(credential)<=256:return None
        with self.store._lock:
            row=self._row()
            if not row or row['state']!='active' or self.clock()>=self.expiry:return None
            device=self.registry.get(DEVICE)
            if not device or device.status!='active' or device.capabilities!=CAPABILITIES:return None
            return DEVICE if self.store.device_for_credential(sha256(credential.encode()).hexdigest())==DEVICE else None
    def issue_pending(self,context):
        if self.clock()>=self.expiry or self.status() in ('active','pending'):raise ValueError('enrollment unavailable')
        # Gate precedes DeviceRegistry registration: crashes at any following step deny.
        with self.store._lock,self.db:
            self.db.execute('BEGIN IMMEDIATE')
            row=self._row()
            if row and row['state'] in ('active','pending'):raise ValueError('enrollment unavailable')
            self.db.execute('INSERT OR REPLACE INTO stackchan_pair VALUES (?,?,?,?,?,?)',(DEVICE,self.ca,URI,self.expiry,'pending',context))
        try:
            _,credential=self.registry.redeem_pairing_code(self.registry.create_pairing_code(),device_id=DEVICE,label='Remembered StackChan',capabilities=CAPABILITIES)
            return credential
        except BaseException:
            self.forget();raise ValueError('registration failed') from None
    def commit(self,context):
        with self.store._lock,self.db:
            changed=self.db.execute("UPDATE stackchan_pair SET state='active',context=NULL WHERE device=? AND state='pending' AND context=? AND expiry>?",(DEVICE,context,self.clock())).rowcount
            if changed!=1:raise ValueError('stored acknowledgement denied')
    def forget(self):
        # Durable gate revocation happens FIRST even if underlying removal fails.
        with self.store._lock,self.db:
            self.db.execute("UPDATE stackchan_pair SET state='revoked',context=NULL WHERE device=?",(DEVICE,))
        if self.registry.get(DEVICE) is not None:self.registry.revoke(DEVICE)

class RememberedPairing:
    def __init__(self,registry,*,emit=lambda value:None,clock=time.time,nonce_factory=lambda:secrets.token_hex(16),code_factory=lambda:f'{secrets.randbelow(1000000):06d}'):
        self.registry=registry;self.emit=emit;self.clock=clock;self.nonce_factory=nonce_factory;self.code_factory=code_factory
        self.state='idle';self.mac=False;self.robot=False;self._grant=None;self.sent=False;self.context=None;self.deadline=None
    def status(self):
        value={'state':self.state}
        if self.state=='comparing':value.update(code=self.code,remaining=max(0,math.ceil(self.deadline-self.clock())))
        if self.state=='paired':value['acknowledged']=True
        self.emit(value)
    def enroll(self):
        if self.registry.status() in ('active','pending','expired') or self.state not in ('idle','cancelled','failed','paired'):raise ValueError('Forget or fresh enrollment required')
        self.state='waiting_robot';self.mac=self.robot=self.sent=False;self._grant=None;self.context=None;self.deadline=self.clock()+120;self.status()
    def tick(self):
        if self.state in ('waiting_robot','comparing','staging') and self.clock()>=self.deadline:
            self.cancel();raise ValueError('enrollment expired')
    def hello(self,value):
        self.tick()
        fields={'type','version','request_id','device','device_nonce','boot_nonce','capabilities'}
        if self.state!='waiting_robot' or type(value) is not dict or set(value)!=fields or value['type']!='pair.remember.hello' or type(value['version']) is not int or value['version']!=1:raise ValueError('invalid hello')
        if value['device']!=DEVICE or not fresh_hex(value['request_id']) or not fresh_hex(value['device_nonce']) or type(value['boot_nonce']) is not int or not 1<=value['boot_nonce']<=0xffffffff or value['capabilities']!=list(CAPABILITIES):raise ValueError('wrong peer')
        self.request=value['request_id'];self.device_nonce=value['device_nonce'];self.host_nonce=self.nonce_factory();self.code=self.code_factory()
        if not fresh_hex(self.host_nonce) or type(self.code) is not str or not re.fullmatch('[0-9]{6}',self.code):raise ValueError('invalid entropy')
        self.context=':'.join((self.request,self.device_nonce,self.host_nonce));self.deadline=min(self.deadline,self.clock()+60);self.state='comparing';self.status()
        return self.message('challenge',code=self.code,expires_ms=max(1,min(60000,math.floor((self.deadline-self.clock())*1000))),audio_requested=False,host_ca_sha256=self.registry.ca,remember_until_unix=self.registry.expiry)
    def message(self,kind,**extra):return dict(type='pair.remember.'+kind,version=1,request_id=self.request,device_nonce=self.device_nonce,host_nonce=self.host_nonce,**extra)
    def validate(self,value,kind,extra):
        self.tick()
        if type(value) is not dict or set(value)!={'type','version','request_id','device_nonce','host_nonce',*extra} or type(value['version']) is not int or value['version']!=1 or any(value.get(k)!=v for k,v in self.message(kind).items()):raise ValueError('stale or malformed context')
    def mac_confirm(self):
        self.tick()
        if self.state!='comparing':raise ValueError('comparison required')
        self.mac=True;self._both()
    def robot_confirm(self,value):
        self.validate(value,'confirm',{'physical','remember'})
        if self.state!='comparing' or value['physical'] is not True or value['remember'] is not True:raise ValueError('physical remember required')
        self.robot=True;self._both()
    def _both(self):
        if self.mac and self.robot:
            credential=self.registry.issue_pending(self.context)
            self._grant=self.message('grant',device=DEVICE,uri=URI,credential=credential,ttl_seconds=120,audio_requested=False,host_ca_sha256=self.registry.ca,remember_until_unix=self.registry.expiry)
            self.deadline=self.clock()+120;self.state='staging';self.status()
    def take_grant(self):
        self.tick();value=self._grant;self._grant=None
        if value is not None:self.sent=True
        return value
    def stored(self,value):
        self.validate(value,'stored',{'stored'})
        if self.state!='staging' or not self.sent or value['stored'] is not True:raise ValueError('storage acknowledgement required')
        self.registry.commit(self.context);self.state='paired';self.status()
        return self.message('ready',ready=True,audio_requested=False)
    def cancel(self):
        self._grant=None
        if self.registry.status()=='pending':self.registry.forget()
        self.state='cancelled';self.status()

class RememberedGateway:
    def __init__(self,pairing):self.pairing=pairing;self.sockets=set();self.candidate=False
    def process_request(self,connection,request):
        if request.path=='/companion/pair' and self.pairing.state=='waiting_robot' and not self.candidate:return None
        return connection.respond(403,'Pairing unavailable.\n')
    async def handle(self,socket):
        if self.candidate or socket.request.path!='/companion/pair':await socket.close(code=1008,reason='Pairing unavailable');return
        self.candidate=True;self.sockets.add(socket);pending=None
        try:
            challenge=self.pairing.hello(strict_json(await asyncio.wait_for(socket.recv(),4)))
            await asyncio.wait_for(socket.send(json.dumps(challenge)),2)
            pending=asyncio.create_task(socket.recv())
            while self.pairing.state in ('comparing','staging'):
                self.pairing.tick();grant=self.pairing.take_grant()
                if grant is not None:
                    await asyncio.wait_for(socket.send(json.dumps(grant)),2);grant=None
                done,_=await asyncio.wait({pending},timeout=.05)
                if done:
                    value=strict_json(pending.result());pending=None
                    if value.get('type')=='pair.remember.confirm':self.pairing.robot_confirm(value)
                    elif value.get('type')=='pair.remember.stored':
                        ready=self.pairing.stored(value);await asyncio.wait_for(socket.send(json.dumps(ready)),2);break
                    else:raise ValueError('invalid message')
                    pending=asyncio.create_task(socket.recv())
        except BaseException:
            if self.pairing.state!='paired':self.pairing.cancel()
        finally:
            if pending:pending.cancel();await asyncio.gather(pending,return_exceptions=True)
            try:await asyncio.wait_for(socket.close(),.5)
            except BaseException:pass
            else:self.sockets.discard(socket)
    async def close(self):
        if self.pairing.state!='paired':self.pairing.cancel()
        for socket in tuple(self.sockets):
            await asyncio.wait_for(socket.close(),.5);self.sockets.discard(socket)
