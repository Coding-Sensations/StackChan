# Human-operated USB pairing handoff

`user_pairing.py` is a one-shot interactive transport for the existing SCPG v1 grant format. It is pinned in source to device `<approved-device-id>` and endpoint `wss://<approved-lan-host>:18443/companion/audio`. It neither issues credentials nor validates gateway readiness. A qualified route and a current, approved temporary token in a protected local preparation sheet are prerequisites; this source change does not mean either is ready.

Only the human operator runs the CLI and enters the token. Do not put a token in chat, command arguments, shell variables, a pipe, redirected input, or logs. The script reads no credential file; copy the token manually from the protected local sheet into its masked terminal prompt. Python may retain immutable token/frame data in process memory temporarily; the script does not promise secure memory erasure.

Use the existing official pyserial 3.5 installation in the IDF Python environment. No dependency or USB driver installation is needed:

```
cd $STACKCHAN_ROOT/companion/host/streaming
$STACKCHAN_ROOT/.tools/espressif/python_env/idf5.5_py3.13_env/bin/python user_pairing.py --port <verified-robot-serial-port>
```

Replace only the serial-port placeholder with the independently verified robot port. There is no automatic port discovery. Run in a real terminal without redirection; stdin, stdout and stderr must all be TTYs. Verify the printed pinned endpoint and device. Enter the current token at the masked prompt. After that prompt, physically open Companion on the robot and tap **Pair trusted host**. Type the exact word `PAIR` only while that window is open. Enter or any other response cancels before the serial port is opened.

Opening a USB-UART serial port MAY reset the robot despite disabling DTR and RTS before opening. That can close the physical Pair window. The CLI cannot detect a reset or an expired pairing window, and it never retries automatically. If the device resets or no staged grant appears, close the attempt, reopen the physical Pair window and manually rerun with a still-valid token; do not assume receipt from the write result.

The port is constructed closed (`port=None`), DTR and RTS are set false, then the selected port is assigned and opened at 115200 baud. One frame of at most 755 bytes is submitted in exactly one write with a two-second write timeout, then the port is closed. A partial write or exception fails without a retry. The CLI does not read serial or promise an acknowledgement. Inspect the robot screen for a staged grant. **Connect** is a separate physical action to take only when ready; this CLI never initiates audio, network connections, automatic pairing, or NVS writes.

Offline verification:

```
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest test_user_pairing -v
```

The tests exercise the real CLI and real provisioning framing. Masked terminal input and the physical serial boundary use offline surrogates. They do not prove actual USB reset behavior, robot receipt, pairing-window state, TLS trust, gateway admission or audio operation.
