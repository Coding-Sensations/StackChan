"""Explicitly started, latest-frame delivery with retained local cleanup ownership.

No camera, listener or CMIO extension is constructed here. Local joins are not
physical Stop evidence. The caller supplies an admitted producer and the existing
authority/clock-checking BoundedWebcamWorkerEmitter for this exact purpose.
"""
import asyncio
import os
from typing import Protocol
from webcam_worker_emitter import BoundedWebcamWorkerEmitter


class ProducerOwner(Protocol):
    async def stop_join(self):
        """Revoke ingress and return only after producer callbacks have joined.

        Raise on uncertain closure. Repeat calls must join the same owner, never
        start/reacquire it. Device/DMA qualification is a separate contract.
        """


class DeliverySink(Protocol):
    async def write(self, line: bytes, *, validate):
        """Write one bounded event; return only after consumption by this sink.

        Call validate() before each write, including after every IO wait.
        This does not attest CMIO consumption or physical camera state.
        """

    async def close_join(self):
        """Return only after this sink's output resources and callbacks close."""


class NonblockingPipeSink:
    """Adopt an exclusive, already nonblocking FD; never use shared stdout.

    Construction does no IO. The caller transfers FD ownership. Closing may
    truncate an in-flight line; the exclusive stream is then closed, never reused
    for a new session. A receiving decoder must reject an incomplete final line.
    """
    def __init__(self, fd):
        if type(fd) is not int or fd < 0 or os.get_blocking(fd):
            raise ValueError('exclusive nonblocking descriptor required')
        self._fd = fd
        self._writing = False
        self._close_failure = None

    async def write(self, line, *, validate):
        if type(line) is not bytes or not 1 <= len(line) <= 8192 or not line.endswith(b'\n'):
            raise ValueError('bounded complete worker line required')
        if self._fd is None or self._writing:
            raise RuntimeError('closed or concurrent sink')
        self._writing = True
        loop = asyncio.get_running_loop()
        fd = self._fd
        view = memoryview(line)
        try:
            while view:
                validate()
                try:
                    count = os.write(fd, view)
                    if count <= 0:
                        raise OSError('pipe made no progress')
                    view = view[count:]
                except BlockingIOError:
                    ready = loop.create_future()
                    def writable():
                        if not ready.done():
                            ready.set_result(None)
                    loop.add_writer(fd, writable)
                    try:
                        await ready
                    finally:
                        loop.remove_writer(fd)
        finally:
            self._writing = False

    async def close_join(self):
        if self._close_failure is not None:
            raise RuntimeError('uncertain descriptor closure requires reconciliation') from self._close_failure
        if self._writing:
            raise RuntimeError('writer must join before sink closure')
        if self._fd is not None:
            fd = self._fd
            # No await between close and retiring ownership. A failed close
            # retains the descriptor; callers must reconcile rather than retry
            # a possibly reused descriptor after an uncertain OS close error.
            try:
                os.close(fd)
            except OSError as failure:
                self._close_failure = failure
                self._fd = None
                raise
            self._fd = None


class JoinedWebcamDelivery:
    """Serialized async owner: one frame in emitter, one in-flight event.

    Priority owner calls wake() when controls/audio drain; blocked video never
    polls. Hard bursts yield after eight events. All public calls use one loop.
    No object restart is possible after close, failure or authority revocation.
    """
    def __init__(self, emitter, producer: ProducerOwner, sink: DeliverySink, *, media_ready):
        if type(emitter) is not BoundedWebcamWorkerEmitter or not all(callable(f) for f in
                (getattr(producer, 'stop_join', None), getattr(sink, 'write', None),
                 getattr(sink, 'close_join', None), media_ready)):
            raise ValueError('existing typed emitter and explicit ownership contracts required')
        self.emitter, self.producer, self.sink = emitter, producer, sink
        self.media_ready = media_ready
        self._loop = None
        self._task = self._cleanup = None
        self._event = asyncio.Event()
        self._accepting = False
        self._retired = False
        self._producer_joined = self._sink_joined = False
        self._receipt = None
        self.failure = None

    def _serialized(self):
        if asyncio.get_running_loop() is not self._loop:
            raise RuntimeError('exact delivery owner loop required')

    def start(self):
        if self._loop is not None or self._retired:
            raise RuntimeError('one explicit start per owner')
        self._loop = asyncio.get_running_loop()
        self._accepting = True
        self._task = self._loop.create_task(self._run())

    def wake(self):
        self._serialized()
        if self._accepting:
            self._event.set()

    def offer(self, frame):
        self._serialized()
        if not self._accepting:
            raise PermissionError('delivery ingress revoked')
        try:
            self.emitter.offer(frame)
        except BaseException:
            self._accepting = False
            self._retired = True
            self.emitter.revoke()
            if self._task is not None:
                self._task.cancel()
            self._begin_cleanup()
            raise
        self.wake()

    async def _run(self):
        try:
            while self._accepting:
                await self._event.wait()
                self._event.clear()
                for _ in range(8):
                    if not self._accepting or self.media_ready() is not True:
                        break
                    event = self.emitter.next_event(controls_drained=True, audio_drained=True)
                    if event is None:
                        break
                    await self.sink.write(event + b'\n', validate=self.emitter.delivery_guard())
                else:
                    self._event.set()
                await asyncio.sleep(0)
        except asyncio.CancelledError:
            raise
        except Exception as failure:
            self.failure = failure
            self._accepting = False
            self._retired = True
            self.emitter.revoke()
            # Join after this task returns; _join_owned waits for this task.
            self._begin_cleanup()

    def _begin_cleanup(self):
        if self._loop is not None and self._cleanup is None:
            self._cleanup = self._loop.create_task(self._join_owned())

    async def close_join(self):
        if self._loop is None:
            self._loop = asyncio.get_running_loop()
        self._serialized()
        # Revoke synchronously, before a caller cancellation or producer await.
        self._accepting = False
        self._retired = True
        self.emitter.revoke()
        if self._cleanup is None or (self._cleanup.done() and self._cleanup.exception() is not None):
            self._cleanup = self._loop.create_task(self._join_owned())
        return await asyncio.shield(self._cleanup)

    async def _join_owned(self):
        if self._task is not None:
            self._task.cancel()
            try:
                await self._task
            except asyncio.CancelledError:
                pass
        errors = []
        if not self._producer_joined:
            try:
                await self.producer.stop_join()
                self._producer_joined = True
            except Exception as failure:
                errors.append(failure)
        if not self._sink_joined:
            try:
                await self.sink.close_join()
                self._sink_joined = True
            except Exception as failure:
                errors.append(failure)
        if errors:
            raise ExceptionGroup('local delivery cleanup unconfirmed', errors)
        if self._receipt is None:
            self._receipt = object()
        return self._receipt

    def owns_join_receipt(self, receipt):
        """Local ownership closure only; cannot release physical Stop custody."""
        return receipt is not None and receipt is self._receipt and self._producer_joined and self._sink_joined
