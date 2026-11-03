"""Actual pySerial/POSIX lifecycle on synthetic PTYs; never opens robot USB."""
import os
from pathlib import Path
import sys
import termios
import unittest
from unittest.mock import patch
import serial.serialposix as backend
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from diagnostic_serial import DiagnosticSerial

class SerialControlTests(unittest.TestCase):
    def setUp(self):
        self.master,self.slave=os.openpty()
        self.path=os.ttyname(self.slave)
        attrs=termios.tcgetattr(self.slave);attrs[2]|=termios.HUPCL
        termios.tcsetattr(self.slave,termios.TCSANOW,attrs)
    def tearDown(self):
        os.close(self.master);os.close(self.slave)
    def trace_open(self,cls):
        ops=[];original=backend.fcntl.ioctl
        def ioctl(fd,op,*args):
            if op in (backend.TIOCMBIS,backend.TIOCMBIC,backend.TIOCMSET):
                ops.append((op,args));return 0
            return original(fd,op,*args)
        port=cls(port=None,baudrate=115200,timeout=.1,write_timeout=.1)
        port.dtr=False;port.rts=False;port.port=self.path
        with patch.object(backend.fcntl,'ioctl',side_effect=ioctl):
            port.open()
            try:
                self.assertTrue(port.is_open)
                hupcl=bool(termios.tcgetattr(port.fd)[2]&termios.HUPCL)
                # Setters while open must also have no modem-line effect.
                if cls is DiagnosticSerial:port.dtr=True;port.rts=True
            finally:port.close()
        self.assertFalse(port.is_open)
        return ops,hupcl
    def test_original_backend_updates_both_lines_even_when_false(self):
        ops,hupcl=self.trace_open(backend.Serial)
        self.assertEqual([op for op,args in ops],[backend.TIOCMBIC,backend.TIOCMBIC])
        self.assertTrue(hupcl)
    def test_diagnostic_open_setters_close_have_no_modem_updates(self):
        ops,hupcl=self.trace_open(DiagnosticSerial)
        self.assertEqual(ops,[]);self.assertFalse(hupcl)
    def test_read_bytes_and_forbid_output_break(self):
        port=DiagnosticSerial(port=self.path,timeout=.1)
        try:
            os.write(self.master,b'SCFDsynthetic')
            self.assertEqual(port.read(13),b'SCFDsynthetic')
            for action in (lambda:port.write(b'x'),lambda:port.send_break(),lambda:setattr(port,'break_condition',True)):
                with self.assertRaises(ValueError):action()
        finally:port.close()
    def test_actual_capture_factory_uses_control_preserving_backend(self):
        import read_diagnostic_snapshot as helper
        from test_diagnostic_snapshot import frame
        port=helper._serial_factory()
        self.assertIsInstance(port,DiagnosticSerial);self.assertFalse(port.is_open)
        original=port.open
        def opened():
            original()
            os.write(self.master,frame(changes={0:12345}))
        port.open=opened
        value=helper.capture_snapshot(self.path,serial_factory=lambda:port)
        self.assertEqual(value['boot_nonce'],12345);self.assertFalse(port.is_open)
    def test_original_accumulating_read_discards_complete_frame_on_later_eof(self):
        from test_diagnostic_snapshot import frame
        port=backend.Serial(port=self.path,timeout=.1)
        try:
            with patch.object(backend.select,'select',return_value=([port.fd],[],[])),patch.object(backend.os,'read',side_effect=[frame(),b'']) as read:
                with self.assertRaises(backend.SerialException):port.read(512)
                self.assertEqual(read.call_count,2)
        finally:port.close()
    def test_capture_decodes_complete_frame_before_following_eof(self):
        import read_diagnostic_snapshot as helper
        from test_diagnostic_snapshot import frame
        port=helper._serial_factory();original=port.open
        def opened():
            original()
            self.fd=port.fd
        port.open=opened
        def ready(*args):return [self.fd],[],[]
        with patch.object(backend.select,'select',side_effect=ready),patch.object(backend.os,'read',side_effect=[frame(changes={0:777}),b'']) as read:
            value=helper.capture_snapshot(self.path,serial_factory=lambda:port)
        self.assertEqual(value['boot_nonce'],777);self.assertEqual(read.call_count,1);self.assertFalse(port.is_open)
    def test_flow_control_rejected_and_fd_closed(self):
        for setting in ('rtscts','dsrdtr'):
            port=DiagnosticSerial(port=None,**{setting:True});port.port=self.path
            with self.assertRaises(ValueError):port.open()
            self.assertFalse(port.is_open);self.assertIsNone(port.fd)
    def test_hangup_configuration_failure_closes_owned_fd(self):
        port=DiagnosticSerial(port=None);port.port=self.path
        with patch('posix_serial_controls.termios.tcsetattr',side_effect=OSError('synthetic')):
            with self.assertRaises(OSError):port.open()
        self.assertFalse(port.is_open);self.assertIsNone(port.fd)
if __name__=='__main__':unittest.main()
