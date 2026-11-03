"""Numeric pairing POSIX port: control-preserving, one bounded SCNP write.

Import/closed construction is inert. Preserve pySerial nonblocking read behavior;
only the numeric worker owns this port. Diagnostic read/write policy is separate.
"""
from posix_serial_controls import ControlPreservingSerial

class NumericPairingSerial(ControlPreservingSerial):
    def __init__(self,*args,**kwargs):
        self._numeric_write_attempted=False
        super().__init__(*args,**kwargs)

    def write(self,data):
        if (type(data) is not bytes or len(data)!=25 or data[:5]!=b'SCNP\x01'
            or data[5:21]==bytes(16) or data[21:25]==bytes(4)):
            raise ValueError('numeric_write_bound')
        if self._numeric_write_attempted:raise ValueError('numeric_write_consumed')
        # Consume before the OS write; failure/partial delivery cannot retry.
        self._numeric_write_attempted=True
        return super().write(data)

    def send_break(self,duration=.25):
        raise ValueError('numeric_break_forbidden')

    def _update_break_state(self):
        raise ValueError('numeric_break_forbidden')
