# Existing StackChan event interface and missing native producer

This is an offline source interface, not proof of a physical-device/desktop producer or this Mira conversation. Task5 owns the existing MCP Events bridge. No listener, subscription or event delivery is started here.

`companion_events.CompanionSession(provider,binding,generation,event_key,decision_key,scopes,clock)` selects one authenticated routing generation. `receive(packet, signature)` checks an exact field set: version1, provider, binding, generation, sequence, kind, task. Decision packets additionally require request_id, operation_sha256 and choice. Task/session IDs are bounded correlation strings, not authorization. There is no wire device ID, user/installation identity or OpenAI conversation field; none may be inferred from binding.

Signing is HMAC-SHA256 over domain `stackchan-provider-events-v1` followed by a NUL and canonical sorted compact UTF8 JSON. Signature is lowercase64hex. Activity and approval use separate owner-provided keys and independently increasing integer sequences below2^63; exact provider/binding/generation and scopes must match. No production key loader, key enrollment or native Source producer is supplied. A caller possessing a key can forge its local event: authentication proves only the selected key, not native model/device/conversation attestation.

A request binds exact native task/request/operation digest, bounded deadline and original response callback. A decision is retired before callback invocation; uncertain callbacks are not retried. Disconnect marks inactive, cancels pending requests, marks submitted requests uncertain and removes callbacks. The caller must issue a fresh unique generation on real source replacement/reconnect and maintain synchronous current-authority/revocation checks. There is no durable anti-replay store across process restart or replay nonce beyond generation+sequence; the trusted producer must provide that lifecycle and prevent reused generations/keys.

`provider_room_bridge.RoomBridge.snapshot()` produces bounded directional HMAC room frames: provider/binding/generation/sequence, scene and exact decision presentation fields/TTL. It proves local publication custody only, not device display, delivery ACK, microphone capture or an OpenAI response. `hermes_adapter.HermesAdapter` correlates exact API-owned profile/device/session/run, revalidates an injected current Source around callouts, and projects signed activity. Its sole production Source implementation remains unavailable. API-owned Hermes conversation is not a native/Desktop session and not this Mira chat.

Native hook projection supports exact bound Claude session / Codex thread and turn/request fields. Payload projection and synthetic hook fixtures do not establish hook installation, authenticated source process, per-user native attestation or a physical StackChan producer.

For Task5: **do not feed these events into HermesCompletion or enable the launcher** as if they carried profile/instance/admission/input/authority-epoch/execution-generation facts. No tested current source supplies those facts. A separate authenticated physical or desktop producer contract must specify its principal, device/installation identity, exact current session, source generation/expiry, revocation and durable duplicate semantics. Then Task5 can implement the appropriate event schema/durable commit and receiver binding. Only an admitted platform subscription from the exact eligible conversation establishes its callback destination; do not accept a model-supplied callback/session or an `authenticated:true` flag.

Existing tests cover wrong provider/binding/generation/signature, replay and terminal revival, expiry/disconnect, stale decision/presentation, callback uncertainty, exact native hook projection, and Hermes Source revoke/close/repeated turns with synthetic ports. They establish source logic only; no native producer, physical-device emission or platform subscription is qualified.

## Current source fingerprints

{
  "companion/host/streaming/companion_events.py": "a47d7e49a192a62cfaf9830fe426b5ba6730f33c28c6294a9739ac450bfbd0e2",
  "companion/host/streaming/provider_room_bridge.py": "993474dc608591b326d830775c69db3d62f28865683d102b8431e0634a86bc0a",
  "companion/host/streaming/hermes_adapter.py": "44430ade8bb806b8b883bf30e7eb175990c6b4a463dac54c6885d24493acee5f",
  "firmware/main/companion/provider_room_wire.hpp": "da8db2e2b3145192bb57dc68ef66f08ad1b266e22f4930ef0869a596496f867f"
}
