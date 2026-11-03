"""Human-operated volatile USB pairing. Import/help do no serial or network I/O.

Open/stabilize precedes physical Pair. A receipt proves local staging only;
Connect remains a separate physical action. No tokens in arguments/files/logs.
"""
import public_config as config
import argparse
import getpass
import signal
import sys
import threading
import time
import uuid
import warnings
from contextlib import contextmanager
from pathlib import Path

from provisioning import MAX_FRAME, pack_grant
from pairing_receipt import READY, STAGED, REJECTED, EXPIRED, decode_receipt

ENDPOINT = config.URI
DEVICE = config.DEVICE
SESSION_SECONDS = 180
INPUT_LIMIT = 4096

class PairingFailure(Exception):
    """Only fixed nonsecret status labels may be raised here."""

class _Parser(argparse.ArgumentParser):
    def error(self, message):
        raise PairingFailure('invalid_arguments')

def _pairing_paused():
    source = Path(__file__).resolve()
    # Repository checkout lives below StackChan/src/upstream. Check both the
    # project control marker and a marker alongside the repository itself.
    return any((source.parents[index] / '.tools' / 'TRANSPORT-PAIRING-PAUSED').exists()
               for index in (3, 5))

@contextmanager
def _session_deadline():
    if threading.current_thread() is not threading.main_thread() or signal.getitimer(signal.ITIMER_REAL) != (0.0, 0.0):
        raise PairingFailure('deadline_unavailable')
    previous = signal.getsignal(signal.SIGALRM)
    def expired(signum, frame):
        raise PairingFailure('session_expired')
    signal.signal(signal.SIGALRM, expired)
    signal.setitimer(signal.ITIMER_REAL, SESSION_SECONDS)
    try:
        yield
    finally:
        signal.setitimer(signal.ITIMER_REAL, 0)
        signal.signal(signal.SIGALRM, previous)

def _read(connection):
    try:
        data = connection.read(64)
    except PairingFailure:
        raise
    except Exception:
        raise PairingFailure('transport_lost') from None
    if type(data) is not bytes or len(data) > 64:
        raise PairingFailure('invalid_transport')
    return data

def _stabilize(connection):
    deadline = time.monotonic() + 2
    total = 0
    while time.monotonic() < deadline:
        total += len(_read(connection))
        if total > INPUT_LIMIT:
            raise PairingFailure('input_limit')
        time.sleep(.02)

class _ReceiptReader:
    def __init__(self, connection):
        self.connection = connection
        self.buffer = b''

    def receive(self, missing):
        deadline = time.monotonic() + 5
        total = 0
        while time.monotonic() < deadline:
            start = self.buffer.find(b'SCPS')
            if start < 0:
                self.buffer = self.buffer[-3:]  # Partial magic only; discard console text.
            else:
                self.buffer = self.buffer[start:]
                if len(self.buffer) >= 22:
                    frame, self.buffer = self.buffer[:22], self.buffer[22:]
                    try:
                        return decode_receipt(frame)
                    except ValueError:
                        raise PairingFailure('invalid_receipt') from None
            chunk = _read(self.connection)
            total += len(chunk)
            if total > INPUT_LIMIT:
                raise PairingFailure('input_limit')
            self.buffer += chunk
            time.sleep(.02)
        raise PairingFailure(missing)

def _rejection(receipt):
    if receipt.status == REJECTED:
        raise PairingFailure('rejected')
    if receipt.status == EXPIRED:
        raise PairingFailure('expired')

