"""Synthetic PTY lifecycle only; no hardware or LAN listener."""
import os
from pathlib import Path
import struct
import sys
import termios
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import serial.serialposix as backend
from numeric_pairing_serial import NumericPairingSerial
import numeric_pairing_worker as worker

FRAME=struct.pack('>4sB16sI',b'SCNP',1,b'a'*16,91)

class NumericSerialTests(unittest.TestCase):
    def setUp(self):
        self.master,self.slave=os.openpty();self.path=os.ttyname(self.slave)
        attrs=termios.tcgetattr(self.slave);attrs[2]|=termios.HUPCL
        termios.tcsetattr(self.slave,termios.TCSANOW,attrs)
    def tearDown(self):os.close(self.master);os.close(self.slave)
    def test_real_factory_closed_nonblocking_lifecycle_controls_and_one_frame(self):
        port=worker._serial_factory(self.path)
        self.assertIsInstance(port,NumericPairingSerial);self.assertFalse(port.is_open)
        self.assertEqual(port.timeout,0);self.assertEqual(port.write_timeout,2)
        ops=[];original=backend.fcntl.ioctl
        def ioctl(fd,op,*args):
            if op in (backend.TIOCMBIS,backend.TIOCMBIC,backend.TIOCMSET):ops.append(op);return 0
            return original(fd,op,*args)
        with patch.object(backend.fcntl,'ioctl',side_effect=ioctl):
            port.open()
            try:
                self.assertFalse(termios.tcgetattr(port.fd)[2]&termios.HUPCL)
                port.dtr=True;port.rts=True;port.timeout=0
                self.assertEqual(port.read(64),b'')
                os.write(self.master,b'SCPS-synthetic')
                self.assertEqual(port.read(64),b'SCPS-synthetic')
                self.assertEqual(port.write(FRAME),25);self.assertEqual(os.read(self.master,25),FRAME)
                with self.assertRaises(ValueError):port.write(FRAME)
                for action in (lambda:port.send_break(),lambda:setattr(port,'break_condition',True)):
                    with self.assertRaises(ValueError):action()
            finally:port.close()
        self.assertEqual(ops,[]);self.assertFalse(port.is_open)
        self.assertFalse(termios.tcgetattr(self.slave)[2]&termios.HUPCL)
    def test_invalid_output_never_reaches_os(self):
        port=NumericPairingSerial(port=None,timeout=0)
        with patch.object(backend.Serial,'write') as write:
            for payload in (b'x',bytearray(FRAME),b'BAD!'+FRAME[4:],FRAME[:4]+b'\x02'+FRAME[5:],FRAME[:5]+bytes(16)+FRAME[21:],FRAME[:21]+bytes(4)):
                with self.assertRaises(ValueError):port.write(payload)
            write.assert_not_called()
    def test_failed_or_short_write_consumes_once_without_retry(self):
        for outcome in (OSError('synthetic'),24):
            port=NumericPairingSerial(port=None,timeout=0)
            with patch.object(backend.Serial,'write',side_effect=outcome if isinstance(outcome,Exception) else None,return_value=outcome) as write:
                if isinstance(outcome,Exception):
                    with self.assertRaises(OSError):port.write(FRAME)
                else:self.assertEqual(port.write(FRAME),24)
                with self.assertRaises(ValueError):port.write(FRAME)
                write.assert_called_once_with(FRAME)
    def test_configuration_failure_closes_owned_fd(self):
        for setting in ('rtscts','dsrdtr'):
            port=NumericPairingSerial(port=None,**{setting:True});port.port=self.path
            with self.assertRaises(ValueError):port.open()
            self.assertFalse(port.is_open);self.assertIsNone(port.fd)
        port=NumericPairingSerial(port=None);port.port=self.path
        with patch('posix_serial_controls.termios.tcsetattr',side_effect=OSError('synthetic')):
            with self.assertRaises(OSError):port.open()
        self.assertFalse(port.is_open);self.assertIsNone(port.fd)
if __name__=='__main__':unittest.main()
