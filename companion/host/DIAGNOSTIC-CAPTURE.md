# Stopped-only diagnostic capture

The helper captures one validated public SCFD frame. Keep Companion muted/stopped.
It does not write USB commands, pair, open audio, reopen the port, or flash firmware.
The default window is 30 seconds; an explicitly requested window can be up to 300
seconds. The byte scan remains bounded to 8192 bytes. Use a fresh evidence directory
and filenames for every capture; existing output and status files are rejected.

Use the project's pinned Python environment (Python 3.13.15, pySerial 3.5):

```sh
PYTHON=$STACKCHAN_ROOT/.tools/espressif/python_env/idf5.5_py3.13_env/bin/python
# Start only after authorization for a single stopped-only hardware capture.
"$PYTHON" -B companion/host/read_diagnostic_snapshot.py \
  --port /dev/cu.usbmodem31201 --timeout-seconds 300 \
  --output /absolute/fresh-evidence/snapshot.json \
  --status-file /absolute/fresh-evidence/status.json
```

From a separate terminal, observe that same capture. Copy its capture_id from the
starting status receipt; binding the observer to it prevents stale capture reuse:

```sh
"$PYTHON" -B companion/host/read_diagnostic_snapshot.py \
  --watch-status /absolute/fresh-evidence/status.json \
  --capture-id CAPTURE_ID --timeout-seconds 300
"$PYTHON" -B companion/host/read_diagnostic_snapshot.py \
  --check-status /absolute/fresh-evidence/status.json --capture-id CAPTURE_ID
```

Keep the watcher active through the user prompt and export. Run the fresh status
check immediately before asking the user to tap Export diagnostics. Readiness is
point-in-time: it requires confirmed serial open, a heartbeat no older than two
seconds, the current deadline, and a matching live process birth token. A historical
listening message alone is insufficient. Report terminal failure promptly and stop;
there is no automatic rearm. The watcher polls at 100 ms, with a bounded process
probe, so human message delivery is not guaranteed instantaneous.

A successful capture closes its owned reader, saves the validated JSON exclusively,
and exits zero. EOF, OS errors, cancellation and expiry produce distinct sanitized
categories and a nonzero exit. Cleanup errors retain primary_failure in the status
receipt. Raw exception text, serial console data and credentials are never persisted.
SIGKILL cannot write a terminal receipt; the independent observer rejects dead or
reused processes instead of presenting their old receipt as listening.

The POSIX backend returns one available chunk immediately. This avoids pySerial's
accumulating read discarding an already-received complete frame when a later read
fails. EOF remains terminal. Application DTR/RTS updates and break/output commands
remain disabled; HUPCL is cleared and verified. OS or USB-driver behavior on open
can still reset hardware, so these controls do not prove physical boot continuity.

Offline verification (synthetic PTYs, no robot USB):

```sh
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=companion/host:companion/host/streaming \
  "$PYTHON" -B -m unittest test_diagnostic_serial test_diagnostic_snapshot \
  test_diagnostic_reader_lifecycle
```
