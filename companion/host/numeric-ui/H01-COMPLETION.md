# H01 source completion — 2026-10-22

This candidate preserves the two-file lifecycle work inherited from
`e3642ddceefae70ede862cde83adc8d276fb42c6` and completes its remaining source issues.
The original working trees were left untouched; the inherited diff was saved
before editing an isolated checkout.

Worker stdout has one permanently nonblocking, bounded reader. Read/enqueue
and final drain/retirement share a lock so the final cleanup report precedes
exit finalization. Read faults, incomplete drains and trailing undecoded status
bytes prevent confirmed release. Multiple shutdown requests join one escalation
cycle and each completion runs once. Only ordered exit/final-output finalization
can confirm release; the deadline reports unconfirmed cleanup. Persistent workers
retain custody rather than being killed by the temporary-worker kill timer.

Real-pipe fixtures cover final cleanup reports, a still-open descendant pipe,
EPIPE, repeated shutdown, truncation, forced exit, and a deadline while exit has
already occurred but finalization is withheld. The descendant fixture releases
its child before asserting. The multi-record malformed fixture uses one atomic
pipe write, avoiding read-order-dependent expectations.

A fully consumed malformed line retains the existing contract: it marks the
attempt failed, but an otherwise clean exit with no cleanup issue may confirm
process release. Unprocessed/truncated output now keeps cleanup unconfirmed.
These verdicts establish worker process release, not microphone restoration,
call attachment, physical mute, device behavior or a supervisor receipt.

Validation: all seven executables from `script/test_offline.sh` pass with
`STACKCHAN_OFFLINE_BUILD_ROOT` pointing inside the task workspace. The complete
SwiftUI app build was attempted with `--build-only`, but its macro subprocess
failed because sandbox application was denied. No app launch or device action
was performed. The HomeLab polling candidate is a separate repository/commit;
there is no direct code dependency between the two repairs.
