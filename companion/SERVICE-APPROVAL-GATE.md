# Narrow deployment approval gate — not activated

The source sidecar/provisioner is implemented and tested. No deployment approval is being requested with guessed infrastructure. A concrete proposal requires an existing always-on host and approved reachable TLS endpoint to be identified and inspected. Current verified local HomeLab/Mac route does not meet the Mac-off requirement; connected Pi metadata alone is insufficient deployment evidence.

The complete later proposal will identify these exact resources before asking:

1. Existing always-on host and operating-system service identity; verified Python3.14.7 runtime and pinned source/package hashes.
2. Existing HomeLab state directory and owner pairing/revoke API; read credential hashes only in the audio sidecar. Permission scope is microphone/speaker for one device. No computer/home-control permissions, camera/motion or provider credentials on the robot.
3. Exact WSS hostname/path /companion/audio, bind address/port and existing trusted TLS/proxy configuration. No public/Funnel/router exposure, domain/certificate issuance or new account changes implied; any such change needs a separate concrete proposal.
4. A supported existing-Mira audio/session adapter with verified room/memory continuity, or an explicit user decision to change the target. With no such adapter, service returns503 and grant qualification staysfalse. Do not sell an API persona as equivalent.
5. Reviewed firmware provisioner image/hash and physical one-use window, plus exact limited app/OTA write scope if needed. The currently flashed image has no external provisioning receiver. Initial source protocol is volatile: it does not save a credential in NVS and needs reprovisioning after power loss. Persistent robot access is not silently added.
6. One owner-issued5min pairing code, redeemed through existing HomeLab authority into one revocable device credential, transported only through the approved private provisioning channel. No credential in chat, logs, Git or command arguments. No real grant is issued before specific approval.
7. Finite first test duration, explicit Allow for actual microphone/speaker transmission, measured mute/disconnect/revocation and physical silence. Raw audio is not retained. Normal home automation and existing routes remain operational.
8. Rollback: stop/remove only this sidecar service, revoke this one device grant, leave the robot muted and preserve its settings. No costs are authorized; any provider billing requires exact separate consent.

Blocking information for parent: which existing always-on host and reachable trusted TLS route should be inspected? No live deployment can truthfully be specified until those facts and the provider boundary are resolved. Continue source-only work without reopening the declined full-backup workflow.
