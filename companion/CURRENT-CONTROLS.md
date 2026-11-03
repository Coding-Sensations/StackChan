# Current controls and supported next step

Michael physically reports the custom muted screen; see BOOT-SCREEN-ACCEPTANCE.json. This confirms boot/display text, not touch, pairing, audio, camera or motion.

- **Connect:** checks for a volatile grant staged internally by StageAuthorizedGrant. With no grant, changes text to “Muted - trusted host grant required” and does not start Wi-Fi. With a grant, explicitly starts saved Wi-Fi using RAM configuration, then connects to authenticated WSS /companion/audio. No grant issuer or external provisioning surface is wired into the flashed image.
- **Allow this conversation:** requires a connected host in ConsentRequired state; sends physical consent for the negotiated epoch. Audio additionally requires host armed acknowledgement. Without a session, displays “Muted - host session not ready”. It does not pair the robot or select Mira.
- **Mute:** closes the socket, requests codec stop, invalidates the epoch and clears pending credentials. Physical speaker silence/stop latency remains unmeasured.

Smallest supported user step now: leave the robot muted. There is no pairing code, phone-app companion setup, host address field or connected grant provisioning step to offer yet. Do not ask Michael to invent a token or put one in chat. These controls do not activate camera or motion.

Implemented protocol: bounded SCAV v1 binary Opus frames;16k mono60ms uplink,24k mono60ms return; begin/ack/physical-consent/armed/heartbeat/playing/interrupt controls;epochs,sequences,two-packet limits and a2s heartbeat. Firmware rejects fixture=true. The host StreamingSession uses HomeLab in-memory DeviceRegistry and ConversationSessions boundaries; gateway_connection.py services an already authenticated socket without CoreAudio. gateway_admission.py now validates an existing device grant, raw duplicate headers, device binding, active status and audio capabilities before a future listener constructs the session. It issues no credential and starts no listener. Offline fixtures are not trusted real hosts.

Next implementation: a supported trusted grant-provisioning channel into firmware, real remote WSS endpoint/TLS lifecycle, provider/audio adapter and evidence-backed consent qualification. Actual existing-Mira/dot audio attachment remains unverified by inspected supported interfaces. Standalone operation with the Mac off remains the target; neither Mac loopback nor phone/USB fallback is silently substituted. No real pairing grant, persistent access, provider key or live audio has been created.
