# Remote companion integration contract

Status: source-only design mapped to implemented boundaries; no listener, real grant, provider credential, deployment or service activation. Production acceptance requires the robot on another internet connection with Michael's Mac off. See EXPANSION-PLAN.md for the supported-interface feasibility blocker.

## Existing boundaries and exact gaps

| Boundary | Existing source | Required remote integration |
| --- | --- | --- |
| Robot grant staging | firmware/main/companion/boot.h: AuthorizedGrant, StageAuthorizedGrant | Trusted issuer/provisioning surface, device-specific revocable audio scope, verified endpoint; no issuer currently connected |
| Robot network | firmware/main/companion/boot.cc: existing Wi-Fi read-only, explicit Connect | Remote WSS trust/lifecycle qualification; no vendor provisioning fallback or provider key on robot |
| Streaming admission | companion/host/streaming/streaming_session.py: StreamingSession | Real gateway authenticates a device before session construction, derives identity from authorization, enforces capabilities and revocation; never pass arbitrary client session references through |
| Conversation binding | same file: native_session_ref, attach_external_session | An external reference is local bookkeeping, not an actual provider attach call or proof of shared memory. A supported adapter must establish and attest the intended existing conversation independently |
| Audio IO | ../../bridge/host-adapter/host_adapter.py (project-level source): CodecHostBridge | Remote provider adapter, bounded queues/codec ownership and verified format negotiation; CoreAudio is optional local diagnostics |
| Test listener | companion/host/streaming/run_loopback.py | Keep the finite localhost synthetic fixture test-only. It is not a production endpoint and never accepts real grants |

The endpoint validation helper require_production_tls enforces WSS, the /companion/audio path and no credentials in URL userinfo/query/fragment. This syntactic check does not establish trusted certificates, DNS reachability, deployment or provider identity.

## Connection sequence

1. Verify a supported existing-Mira attachment interface and its permitted conversation/memory scope before claiming Mira integration. Public native voice docs do not expose the needed raw-audio attachment; MCP Events is discrete, not streaming. GPT-Live can connect audio to a backend, but creates an API voice session with separate credential/billing requirements. Do not substitute it without an explicit scope decision.
2. The trusted remote service authorizes the already paired device and selects the verified provider/session adapter. Provision only a bounded, revocable device grant and WSS endpoint to the robot through a reviewed trusted channel; keep provider/account credentials server-side. Do not turn the internal staging function into an unauthenticated serial/network listener.
3. Robot Connect establishes WSS and begins a turn. Acknowledge the negotiated epoch; only explicit physical Allow plus host arm acknowledgement enables audio. A session label or host_audio_qualified boolean must not replace actual provider qualification.
4. Gateway bridges audio through bounded codec queues. Revalidate device authority, epochs, sequence and heartbeat. Disconnect, mute, revocation and provider errors stop acceptance and invalidate pending output. Provider cancellation and physical speaker silence need separate measurements.
5. On reconnect, require a current authorized grant and a fresh accepted turn; never replay old speech or actions. Keep original firmware recovery separate from runtime transport configuration.

## Evidence needed before production

- Robot microphone/speaker and panel acceptance after separately approved image write, using the NVS-preserving minimal-write proposal; Michael declined full backup.
- TLS handshake against the intended remote service, certificate rejection and revocation/expired-grant rejection.
- Existing Mira conversation identity and intended memory continuity demonstrated through a supported adapter, not inferred from a string or matching voice.
- Measured bidirectional speech, mute, disconnect, interruption and bounded latency; explicitly report any unconfirmed cancellation/silence/barge-in behavior.
- Repeat on a different internet network with the Mac off. Optional paired computers add capabilities only after discovery and authorization; USB power is not host authorization.

References: https://learn.chatgpt.com/docs/features/voice ; https://developers.openai.com/plugins/build/mcp-events ; https://developers.openai.com/api/docs/guides/live
