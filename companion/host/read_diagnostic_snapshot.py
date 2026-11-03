#!/usr/bin/env python3
"""Capture one explicitly exported, public SCFD frame; inert until invoked.

On StackChan, keep Companion muted/stopped and press Export diagnostics after
starting this helper. It never sends commands, pairing data or credentials.
Opening USB may still reset a board through the OS driver: this helper does not attest physical silence or
that a prior reboot record survived. No listener or audio device is opened.
"""
import argparse
import json
import math
from pathlib import Path
import time
from streaming.diagnostic_snapshot import MAGIC,FRAME_SIZE,SnapshotError,decode_snapshot

class CaptureError(ValueError):
    def __init__(self,code,*,phase=None,error_kind=None,number=None):
        super().__init__(code)
        self.public=dict(code=code,phase=phase,error_kind=error_kind,errno=number)

def _serial_factory():
    from diagnostic_serial import DiagnosticSerial
    return DiagnosticSerial(port=None,baudrate=115200,timeout=.1,write_timeout=.1)

def _public_error(exc,phase):
    # Retain only fixed categories/numeric errno, never args/str/trace/console.
    try:
        from diagnostic_serial import DiagnosticReadError
    except ImportError:DiagnosticReadError=()
    if isinstance(exc,DiagnosticReadError):
        return CaptureError(exc.diagnostic_code,phase=exc.diagnostic_phase,error_kind='diagnostic_serial',number=exc.errno)
    kind='os' if isinstance(exc,OSError) else 'internal'
    number=getattr(exc,'errno',None)
    if type(number) is not int or not 0<number<=4095:number=None
    return CaptureError('serial_'+phase+'_failed',phase=phase,error_kind=kind,number=number)

def capture_snapshot(port_name,*,serial_factory=None,clock=time.monotonic,timeout_seconds=30,event_callback=None):
    if type(port_name) is not str or not 1<=len(port_name)<=256 or any(ord(c)<32 for c in port_name):raise CaptureError('invalid_capture')
    if type(timeout_seconds) not in (int,float) or not math.isfinite(timeout_seconds) or not 0<timeout_seconds<=300:raise CaptureError('invalid_capture')
    started=clock()
    if type(started) not in (int,float) or not math.isfinite(started):raise CaptureError('invalid_capture')
    try:port=(serial_factory or _serial_factory)()
    except Exception as exc:raise _public_error(exc,'create') from None
    if getattr(port,'is_open',True) is not False:raise CaptureError('serial_must_be_closed')
    buffer=bytearray();scanned=0;deadline=started+timeout_seconds
    phase='open';failure=None;captured=False;last_event=started
    def publish(state,**extra):
        if event_callback:event_callback(dict(state=state,scanned_bytes=scanned,remaining_seconds=max(0.,deadline-clock()),**extra))
    try:
        port.dtr=False;port.rts=False;port.port=port_name;port.timeout=min(.1,timeout_seconds)
        port.open()
        if getattr(port,'is_open',False) is not True:raise CaptureError('serial_open_unconfirmed',phase='open')
        phase='status';publish('listening',reader_closed=False);phase='read'
        while True:
            now=clock()
            if now>=deadline:raise CaptureError('capture_expired',phase='read')
            if scanned>=8192:raise CaptureError('scan_limit',phase='read')
            if now-last_event>=.5:
                phase='status';publish('listening',reader_closed=False);phase='read';last_event=now
            next_timeout=min(.1,deadline-now)
            if port.timeout!=next_timeout:
                phase='configure';port.timeout=next_timeout;phase='read'
            requested=min(512,8192-scanned)
            chunk=port.read(requested)
            if clock()>=deadline:raise CaptureError('capture_expired',phase='read')
            if type(chunk) is not bytes or len(chunk)>requested:raise CaptureError('invalid_serial_read',phase='read')
            scanned+=len(chunk);buffer.extend(chunk)
            while True:
                offset=buffer.find(MAGIC)
                if offset<0:
                    if len(buffer)>3:del buffer[:-3]
                    break
                if offset:del buffer[:offset]
                if len(buffer)<FRAME_SIZE:break
                candidate=bytes(buffer[:FRAME_SIZE])
                try:
                    value=decode_snapshot(candidate);captured=True;return value
                except SnapshotError:del buffer[0]
    except CaptureError as exc:failure=exc;raise
    except KeyboardInterrupt:
        failure=CaptureError('capture_cancelled',phase='signal');raise failure from None
    except Exception as exc:
        failure=_public_error(exc,phase);raise failure from None
    finally:
        buffer.clear();closed=False;close_failure=None
        try:
            port.close();closed=getattr(port,'is_open',True) is False
            if not closed:raise CaptureError('serial_close_failed',phase='close')
        except Exception as exc:
            close_failure=exc if isinstance(exc,CaptureError) else _public_error(exc,'close')
        try:publish('closed',reader_closed=closed,captured=captured,failure=failure.public if failure else None,primary_failure=failure.public if failure else None,cleanup_failure=close_failure.public if close_failure else None)
        finally:
            if close_failure:raise close_failure from None

