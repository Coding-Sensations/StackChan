"""Offline async ownership, mocked I/O and anonymous-pipe tests only."""
import asyncio
import base64
import json
import os
import unittest
from unittest.mock import patch
from joined_webcam_delivery import JoinedWebcamDelivery, NonblockingPipeSink
from webcam_worker_emitter import BoundedWebcamWorkerEmitter
from webcam_transport import WebcamPurpose, VideoFrame, MappedCapture, FRAME_BYTES, HEADER


class Producer:
    def __init__(self):
        self.calls = 0
        self.gate = asyncio.Event()
        self.gate.set()
        self.fail = False

    async def stop_join(self):
        self.calls += 1
        await self.gate.wait()
        if self.fail:
            raise RuntimeError('synthetic producer join failed')


class Sink:
    def __init__(self):
        self.lines = []
        self.closed = False
        self.block = None
        self.entered = asyncio.Event()
        self.fail = False

    async def write(self, line, *, validate):
        self.entered.set()
        if self.block is not None:
            await self.block.wait()
        validate()
        if self.fail:
            raise OSError('synthetic sink failure')
        self.lines.append(line)

    async def close_join(self):
        self.closed = True


class Tests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.p = WebcamPurpose('device', 'conversation', 'turn', 1, 1)
        self.current = True
        self.ready = True
        self.producer, self.sink = Producer(), Sink()
        self.emitter = BoundedWebcamWorkerEmitter(self.p, current=lambda p:self.current,
            map_capture_clock=lambda p,ns:MappedCapture(10., 0.), clock=lambda:10.01)
        self.owner = JoinedWebcamDelivery(self.emitter, self.producer, self.sink,
            media_ready=lambda:self.ready)

    async def asyncTearDown(self):
        self.producer.fail = False
        self.producer.gate.set()
        await self.owner.close_join()

    def frame(self, seq=1):
        return VideoFrame(self.p, seq, 10., b'\x00'*FRAME_BYTES, 1234)

    async def ticks(self, n=30):
        for _ in range(n):
            await asyncio.sleep(0)

    async def test_construction_inert_and_latest_frame_priority(self):
        self.assertIsNone(self.owner._task)
        self.assertEqual(self.producer.calls, 0)
        self.ready = False
        self.owner.start()
        self.owner.offer(self.frame(1))
        self.owner.offer(self.frame(2))
        await self.ticks()
        self.assertEqual(self.sink.lines, [])
        self.ready = True
        self.owner.wake()
        await self.ticks()
        self.assertGreater(len(self.sink.lines), 70)
        headers = [HEADER.unpack_from(base64.b64decode(json.loads(line)['wire'])) for line in self.sink.lines]
        self.assertTrue(all(h[7] == 2 and h[8] == 1234 for h in headers))

    async def test_priority_rechecked_between_every_event(self):
        async def write(line, *, validate):
            validate()
            self.sink.lines.append(line)
            self.ready = False
        self.sink.write = write
        self.owner.start()
        self.owner.offer(self.frame())
        await self.ticks()
        self.assertEqual(len(self.sink.lines), 1)

    async def test_authority_loss_between_events_retires_owner(self):
        async def write(line, *, validate):
            validate()
            self.sink.lines.append(line)
            self.current = False
        self.sink.write = write
        self.owner.start()
        self.owner.offer(self.frame())
        await self.ticks()
        self.assertEqual(len(self.sink.lines), 1)
        self.assertIsInstance(self.owner.failure, PermissionError)
        await self.owner._cleanup
        self.assertTrue(self.sink.closed)
        with self.assertRaises(PermissionError):
            self.owner.offer(self.frame(2))

    async def test_close_cancels_blocked_write_and_joins_producer(self):
        self.sink.block = asyncio.Event()
        self.producer.gate.clear()
        self.owner.start()
        self.owner.offer(self.frame())
        await self.sink.entered.wait()
        close = asyncio.create_task(self.owner.close_join())
        await self.ticks()
        self.assertFalse(close.done())
        self.assertFalse(self.owner.owns_join_receipt(object()))
        with self.assertRaises(PermissionError):
            self.owner.offer(self.frame(2))
        self.producer.gate.set()
        receipt = await close
        self.assertTrue(self.sink.closed)
        self.assertTrue(self.owner._task.done())
        self.assertTrue(self.owner.owns_join_receipt(receipt))
        self.assertIs(receipt, await self.owner.close_join())
        self.assertEqual(self.producer.calls, 1)
        with self.assertRaises(RuntimeError):
            self.owner.start()

    async def test_authority_revoked_during_backpressure_writes_no_bytes(self):
        self.sink.block = asyncio.Event()
        self.owner.start()
        self.owner.offer(self.frame())
        await self.sink.entered.wait()
        self.current = False
        self.sink.block.set()
        await self.ticks()
        self.assertEqual(self.sink.lines, [])
        self.assertIsInstance(self.owner.failure, PermissionError)
        await self.owner._cleanup
        self.assertTrue(self.sink.closed)

    async def test_frame_replacement_during_backpressure_retires_old_event(self):
        self.sink.block = asyncio.Event()
        self.owner.start()
        self.owner.offer(self.frame())
        await self.sink.entered.wait()
        self.owner.offer(self.frame(2))
        self.sink.block.set()
        await self.ticks()
        self.assertEqual(self.sink.lines, [])
        self.assertIsInstance(self.owner.failure, PermissionError)
        await self.owner._cleanup
        self.assertTrue(self.sink.closed)

    async def test_failed_join_retains_custody_then_retries_same_owner(self):
        self.producer.fail = True
        self.owner.start()
        with self.assertRaises(ExceptionGroup):
            await self.owner.close_join()
        self.assertTrue(self.sink.closed)
        self.assertIsNone(self.owner._receipt)
        self.producer.fail = False
        receipt = await self.owner.close_join()
        self.assertTrue(self.owner.owns_join_receipt(receipt))
        self.assertEqual(self.producer.calls, 2)

    async def test_cancelling_close_caller_does_not_cancel_cleanup(self):
        self.producer.gate.clear()
        self.owner.start()
        close = asyncio.create_task(self.owner.close_join())
        await self.ticks()
        close.cancel()
        with self.assertRaises(asyncio.CancelledError):
            await close
        self.assertFalse(self.owner._cleanup.done())
        self.producer.gate.set()
        receipt = await self.owner.close_join()
        self.assertTrue(self.owner.owns_join_receipt(receipt))
        self.assertEqual(self.producer.calls, 1)

    async def test_sink_failure_still_allows_local_join_not_physical_proof(self):
        self.sink.fail = True
        self.owner.start()
        self.owner.offer(self.frame())
        await self.ticks()
        self.assertIsInstance(self.owner.failure, OSError)
        receipt = await self.owner.close_join()
        self.assertTrue(self.owner.owns_join_receipt(receipt))
        self.assertFalse(hasattr(receipt, 'physical_off'))