def main(argv=None):
    token = frame = None
    connection = None
    try:
        parser = _Parser(description='Interactive USB pairing; never pass a token in arguments.')
        parser.add_argument('--port', required=True, help='Verified robot port; no discovery or automatic reopen.')
        parser.add_argument('--open-approved-usb', action='store_true', help='Already authorized opener: skip OPEN only; STABLE/token/PAIR remain human controlled.')
        try:
            args = parser.parse_args(argv)
        except SystemExit as request:
            return int(request.code)
        if _pairing_paused():
            raise PairingFailure('paused')
        if not all(stream.isatty() for stream in (sys.stdin, sys.stdout, sys.stderr)):
            raise PairingFailure('interactive_terminal_required')
        if not args.port or len(args.port) > 256 or any(not 33 <= ord(c) <= 126 for c in args.port):
            raise PairingFailure('invalid_port')
        config.require()
        print(f'Pinned device: {DEVICE}\nPinned endpoint: {ENDPOINT}')
        print('Opening or closing native USB MAY reset the robot despite DTR/RTS=False. The cause of a reboot cannot be inferred from a write.')
        print('Do NOT tap Pair trusted host yet. Open once first; no token or frame is sent during stabilization.')
        print('After opening is authorized, the session is limited to 180 seconds. Keep this terminal open through your test and physically Mute before closing.')
        if not args.open_approved_usb:
            if input('Type OPEN to authorize opening, or Enter to cancel: ').strip().casefold() != 'open':
                raise PairingFailure('cancelled')
        with _session_deadline():
            import serial
            connection = serial.Serial(port=None, baudrate=115200, timeout=0, write_timeout=2)
            connection.dtr = False
            connection.rts = False
            connection.port = args.port
            try:
                connection.open()
            except PairingFailure:
                raise
            except Exception:
                raise PairingFailure('transport_lost') from None
            _stabilize(connection)
            print('Check the robot is stable. If it restarted, reopen Companion on its screen before continuing; this program never reopens USB.')
            if input('Type STABLE after verifying the screen, or Enter to cancel: ').strip().casefold() != 'stable':
                raise PairingFailure('cancelled')
            print('Enter the current token from the protected local sheet or transport-test terminal; never paste it into chat.')
            with warnings.catch_warnings():
                warnings.simplefilter('error', getpass.GetPassWarning)
                token = getpass.getpass('Token (masked): ')
            request = uuid.uuid4().hex
            frame = pack_grant(ENDPOINT, DEVICE, token, request)
            if len(frame) > MAX_FRAME:
                raise PairingFailure('frame_limit')
            print('Now physically tap Pair trusted host on the robot (60-second window).')
            if input('Type PAIR to authorize one write after Ready, or Enter to cancel: ').strip().casefold() != 'pair':
                raise PairingFailure('cancelled')
            receipts = _ReceiptReader(connection)
            ready = receipts.receive('ready_missing_legacy_or_unsupported')
            _rejection(ready)
            if ready.status != READY:
                raise PairingFailure('unexpected_receipt')
            print(f'Ready received: boot nonce {ready.boot_nonce}, reset reason code {ready.reset_reason}. This does not establish the cause of the earlier reboot.')
            intended_length = len(frame)
            try:
                written = connection.write(frame)
            except PairingFailure:
                raise
            except Exception:
                raise PairingFailure('transport_lost') from None
            token = frame = None
            if written != intended_length:
                raise PairingFailure('incomplete_write')
            staged = receipts.receive('receipt_missing')
            _rejection(staged)
            if staged.status == READY:
                raise PairingFailure('reset_detected' if staged.boot_nonce != ready.boot_nonce else 'window_changed')
            if staged.status != STAGED:
                raise PairingFailure('unexpected_receipt')
            if staged.request != request:
                raise PairingFailure('request_mismatch')
            print('Grant staged receipt confirmed. This is not gateway authentication or audio connection. No NVS settings were saved.')
            print('Keep this port open. Use Connect separately on the robot only when ready; after your test physically tap Mute.')
            while input('Type MUTED only after physically tapping Mute; the port stays open until then: ').strip().casefold() != 'muted':
                print('Port remains open. Physically tap Mute before confirming MUTED.')
            print('Closing USB after your Mute confirmation. Closing may reset the robot.')
        return 0
    except PairingFailure as failure:
        print(f'Pairing stopped: {failure}. No automatic retry. If a frame was sent, physically Mute before retrying.', file=sys.stderr)
        if str(failure) == 'transport_lost':
            print('USB transport was lost; reset or re-enumeration is possible, CPU reset is not proven.', file=sys.stderr)
        return 3
    except (Exception, KeyboardInterrupt):
        print('Pairing stopped: input_or_setup_failed. No automatic retry. Physically Mute before retrying.', file=sys.stderr)
        return 3
    finally:
        if connection is not None:
            try:
                connection.close()
            except Exception:
                pass
        token = frame = None  # Immutable Python values cannot promise secure memory erasure.

if __name__ == '__main__':
    raise SystemExit(main())