def save_snapshot(path,value):
    """Save the validated parser result, never console bytes; preserve evidence."""
    payload=json.dumps(value,sort_keys=True,indent=2,allow_nan=False)+'\n'
    with Path(path).open('x',encoding='utf-8') as output:output.write(payload)

def main(argv=None):
    import signal
    from diagnostic_reader_status import ReaderStatus,check_status,watch_status
    parser=argparse.ArgumentParser(description='One stopped-only public diagnostic export. No USB writes/pairing/audio/reopen.')
    parser.add_argument('--port')
    parser.add_argument('--output',type=Path)
    parser.add_argument('--status-file',type=Path)
    parser.add_argument('--timeout-seconds',type=float,default=30)
    parser.add_argument('--check-status',type=Path)
    parser.add_argument('--watch-status',type=Path)
    parser.add_argument('--capture-id')
    args=parser.parse_args(argv)
    if args.check_status or args.watch_status:
        if args.port or args.output or args.status_file or (args.check_status and args.watch_status):parser.error('status observation cannot open a port')
        if args.watch_status:return watch_status(args.watch_status,args.capture_id,args.timeout_seconds)
        value=check_status(args.check_status,args.capture_id);print(json.dumps(value),flush=True);return 0 if value['listening'] else 1
    if not args.port or not args.output:parser.error('--port and --output are required for capture')
    if not math.isfinite(args.timeout_seconds) or not 0<args.timeout_seconds<=300:parser.error('timeout must be within (0,300] seconds')
    if args.output.exists():parser.error('snapshot output already exists')
    status=None;old_handlers={}
    try:
        status=ReaderStatus(args.status_file)
        def cancelled(signum,frame):raise CaptureError('capture_cancelled',phase='signal')
        for signum in (signal.SIGTERM,signal.SIGINT):old_handlers[signum]=signal.signal(signum,cancelled)
        value=capture_snapshot(args.port,timeout_seconds=args.timeout_seconds,event_callback=status.emit)
        save_snapshot(args.output,value)
        status.emit(dict(state='completed',reader_closed=True,snapshot_saved=True))
        return 0
    except Exception as exc:
        error=exc.public if isinstance(exc,CaptureError) else {**_public_error(exc,'runner').public,'code':'capture_runner_failed'}
        if status:
            try:status.emit(dict(state='failed',failure=error,snapshot_saved=False))
            except (OSError,ValueError):pass
        print(json.dumps(dict(state='failed',failure=error,snapshot_saved=False)),flush=True)
        return 1
    finally:
        for signum,handler in old_handlers.items():signal.signal(signum,handler)
if __name__=='__main__':raise SystemExit(main())
