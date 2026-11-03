"""Finite serialized PCM pump; importing this module never opens audio IO.

Caller owns explicit approved adapter open/begin/arm and finally mute/close.
Input/output device identifiers and system defaults are never changed here.
Run this worker in the main thread of its own process: 40 s pump ceiling, a
50 s process alarm, and a caller-owned 60 s external process supervisor. Native
calls that never return still need that external supervisor.

Adapter contract: push_pcm(epoch, PCM16 bytes)->actual accepted frames <=2048;
pull_pcm(epoch, max_frames)->actual PCM16 bytes (no padded/fabricated samples);
diagnostics()->the documented native callback/frame/drop/underrun/error counts.
Input diagnostics mean 2-B captured return; output means 2-A rendered uplink.
Clean transport/return energy does not establish response or persona identity.
"""
from dataclasses import dataclass
import math
import signal
import struct
import threading
import time

RATE = 48000
MAX_SPEECH_SECONDS = 10
MAX_RUN_SECONDS = 40
PROCESS_ALARM_SECONDS = 50
EXTERNAL_SUPERVISOR_SECONDS = 60
TICK_SECONDS = .01
CHUNK_FRAMES = 480
PULL_FRAMES = 2048
COUNTERS = ('input_callbacks', 'output_callbacks', 'input_frames', 'output_frames',
            'input_dropped', 'output_dropped', 'input_underruns', 'output_underruns', 'output_consumed_frames')
OPTIONAL = ('input_queue_frames', 'output_queue_frames')

@dataclass(frozen=True)
class ProbeResult:
    ok: bool
    reason: str
    requested_frames: int
    accepted_frames: int
    consumed_frames: int
    returned_frames: int
    diagnostics: dict
    return_peak: int
    return_energy: int
    def as_dict(self):
        return dict(ok=self.ok, reason=self.reason, requested_frames=self.requested_frames,
                    accepted_frames=self.accepted_frames, consumed_frames=self.consumed_frames,
                    returned_frames=self.returned_frames,
                    diagnostics=self.diagnostics, return_peak=self.return_peak,
                    return_energy=self.return_energy, identity_verified=False)

class _Abort(Exception):
    pass

