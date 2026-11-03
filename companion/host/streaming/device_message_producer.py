# SPDX-License-Identifier: MIT
"""Proposed outbound device-message custody, independent of inbound room events.

No production native authority or receiver transport is supplied. Dependencies
are privileged controller objects, never model arguments or authentication flags.
Calls are serialized on one asyncio event loop. SQLite consumes operations before
any dispatch; no startup recovery resends. The caller owns journal preservation.
"""
import asyncio
from dataclasses import asdict, dataclass
import hashlib
import json
import math
import os
from pathlib import Path
import re
import sqlite3
import stat
import time

_ID = re.compile(r'[A-Za-z0-9_.:-]{1,64}\Z')
_PRINCIPALS = ('user','device','session','receiver','provider','binding','generation','native_incarnation')

def _identifier(value):
    return type(value) is str and _ID.fullmatch(value) is not None

def _finite(value):
    return type(value) in (int,float) and math.isfinite(value)

@dataclass(frozen=True)
class Scope:
    user: str
    device: str
    session: str
    receiver: str
    provider: str
    binding: str
    generation: str
    native_incarnation: str
    operation: str
    expires: float

@dataclass(frozen=True)
class Envelope:
    user: str
    device: str
    session: str
    receiver: str
    provider: str
    binding: str
    generation: str
    native_incarnation: str
    operation: str
    expires: float
    message_sha256: str
    envelope_sha256: str

    @classmethod
    def from_scope(cls, scope, text):
        fields=asdict(scope)
        fields['message_sha256']=hashlib.sha256(text.encode('utf-8')).hexdigest()
        raw=json.dumps(fields,sort_keys=True,separators=(',',':'),allow_nan=False).encode('ascii')
        return cls(**fields,envelope_sha256=hashlib.sha256(b'outbound-device-message-v1\0'+raw).hexdigest())

@dataclass(frozen=True)
class Receipt:
    envelope: Envelope
    disposition: str

@dataclass(frozen=True)
class Result:
    state: str
    operation: str = ''
    envelope_sha256: str = ''
    receipt_checked: bool = False
    native_attested: bool = False

class UnavailableAuthority:
    def current(self, scope, action, exact_port):
        return False

class _Journal:
    """Metadata only. No eviction: a full ledger refuses new operations."""
    def __init__(self, path):
        supplied=Path(path)
        if not supplied.is_absolute(): raise ValueError('explicit absolute journal path required')
        parent=supplied.parent.resolve(strict=True)
        info=parent.stat()
        if info.st_uid!=os.getuid() or stat.S_IMODE(info.st_mode)&0o022:
            raise ValueError('trusted caller-owned journal directory required')
        self.path=parent/supplied.name
        fd=os.open(self.path,os.O_RDWR|os.O_CREAT|os.O_NOFOLLOW,0o600)
        try:
            info=os.fstat(fd)
            if not stat.S_ISREG(info.st_mode) or info.st_uid!=os.getuid() or stat.S_IMODE(info.st_mode)!=0o600 or info.st_nlink!=1:
                raise ValueError('private regular journal required')
        finally: os.close(fd)
        self.db=sqlite3.connect(self.path,timeout=0.,isolation_level=None)
        try:
            self.db.execute('PRAGMA journal_mode=DELETE')
            self.db.execute('PRAGMA synchronous=EXTRA')
            self.db.execute('PRAGMA max_page_count=128')
            self.db.execute('PRAGMA journal_size_limit=0')
            self.db.execute('CREATE TABLE IF NOT EXISTS operations (operation TEXT PRIMARY KEY, envelope TEXT NOT NULL, digest TEXT NOT NULL, state TEXT NOT NULL)')
            if self.db.execute('SELECT count(*) FROM operations').fetchone()[0]>64:
                raise ValueError('journal capacity invalid')
        except BaseException:
            self.db.close();raise

    def reserve(self, envelope):
        self.db.execute('BEGIN IMMEDIATE')
        try:
            if self.db.execute('SELECT 1 FROM operations WHERE operation=?',(envelope.operation,)).fetchone():
                self.db.execute('ROLLBACK');return 'replay'
            for (previous,) in self.db.execute("SELECT envelope FROM operations WHERE state='unknown'"):
                fields=json.loads(previous)
                if all(fields.get(k)==getattr(envelope,k) for k in _PRINCIPALS):
                    self.db.execute('ROLLBACK');return 'unresolved'
            if self.db.execute('SELECT count(*) FROM operations').fetchone()[0]>=64:
                self.db.execute('ROLLBACK');return 'capacity'
            self.db.execute('INSERT INTO operations VALUES (?,?,?,?)',(envelope.operation,json.dumps(asdict(envelope),sort_keys=True,separators=(',',':')),envelope.envelope_sha256,'unknown'))
            self.db.execute('COMMIT')
            return 'reserved'
        except BaseException:
            if self.db.in_transaction:self.db.execute('ROLLBACK')
            raise

    def lookup(self, operation):
        return self.db.execute('SELECT envelope,digest,state FROM operations WHERE operation=?',(operation,)).fetchone()

    def record(self, envelope, state):
        self.db.execute('UPDATE operations SET state=? WHERE operation=? AND digest=?',(state,envelope.operation,envelope.envelope_sha256))

    def close(self): self.db.close()

