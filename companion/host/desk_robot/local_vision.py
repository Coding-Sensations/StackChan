"""Bounded, opt-in RAM-only pixel statistics; no capture, IO or recognition."""
from dataclasses import dataclass, field
import math
import struct
from threading import RLock
import time

from desk_robot.camera_adapter import LatestFrameAdapter
from desk_robot.session_contract import Binding

_HEADER = struct.Struct('>4sHH')
_MAGIC = b'LV01'
_MAX_WIDTH, _MAX_HEIGHT = 160, 120


def _validate_rgb(width, height, pixels):
    if (type(width) is not int or type(height) is not int
            or not 1 <= width <= _MAX_WIDTH or not 1 <= height <= _MAX_HEIGHT
            or type(pixels) is not bytes or len(pixels) != width * height * 3):
        raise ValueError('bounded RGB8 thumbnail required')


@dataclass(frozen=True)
class RGBThumbnail:
    """Row-major RGB8, exactly three bytes per pixel, at most 57,600 bytes."""
    width: int
    height: int
    pixels: bytes = field(repr=False)

    def __post_init__(self):
        _validate_rgb(self.width, self.height, self.pixels)

    def encode(self):
        _validate_rgb(self.width, self.height, self.pixels)
        return _HEADER.pack(_MAGIC, self.width, self.height) + self.pixels

    @classmethod
    def decode(cls, payload):
        if type(payload) is not bytes or not _HEADER.size <= len(payload) <= 57608:
            raise ValueError('bounded RGB8 envelope required')
        magic, width, height = _HEADER.unpack_from(payload)
        if magic != _MAGIC:
            raise ValueError('unsupported RGB8 schema')
        return cls(width, height, payload[_HEADER.size:])


@dataclass(frozen=True)
class MotionSummary:
    mean_difference: float
    changed_fraction: float
    changed_centroid: tuple[float, float] | None


@dataclass(frozen=True)
class VisualSummary:
    binding: Binding
    sequence: int
    captured_at: float
    width: int
    height: int
    brightness: float
    contrast: float
    bright_centroid: tuple[float, float] | None
    motion: MotionSummary | None


def _statistics(thumbnail):
    width, height = thumbnail.width, thumbnail.height
    # Integer luma is a coarse approximation, not calibrated illumination.
    luma = [(77 * thumbnail.pixels[i] + 150 * thumbnail.pixels[i + 1]
             + 29 * thumbnail.pixels[i + 2]) // 256
            for i in range(0, len(thumbnail.pixels), 3)]
    count = len(luma)
    mean = sum(luma) / count
    contrast = math.sqrt(sum((value - mean) ** 2 for value in luma) / count) / 255
    bright = [i for i, value in enumerate(luma) if value >= 192]
    centroid = None if not bright else (
        round(sum((i % width + .5) / width for i in bright) / len(bright), 3),
        round(sum((i // width + .5) / height for i in bright) / len(bright), 3))
    # At most 300 tile means survive a request, never RGB pixels.
    columns, rows = min(width, 20), min(height, 15)
    grid = []
    for row in range(rows):
        y0, y1 = row * height // rows, (row + 1) * height // rows
        for column in range(columns):
            x0, x1 = column * width // columns, (column + 1) * width // columns
            total = sum(luma[y * width + x] for y in range(y0, y1) for x in range(x0, x1))
            grid.append(total / ((y1 - y0) * (x1 - x0)))
    return round(mean / 255, 3), round(contrast, 3), centroid, tuple(grid), columns, rows


class LocalVision:
    """Serialized explicit requests over the existing owner's latest-frame policy.

    Owner validation must be supplied by the existing conversation owner. This
    object does not grant camera or transport permission or start a session.
    """
    def __init__(self, is_current, *, clock=time.monotonic):
        self._adapter = LatestFrameAdapter(is_current, clock=clock)
        self._current, self._clock = is_current, clock
        self._lock = RLock()
        self._binding = None
        self._history = None
        self._last_time = None

    @property
    def enabled(self):
        with self._lock:
            return self._adapter.enabled

    def _disable(self):
        self._adapter.disable()
        self._binding = self._history = None

    def disable(self):
        with self._lock:
            self._disable()

    def enable(self, binding, *, local_opt_in):
        with self._lock:
            self._disable()
            try:
                self._adapter.enable(binding, local_opt_in=local_opt_in)
                self._binding = binding
            except Exception:
                self._disable()
                raise

    def _guard(self, binding):
        try:
            current = type(binding) is Binding and self._current(binding) is True
            valid = (current and binding == self._binding and self._adapter.enabled)
        except Exception:
            valid = False
        if not valid:
            raise PermissionError('current opted-in camera binding required')
        try:
            now = self._clock()
            valid_time = (type(now) in (int, float) and math.isfinite(now) and now >= 0
                          and (self._last_time is None or now >= self._last_time))
        except Exception:
            valid_time = False
        if not valid_time:
            raise ValueError('valid monotonic time required')
        self._last_time = now
        return now

    def push(self, binding, sequence, captured_at, thumbnail):
        with self._lock:
            try:
                self._guard(binding)
                if type(thumbnail) is not RGBThumbnail:
                    raise ValueError('typed RGB8 thumbnail required')
                self._adapter.push(binding, sequence, captured_at, thumbnail.encode())
            except Exception:
                self._disable()
                raise

    def request(self, binding, request_id, *, visual_request):
        with self._lock:
            try:
                now = self._guard(binding)
                if self._history is not None and now - self._history[0] >= .5:
                    self._history = None
                frame = self._adapter.take_requested(binding, request_id, visual_request=visual_request)
                if frame is None:
                    return None
                thumbnail = RGBThumbnail.decode(frame.payload)
                brightness, contrast, centroid, grid, columns, rows = _statistics(thumbnail)
                motion = None
                if (self._history is not None and self._history[1:3] == (thumbnail.width, thumbnail.height)
                        and frame.captured_at >= self._history[0]):
                    differences = [abs(a - b) for a, b in zip(grid, self._history[3])]
                    changed = [i for i, difference in enumerate(differences) if difference >= 32]
                    moved = None if not changed else (
                        round(sum((i % columns + .5) / columns for i in changed) / len(changed), 3),
                        round(sum((i // columns + .5) / rows for i in changed) / len(changed), 3))
                    motion = MotionSummary(round(sum(differences) / len(grid) / 255, 3),
                                           round(len(changed) / len(grid), 3), moved)
                # Revalidate after CPU work before releasing information or history.
                now = self._guard(binding)
                if now - frame.captured_at >= .5:
                    raise TimeoutError('stale camera frame')
                self._history = (frame.captured_at, thumbnail.width, thumbnail.height, grid)
                return VisualSummary(binding, frame.sequence, frame.captured_at, thumbnail.width,
                                     thumbnail.height, brightness, contrast, centroid, motion)
            except Exception:
                self._disable()
                raise
