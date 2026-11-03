# Bounded Foundation process fixtures

`run_process_fixtures.py` runs only the three reviewed finite synthetic fixtures:
LifecycleChecks, CompanionChecks and PersistentChecks. It checks each executable
against a supplied SHA256 manifest. Compile them with `Tests/FixtureCustody.swift`;
all actual Foundation Process launch sites in these fixtures use that helper.

This is a separate monitor policy, rather than a relaxation of the original
single-group watchdog. `fixture_watchdog.py` retains the original bytes and its
strict policy. The new monitor admits descendants across process groups only
when kernel birth identity, current parent identity and the owned root session
are verified. Before any synthetic Python test body runs, it atomically publishes
an owner-only registration and stops itself. The monitor persists custody before
sending CONT to that exact birth. Every observed nonroot birth needs a unique
admission; missing, malformed, stale or incomplete registrations fail the run.
Known birth identities remain eligible for individual cleanup after root exit,
reparenting or session violations. No global process inventory or group-wide
signals are used. Cleanup success is distinct from fixture success.

The caps are unchanged: 2 GiB sampled RSS, eight aggregate threads, 300 seconds,
1 GiB across disjoint supplied workspaces, and 200 GiB free drive space. Include
all generated build areas and the receipt directory as `--workspace` arguments.
The receipt and active snapshot must resolve inside those measured roots.
A nonzero fixture exit, telemetry failure, resource breach, custody gap or failed
cleanup prevents a passing result. Existing failed receipts must be retained.

This is a sampled supervisor for audited cooperative no-fork workers, not a
sandbox for hostile code, arbitrary subprocess trees or daemonization. Python
interpreter startup occurs before admission; transient resource usage between
samples is not an atomic quota. Grandchildren and session escape are rejected.
There is no claim of kernel-enforced prevention of a fork/reparent race. Do not
use this runner for hardware, native audio IO, capture, network listeners, model
training or arbitrary commands. No security setting or entitlement is changed.

Example after building and reviewing the fixtures and their binary pins:

```sh
python3 script/run_process_fixtures.py \
  --binary-dir /absolute/build/fixtures \
  --workspace /absolute/build \
  --workspace /absolute/firmware-build-area \
  --workspace /absolute/evidence \
  --pins /absolute/evidence/binary-pins.json \
  --receipt /absolute/evidence/LifecycleChecks.json LifecycleChecks
```

The binary-pins JSON maps each fixture name to its SHA256. Inspect the binaries'
source closure before trusting that manifest. For PersistentChecks' import-only
probe, configure the same existing dependency/project root as the worker:
`STACKCHAN_FIXTURE_PROJECT_ROOT` optionally selects it, otherwise the source
checkout is used. `HOME_LAB_ROOT` supplies the existing host source. Neither
constructs the native adapter nor opens audio. No package is installed by this
runner. The synthetic controller cases use inert roots and mocked route gates.

Run the pure model checks with `python3 script/test_fixture_custody.py`.
They inject a model kernel and mock all signal sends; no live subprocess or native
library is used. Runtime qualification additionally exercises real Foundation
pipes, repeated clicks, cancel/retry, malformed status, EOF, broken input,
TERM-resistant worker KILL, persistent cleanup retention and inert module import.
These results do not establish physical device or live audio acceptance.
