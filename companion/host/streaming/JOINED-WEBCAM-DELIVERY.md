# Joined local webcam delivery owner

This successor adds an executable local output worker and an exclusive
nonblocking FD sink. Construction is inert; the caller explicitly transfers an
already nonblocking descriptor and starts the worker on its serialized event
loop. Nothing imports or starts this worker from the current Mac application.

`ProducerOwner.stop_join()` defines the producer shutdown boundary: revoke input
and join all owned callbacks, raising on uncertainty. `DeliverySink.write()`
checks the supplied exact-frame validator before every write and after every
capacity wait. `close_join()` must join its owned output callbacks/resources.
These are local ownership contracts. They cannot mint a camera-purpose grant,
trusted capture clock, DMA stop proof, or CMIO delivery qualification.

`JoinedWebcamDelivery` holds one latest frame through the existing reviewed
timestamp emitter and at most one in-flight event. Controls/audio are checked
before each event, with a hard eight-event burst followed by an event-loop
yield. Priority backpressure waits for the existing owner's explicit `wake()`;
there is no polling timer. Pipe backpressure uses the event loop's writable FD
notification. Exact frame authority and freshness are revalidated after waits
and before each partial write. Replacement of a blocked old event retires the
owner rather than finishing stale media. This conservative behavior needs live
throughput measurement before a production integration; it claims no frame rate.

Closing revokes ingress before its first await, cancels and joins delivery,
joins the producer, then closes/joins the sink. Failure retains the same owners
and produces no local join receipt. Cancellation of a caller waiting for close
does not cancel cleanup; another close joins that same cleanup task. A failed
producer join can be retried on the same owner; an uncertain OS descriptor-close
error remains blocked and is never retried against a possibly reused FD. There
is no restart on this owner. A later session requires a new admitted owner.

The opaque receipt establishes only this owner's local joins. It is not accepted
by `ExistingSessionStopDispatcher` and does not release physical Stop custody.
The exclusive pipe closes on cancellation, so a partially written final JSON
line must be rejected by the receiver at EOF. This sink must never adopt shared
stdout or another session's descriptor.

Offline tests exercise actual anonymous-pipe writes, partial-write revocation,
backpressure cancellation, latest-frame replacement, per-event priority,
authority loss, retained failed joins and cancellation of the close caller.
Mock producers/sinks supply deterministic IO failures and delayed joins. No
camera, device, listener, native extension or audio route is opened.

Next integration work remains: a production producer owner with original frame
timestamps and qualified physical shutdown, a CMIO delivery owner/decoder that
rejects incomplete EOF, and the exact permission/session-bound lifecycle bridge.
Automatic capture-on-open stays unwired until those owners and retained cleanup
are integrated and reviewed. The existing frozen hardware candidate is separate.