def run_probe(adapter, pcm, *, epoch, duration=MAX_RUN_SECONDS, clock=time.monotonic, sleep=time.sleep):
    """Pump both directions on one owner; no lifecycle/credential/network actions.

    Every tick drains before upload and immediately after upload. Each drain is
    bounded to four native pulls, so callbacks cannot monopolize the worker.
    Every short enqueue, drop or native error fails; lost chunks are not retried.
    Successful completion requires actual output consumption to equal accepted
    frames, measured against the initial consumption-counter baseline.
    Each return chunk is measured and discarded; only numeric results are retained.
    """
    requested = accepted = consumed_frames = 0
    initial_consumed = None
    returned_frames = 0
    latest = {}
    previous = {}
    peak = energy = 0
    reason = 'invalid_input'
    old_handler = None
    alarm_installed = False

    def diagnostics():
        nonlocal latest, previous, consumed_frames
        raw = adapter.diagnostics()
        if type(raw) is not dict or any(type(raw.get(key)) is not int or raw[key] < 0 for key in COUNTERS):
            raise _Abort('invalid_diagnostics')
        if type(raw.get('error')) is not int:
            raise _Abort('invalid_diagnostics')
        if any(key in raw and (type(raw[key]) is not int or raw[key] < 0) for key in OPTIONAL):
            raise _Abort('invalid_diagnostics')
        latest = {key: raw[key] for key in (*COUNTERS, 'error', *OPTIONAL) if key in raw}
        if any(raw[key] < previous.get(key, 0) for key in COUNTERS):
            raise _Abort('counter_regressed')
        previous = {key: raw[key] for key in COUNTERS}
        if initial_consumed is not None:
            consumed_frames = raw['output_consumed_frames'] - initial_consumed
        if latest['error']:
            raise _Abort('native_error')
        if latest['input_dropped'] or latest['output_dropped']:
            raise _Abort('audio_dropped')

    def drain():
        nonlocal peak, energy, returned_frames
        for _ in range(4):
            data = adapter.pull_pcm(epoch, PULL_FRAMES)
            if type(data) is not bytes or len(data) % 2 or len(data) > PULL_FRAMES * 2:
                raise _Abort('invalid_return')
            if not data:
                return
            if returned_frames + len(data) // 2 > RATE * MAX_RUN_SECONDS:
                raise _Abort('return_limit')
            returned_frames += len(data) // 2
            for (sample,) in struct.iter_unpack('<h', data):
                peak = max(peak, abs(sample)); energy += sample * sample

    try:
        if (type(pcm) is not bytes or not pcm or len(pcm) % 2 or len(pcm) > RATE * MAX_SPEECH_SECONDS * 2 or
            type(epoch) is not int or not 1 <= epoch <= 0xffffffff or
            type(duration) not in (int, float) or not math.isfinite(duration) or not 0 < duration <= MAX_RUN_SECONDS):
            raise _Abort('invalid_input')
        if threading.current_thread() is not threading.main_thread():
            raise _Abort('main_process_worker_required')
        if signal.getitimer(signal.ITIMER_REAL) != (0., 0.):
            raise _Abort('existing_process_alarm')
        old_handler = signal.getsignal(signal.SIGALRM)
        def expired(_signal, _frame):
            raise _Abort('process_alarm')
        signal.signal(signal.SIGALRM, expired)
        signal.alarm(PROCESS_ALARM_SECONDS)
        alarm_installed = True
        diagnostics()
        initial_consumed = latest['output_consumed_frames']
        start = clock()
        if not math.isfinite(start):
            raise _Abort('invalid_clock')
        deadline = start + duration
        position = 0
        # Independent iteration ceiling protects against a frozen injected clock.
        for _ in range(int(MAX_RUN_SECONDS / TICK_SECONDS) + 2):
            now = clock()
            if not math.isfinite(now) or now < start:
                raise _Abort('invalid_clock')
            if now >= deadline:
                break
            drain()
            diagnostics()
            if position < len(pcm) and now - start + 1e-9 >= position / 2 / RATE:
                chunk = pcm[position:position + CHUNK_FRAMES * 2]
                wanted = len(chunk) // 2
                requested += wanted
                count = adapter.push_pcm(epoch, chunk)
                if type(count) is not int or not 0 <= count <= wanted:
                    raise _Abort('invalid_enqueue_count')
                accepted += count
                drain()
                diagnostics()
                if count != wanted:
                    raise _Abort('short_enqueue')
                position += len(chunk)
            diagnostics()
            remaining = deadline - clock()
            if remaining > 0:
                sleep(min(TICK_SECONDS, remaining))
        else:
            raise _Abort('clock_stalled')
        drain()
        diagnostics()
        if position != len(pcm):
            raise _Abort('uplink_incomplete')
        if latest.get('output_queue_frames', 0):
            raise _Abort('pending_uplink')
        if consumed_frames != accepted:
            raise _Abort('uplink_consumption_mismatch')
        # Capture remains active until the caller mutes/closes. New samples can
        # arrive between the final pull and diagnostic snapshot, so occupancy
        # is reported rather than confused with proven dropped audio.
        if not latest['input_callbacks'] or not latest['output_callbacks']:
            raise _Abort('no_callback_activity')
        reason = 'transport_complete'
    except _Abort as error:
        reason = str(error)  # Only fixed local reason codes, never native exception messages.
    except Exception:
        reason = 'adapter_or_clock_error'
    finally:
        # Retain native counters after a push/pull exception; do not echo its
        # message or erase evidence by claiming an empty final snapshot.
        if latest and reason not in ('process_alarm', 'transport_complete'):
            try:
                diagnostics()
            except Exception:
                pass
        if alarm_installed:
            signal.alarm(0)
            signal.signal(signal.SIGALRM, old_handler)
    return ProbeResult(reason == 'transport_complete', reason, requested, accepted, consumed_frames, returned_frames,
                       latest, peak, energy)