class PipeTests(unittest.IsolatedAsyncioTestCase):
    async def test_partial_os_write_rechecks_before_remaining_bytes(self):
        r, w = os.pipe()
        os.set_blocking(w, False)
        sink = NonblockingPipeSink(w)
        valid = [True]
        real_write = os.write
        def partial(fd, data):
            count = real_write(fd, data[:2])
            valid[0] = False
            return count
        def validate():
            if not valid[0]:
                raise PermissionError('synthetic revocation')
        try:
            with patch('joined_webcam_delivery.os.write', side_effect=partial):
                with self.assertRaises(PermissionError):
                    await sink.write(b'fixture\n', validate=validate)
            await sink.close_join()
            self.assertEqual(os.read(r, 4096), b'fi')
            self.assertEqual(os.read(r, 1), b'')
        finally:
            await sink.close_join()
            os.close(r)

    async def test_real_pipe_bytes_and_eof(self):
        r, w = os.pipe()
        os.set_blocking(w, False)
        sink = NonblockingPipeSink(w)
        try:
            await sink.write(b'{"type":"fixture"}\n', validate=lambda:None)
            self.assertEqual(os.read(r, 4096), b'{"type":"fixture"}\n')
            await sink.close_join()
            self.assertEqual(os.read(r, 1), b'')
        finally:
            await sink.close_join()
            os.close(r)

    async def test_backpressure_cancellation_removes_writer_before_close(self):
        r, w = os.pipe()
        os.set_blocking(w, False)
        sink = NonblockingPipeSink(w)
        try:
            while True:
                try:os.write(w, b'x'*4096)
                except BlockingIOError:break
            task = asyncio.create_task(sink.write(b'fixture\n', validate=lambda:None))
            await asyncio.sleep(0)
            self.assertFalse(task.done())
            task.cancel()
            with self.assertRaises(asyncio.CancelledError):
                await task
            self.assertFalse(sink._writing)
            await sink.close_join()
        finally:
            await sink.close_join()
            os.close(r)

    async def test_uncertain_os_close_never_becomes_success_on_retry(self):
        r, w = os.pipe()
        os.set_blocking(w, False)
        sink = NonblockingPipeSink(w)
        try:
            with patch('joined_webcam_delivery.os.close', side_effect=OSError('synthetic uncertainty')):
                with self.assertRaises(OSError):
                    await sink.close_join()
            with self.assertRaises(RuntimeError):
                await sink.close_join()
        finally:
            os.close(w)
            os.close(r)


if __name__ == '__main__':
    unittest.main()
