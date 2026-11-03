# Proposed Task5 outbound producer contract

This API is a source proposal. It is separate from inbound CompanionSession activity/permission events and supplies no accepted native producer evidence.

## Privileged construction

`Producer(absolute_journal_path, source=None, port=None, clock=time.monotonic, wait_limit=3.)`

The installation controller supplies the journal path, native authority object, captured transport object and clock. They are dependencies, never fields from a message/model payload. An omitted source becomes `UnavailableAuthority`; no supplied source authorizes production work. No constructor discovers a receiver, session, token, endpoint, account or device. The port is borrowed: this implementation never closes a shared socket/client.

The caller serializes this object on one asyncio event loop. `Scope` is frozen correlation data with explicit `user`, `device`, `session`, `receiver`, `provider`, `binding`, `generation`, `native_incarnation`, `operation`, `expires`. Identifiers are 1..64 visible ASCII letters/digits/underscore/dot/colon/hyphen; expiry is finite native-owner monotonic time, at most120seconds ahead at admission. The native owner must map its real lifetime/generation to that clock. A device receiver principal and desktop/session principal are distinct fields. A Mira session hint cannot manufacture or select a receiver.

`source.current(scope, action, exact_port) -> True` is a privileged native-owner call. Actions are `submit`, `receipt`, `cancel`, and the distinct retained-owner `cleanup`. It must verify actual user/device/receiver binding, provider/session, foreground/consent, native generation/incarnation and exact captured transport object. Scope labels or a JSON `authenticated` flag do not establish those facts. Missing/false/exceptional authority refuses. Source callbacks and journal filesystem operations must themselves be locally bounded and serialized; this proposal does not create a scheduler or worker to interrupt arbitrary privileged code or a hung filesystem.

`cleanup` may authorize only cancellation of an envelope previously captured by this exact original owner after its send lease expires or is revoked. The owner must explicitly retain that cleanup grant. It cannot grant new sends, rebind a port or extend another operation. Without current retained-owner cleanup evidence, the resource remains unresolved.

## Submission and receipts

`await producer.submit(scope, text) -> Result`

Text must be a string of1..1024 UTF8 bytes with no NUL or malformed Unicode. No other model arguments, receiver lookup, attachments, generic methods or native commands exist. The producer derives a message SHA256 and a domain-separated canonical envelope SHA256 over every explicit identity, operation, expiry and message digest. It durably consumes the operation before scheduling any transport call, then rechecks native authority/time immediately before dispatch.

The captured port implements only this proposed private interface:

```
async submit(envelope: Envelope, text: str) -> Receipt
async cancel(envelope: Envelope) -> Receipt
```

`Receipt(envelope, disposition)` must originate as the direct result of the captured checked transport. The producer checks exact envelope equality, including digest and every binding. A bool, model dictionary, different envelope or unsupported disposition becomes unknown. There is no public receipt ingestion method. The native transport must authenticate the actual receiver and correlate its real acknowledgement to this exact immutable envelope; merely echoing an envelope does not prove that in production. No such transport is supplied here.

Submission dispositions are `accepted` (transport admission) or `receiver_acknowledged` (the actual receiver acknowledged the envelope under that transport's contract). Neither implies physical display, speech, delivery to a human or native attestation. Every `Result.native_attested` remains false. Authority/time are checked after the call and after durable receipt recording. Caller waits are bounded by the lesser of the remaining scope lifetime and `wait_limit` (maximum3seconds). The native port owns per-output checks and bounded cleanup; Python cancellation cannot undo a remotely executed send.

`producer.receipt(scope) -> Result` reads custody for the exact authorized scope; it never contacts a receiver or accepts a caller-provided receipt. A fresh process reports persisted records as `historical_unknown`, with `receipt_checked=False`, even if a previous process recorded an acknowledgement. This prevents a restarted producer from claiming a live receipt from a replacement port.

## Cancel, close and unknown work

`await producer.cancel(scope) -> Result` requires the exact current authorized operation. It invalidates send-result adoption before requesting cancellation from the original captured port. Only an exact `Receipt(envelope, 'cancelled')`, retained-owner cleanup authority around the call, and completion of the original submission task can retire custody as `cancelled`. This records transport cancellation acknowledgement, never an assertion that a message was not already received or physically displayed.

`await producer.close() -> bool` retires the producer and attempts only retained original-owner cleanup. It uses one total `wait_limit` budget across all unresolved operations, never closes the shared transport, and returns false while any task or cancellation acknowledgement remains unresolved. Keep that original producer/port alive and call close again after the underlying owner resolves; never construct a replacement to retry. An unconfirmed/failed cancellation task is retained and not automatically replaced or retried. Cleanup results still require the original native retained-owner grant. Late send receipts after timeout/revocation/cancellation never promote unknown work to success.

State results: `accepted`, `receiver_acknowledged`, `cancelled` may have a checked receipt; `unknown`, `cleanup_pending`, `historical_unknown`, `unavailable`, `invalid`, `replay`, `unresolved`, `capacity`, `absent` do not. An uncertain record blocks new operation IDs on the same exact user/device/session/receiver/provider/binding/generation/incarnation route. A new native incarnation is not inferred from a label: it requires independently captured genuine owner authority.

## Durable replay boundary

The explicit journal uses a caller-owned directory without group/world write access and an owned regular0600 non-symlink, single-link file. SQLite DELETE journaling, EXTRA synchronous commits and a128page ceiling bound metadata storage. It stores scope/envelope metadata, hashes and state; never plaintext message content, transport tokens or credentials. Python live pending task frames can retain message text until actual join; zeroization is not promised.

Operation IDs are journal-wide unique across principals, payload changes, process restart and prior outcomes. The journal retains at most64 operations, never evicts or silently rotates consumed IDs, and refuses at capacity. Preserve the same original journal across restarts. Pre-dispatch state is already `unknown`; a real abrupt process crash cannot trigger automatic resend on startup. Native recovery must resolve the original owner, not use a new operation ID to bypass uncertainty. The trusted installation controller must protect the journal from deletion, replacement, snapshot rollback and copying into another installation; this isolated proposal supplies no OS anti-rollback or native journal identity attestation. A corrupt/unwritable/locked journal fails closed. No recovery path automatically transmits.

Journal metadata still identifies routes and message hashes can permit guessing; metadata-only is not anonymity. Installation owner must preserve access controls, anti-rollback and retention. No real message or journal is enrolled by this source PR.
