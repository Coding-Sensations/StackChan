# USB pairing receipts

SCPG grant framing remains version 1. SCPS receipts are exactly 22 bytes: magic SCPS, version 1, status byte, 16 payload bytes. Status 1 Ready carries a nonzero big-endian uint32 boot nonce, uint32 ESP reset reason (0..64), and eight zero reserved bytes. Status 2 Staged carries the 16-byte request UUID. Status 3 Rejected and status 4 Expired have zero payloads. No receipt contains a credential. Legacy non-UUID requests retain parser compatibility but receive a zero UUID receipt; the new host refuses that receipt.

Ready is emitted only after physical Pair has installed the owned USB driver and checked stale input. Acceptance retains driver ownership until StageAuthorizedGrant completes. The staging outcome is written once and flushed with bounded SDK waits before driver cleanup. ACK failure clears the volatile pending grant. Teardown failure after a delivered receipt retains the staged grant and driver ownership; Connect is blocked until USB cleanup succeeds. No settings are written to NVS. Receipt success establishes staging only, not gateway authentication or audio.

The human CLI opens USB before physical Pair, drains for two seconds, and requires screen confirmation. It then reads a masked token, requires physical Pair and Ready, sends one grant, and requires a matching Staged receipt. It holds the same descriptor until physical Mute confirmation. Opening or closing USB may reset the device even with DTR/RTS false. A changed Ready nonce indicates a changed boot; transport loss alone does not establish the reset cause. There is no automatic retry or legacy fallback.

Each ACK wait is five seconds with a 4096-byte input limit. The entire human session is bounded to 180 seconds. Failure output uses fixed status labels and never prints raw serial data or credentials.

## Next hardware test

The project pause marker remains set. Do not run pairing against the currently installed image: it predates SCPS. First review the new exact image/hash and obtain the required flash authorization. Preserve NVS, partition table, bootloader, assets and calibration. After the repaired image is installed, the smallest test is OPEN, screen STABLE, physical Pair, Ready, one synthetic short-lived grant, matching Staged, then physical Mute and MUTED. Keep USB open throughout. Any reboot, missing receipt, rejection or expiry ends the attempt; record only nonsecret status and revoke the ephemeral grant. Connect/audio require their separate prepared human step.