class _Operation:
    def __init__(self, scope, envelope):
        self.scope=scope;self.envelope=envelope
        self.result=Result('unknown',scope.operation,envelope.envelope_sha256)
        self.task=None;self.cancel_requested=False
        self.cleanup_task=None;self.cleanup_cancel_requested=False;self.cleanup_failed=False;self.invalidated=False

class Producer:
    """One captured outbound port; no receiver derivation, listener or networking."""
    def __init__(self, journal_path, source=None, port=None, clock=time.monotonic, *, wait_limit=3.):
        if not _finite(wait_limit) or not .001<=wait_limit<=3.:
            raise ValueError('bounded wait required')
        now=clock()
        if not _finite(now):raise ValueError('finite monotonic clock required')
        self.source,self._port,self.clock=UnavailableAuthority() if source is None else source,port,clock
        self._last=now;self._wait_limit=wait_limit;self._journal=_Journal(journal_path)
        self._live={};self._closed=False;self._busy=False;self._guarding=False;self._revision=0;self._broken=False;self._closing=False;self._journal_closed=False

    def _guard(self, scope, action):
        if self._closed or self._broken or self._guarding:return False
        revision=self._revision;self._guarding=True
        try:
            valid=self.source.current(scope,action,self._port)
            now=self.clock()
        except BaseException:return False
        finally:self._guarding=False
        if valid is not True or self._closed or revision!=self._revision or not _finite(now) or not self._last<=now<scope.expires:
            return False
        self._last=now
        return True

    @staticmethod
    def _scope_valid(scope):
        return type(scope) is Scope and all(_identifier(getattr(scope,k)) for k in (*_PRINCIPALS,'operation')) and _finite(scope.expires)

    @staticmethod
    def _observe(task):
        try:task.exception()
        except BaseException:pass

    @staticmethod
    def _checked(value,envelope,dispositions):
        return type(value) is Receipt and type(value.envelope) is Envelope and value.envelope==envelope and type(value.disposition) is str and value.disposition in dispositions

    def _record(self, operation, state, checked=False):
        try:self._journal.record(operation.envelope,state)
        except BaseException:
            self._broken=True;state='unknown';checked=False
        operation.result=Result(state,operation.scope.operation,operation.envelope.envelope_sha256,checked)
        return operation.result

    async def submit(self, scope, text):
        if self._closed or self._busy or self._guarding or self._broken:return Result('unavailable')
        if not self._scope_valid(scope) or type(text) is not str:return Result('invalid')
        try:valid=1<=len(text.encode('utf-8'))<=1024 and '\x00' not in text
        except UnicodeError:valid=False
        if not valid:return Result('invalid')
        if not self._guard(scope,'submit'):return Result('unavailable')
        now=self.clock()
        if not _finite(now) or not self._last<=now<scope.expires<=now+120:return Result('unavailable')
        envelope=Envelope.from_scope(scope,text)
        self._busy=True
        try:
            admission=self._journal.reserve(envelope)
            if admission!='reserved':return Result(admission,scope.operation,envelope.envelope_sha256)
            operation=_Operation(scope,envelope);self._live[scope.operation]=operation
            async def dispatch():
                if not self._guard(scope,'submit'):raise ValueError('authority unavailable')
                return await self._port.submit(envelope,text)
            if not self._guard(scope,'submit'):return operation.result
            task=asyncio.create_task(dispatch());task.add_done_callback(self._observe);operation.task=task
            left=scope.expires-self.clock()
            if not _finite(left) or left<=0:raise ValueError('expired')
            done,_=await asyncio.wait({task},timeout=min(self._wait_limit,left))
            if not done:raise ValueError('completion unknown')
            value=task.result();operation.task=None
            if operation.invalidated:return operation.result
            if not self._guard(scope,'receipt') or not self._checked(value,envelope,{'accepted','receiver_acknowledged'}):
                raise ValueError('receipt unavailable')
            result=self._record(operation,value.disposition,True)
            if not result.receipt_checked or not self._guard(scope,'receipt'):
                return self._record(operation,'unknown')
            return result
        except BaseException:
            operation=self._live.get(scope.operation)
            if operation is None:self._broken=True;return Result('unavailable')
            if operation.task is not None and not operation.task.done() and not operation.cancel_requested:
                operation.task.cancel();operation.cancel_requested=True
            if operation.invalidated:return operation.result
            return self._record(operation,'unknown')
        finally:self._busy=False

    def receipt(self, scope):
        if not self._scope_valid(scope) or not self._guard(scope,'receipt'):return Result('unavailable')
        operation=self._live.get(scope.operation)
        if operation is not None:
            if operation.scope!=scope:return Result('unavailable')
            return operation.result
        try:row=self._journal.lookup(scope.operation)
        except BaseException:self._broken=True;return Result('unavailable')
        if row is None:return Result('absent',scope.operation)
        fields=json.loads(row[0])
        if any(fields.get(k)!=getattr(scope,k) for k in (*_PRINCIPALS,'operation','expires')):return Result('unavailable')
        if not self._guard(scope,'receipt'):return Result('unavailable')
        return Result('historical_unknown',scope.operation,row[1])

    def _cleanup_guard(self, operation):
        # A distinct retained-owner cleanup grant may survive send lease expiry.
        # It permits only cancel(original envelope), never another message send.
        if self._guarding:return False
        self._guarding=True
        try:
            valid=self.source.current(operation.scope,'cleanup',self._port)
            now=self.clock()
        except BaseException:return False
        finally:self._guarding=False
        if valid is not True or not _finite(now) or now<self._last:return False
        self._last=now
        return True

    async def _cleanup(self, operation, timeout=None):
        if operation.result.state=='cancelled':return True
        operation.invalidated=True
        self._record(operation,'unknown')
        if operation.task is not None and not operation.task.done() and not operation.cancel_requested:
            operation.task.cancel();operation.cancel_requested=True
        if operation.cleanup_failed:return False
        if not self._cleanup_guard(operation):return False
        if operation.cleanup_task is None:
            async def cancel_owned():
                if not self._cleanup_guard(operation):raise ValueError('cleanup authority unavailable')
                return await self._port.cancel(operation.envelope)
            operation.cleanup_task=asyncio.create_task(cancel_owned())
            operation.cleanup_task.add_done_callback(self._observe)
        task=operation.cleanup_task
        try:
            done,_=await asyncio.wait({task},timeout=self._wait_limit if timeout is None else timeout)
            if not done:
                if not operation.cleanup_cancel_requested:task.cancel();operation.cleanup_cancel_requested=True
                return False
            value=task.result()
            if not self._cleanup_guard(operation) or not self._checked(value,operation.envelope,{'cancelled'}):
                operation.cleanup_failed=True;return False
            if operation.task is not None:
                if not operation.task.done():return False
                self._observe(operation.task);operation.task=None
            operation.cleanup_task=None
            result=self._record(operation,'cancelled',True)
            if not result.receipt_checked or not self._cleanup_guard(operation):
                self._record(operation,'unknown');return False
            return True
        except BaseException:
            if not task.done() and not operation.cleanup_cancel_requested:
                task.cancel();operation.cleanup_cancel_requested=True
            if task.done():operation.cleanup_failed=True
            return False

    async def cancel(self, scope):
        if not self._scope_valid(scope) or not self._guard(scope,'cancel'):return Result('unavailable')
        operation=self._live.get(scope.operation)
        if operation is None or operation.scope!=scope:return Result('unavailable')
        if await self._cleanup(operation):return operation.result
        return Result('cleanup_pending',scope.operation,operation.envelope.envelope_sha256)

    async def close(self):
        if self._closing:return False
        if self._journal_closed:return True
        self._closing=True;self._closed=True;self._revision+=1
        try:
            joined=True
            deadline=asyncio.get_running_loop().time()+self._wait_limit
            for operation in self._live.values():
                if operation.result.state=='unknown' or operation.invalidated or operation.task is not None:
                    remaining=deadline-asyncio.get_running_loop().time()
                    if remaining<=0:return False
                    if not await self._cleanup(operation,remaining):joined=False
            if not joined:return False
            self._journal.close();self._journal_closed=True
            return True
        finally:self._closing=False
