# Platform binding research update

Production target remains standalone authenticated Wi-Fi/WSS with Michael's Mac off. No phone/cable requirement has been substituted. Limited app/OTA write and reboot confirmation remains unanswered; no hardware operation is authorized by this research.

- [Dot channels](https://learn.chatgpt.com/docs/dots/channels) documents reaching the same dot through supported connected contact methods and native ChatGPT calls without resetting memory. It does not document an external robot raw-audio attachment interface.
- [GPT-Live session management](https://developers.openai.com/api/docs/guides/live-conversations) concerns API-created sessions. [Server controls](https://developers.openai.com/api/docs/guides/voice-server-controls) explicitly attaches using an API-created/accepted session ID and the project authentication owning it. Its sideband provides reflected audio/control; microphone injection belongs to the primary connection. This does not document access to a native Mira dot room.
- [Codex App Server](https://learn.chatgpt.com/docs/app-server) resumes stored Codex thread IDs. A resumed Codex thread is not evidence of Mira dot memory/voice attachment.

Inference from these inspected interfaces: same-Mira external audio binding remains unverified. This is not a finding of technical impossibility. No API credential, paid session, private endpoint or undocumented app state was used.

Optional future concept, only for discussion: a robot exposing USB microphone/speaker to the phone might route an actual native Mira call. [Apple documents USB-C audio/microphone accessories](https://support.apple.com/en-us/105099), but the particular phone, ESP32-S3 UAC/full-duplex implementation, power budget and native app input/output routing are unverified. This needs a phone and cable and does not meet the standalone Wi-Fi target. No firmware for this fallback is implemented or authorized.

The tested remote connection driver and its 11 offline tests remain independent of platform binding; actual deployed TLS transport, provider adapter, hardware audio and continuity acceptance remain open. A local session reference is bookkeeping, not platform attachment proof. Full backup/encryption remains declined and is not a prerequisite.
