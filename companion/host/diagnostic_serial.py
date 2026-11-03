"""POSIX diagnostic serial: suppress application modem-line changes.

Import opens nothing. This does not prevent OS/USB-driver changes at os.open;
physical boot continuity must be qualified. No claim of reset-free transport.
Only the read-only diagnostic helper uses this class; pairing is unchanged.
"""
import errno
import os
import select
from posix_serial_controls import ControlPreservingSerial
from serial.serialutil import SerialException,Timeout

class DiagnosticReadError(SerialException):
    def __init__(self,code,phase,number=None):
        super().__init__(code)
        self.diagnostic_code=code;self.diagnostic_phase=phase
        self.errno=number if type(number) is int and 0<number<=4095 else None

class DiagnosticSerial(ControlPreservingSerial):
    def read(self,size=1):
        # Return one available OS chunk immediately. pySerial's accumulating
        # read can discard a complete frame if a following read raises/EOFs.
        if not self.is_open:raise DiagnosticReadError('serial_not_open','read')
        if type(size) is not int or not 0<size<=512:raise ValueError('diagnostic_read_bound')
        if self.timeout is None or not 0<=self.timeout<=.1:raise ValueError('diagnostic_timeout_bound')
        timeout=Timeout(self.timeout)
        while True:
            try:
                ready,_,_=select.select([self.fd,self.pipe_abort_read_r],[],[],timeout.time_left())
            except (OSError,ValueError) as exc:
                # Signal cancellation may use a ValueError subclass; do not
                # relabel it as an OS select failure.
                if not isinstance(exc,OSError) and type(exc) is not ValueError:raise
                if isinstance(exc,OSError) and exc.errno==errno.EINTR:
                    if timeout.expired():return b''
                    continue
                raise DiagnosticReadError('serial_select_failed','select',getattr(exc,'errno',None)) from None
            if self.pipe_abort_read_r in ready:raise DiagnosticReadError('serial_read_cancelled','read')
            if not ready:return b''
            try:chunk=os.read(self.fd,size)
            except OSError as exc:
                if exc.errno in (errno.EAGAIN,errno.EWOULDBLOCK,errno.EINTR):
                    if timeout.expired():return b''
                    continue
                raise DiagnosticReadError('serial_os_read_failed','read',exc.errno) from None
            if not chunk:raise DiagnosticReadError('serial_eof','read')
            return chunk

    def write(self, data):
        raise ValueError('diagnostic_write_forbidden')

    def send_break(self, duration=.25):
        raise ValueError('diagnostic_break_forbidden')

    def _update_break_state(self):
        raise ValueError('diagnostic_break_forbidden')
