"""Shared POSIX application modem-line suppression. Import opens nothing.

OS/USB-driver changes during open remain possible; qualify boot continuity.
This base deliberately leaves protocol read/write behavior to its subclasses.
"""
import termios
from serial.serialposix import Serial

class ControlPreservingSerial(Serial):
    def _update_dtr_state(self):
        # pySerial calls this on open even for a preconfigured False value.
        pass

    def _update_rts_state(self):
        pass

    def _reconfigure_port(self, force_update=False):
        if self._rtscts or self._dsrdtr or self._rs485_mode is not None:
            raise ValueError('serial_flow_control_forbidden')
        super()._reconfigure_port(force_update)
        attrs=termios.tcgetattr(self.fd)
        if attrs[2]&termios.HUPCL:
            attrs[2]&=~termios.HUPCL
            termios.tcsetattr(self.fd,termios.TCSANOW,attrs)
        if termios.tcgetattr(self.fd)[2]&termios.HUPCL:
            raise ValueError('serial_hangup_not_disabled')

