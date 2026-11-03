# Companion local TLS offline checks

Run `python3 -B verify.py` from this directory. It compiles only the portable
policy/configuration and shared lifecycle tests, then performs TLS handshakes entirely through Python
SSL MemoryBIO using the ESP-IDF 5.5.4 official mbedTLS fixture files. No network
socket, device, production certificate/key, or key generation is involved.
`--fixtures` can identify the same official fixture directory elsewhere.

The C++ tests first failed against public-root routing and then a fail-closed
configuration stub. They cover normal empty-profile public routing, local exact
private IPv4 matching, malformed/public hosts, partial profiles, invalid ports,
CA bytes, peer-name verification, timeout and absence of bundle fallback.

In-memory certificate tests accept a valid chain and reject wrong CA, wrong host,
and expiry (explicit verify error 10). The SDK fixtures contain legacy SHA-1
signatures, so only the test contexts set SECLEVEL=0 to load those historical
fixtures. Firmware has no such override. Fixture hashes are printed in receipts.
These checks use host OpenSSL, not the ESP-IDF TLS runtime: root must also compile
and independently review the real firmware integration.

Generated firmware must supply static-lifetime NUL-terminated accessors:
`extern "C" const char* companion_local_ca_pem()` and
`extern "C" const char* companion_local_tls_host()`.
Empty both retains existing public WSS; partial configuration is rejected.
A configured local profile accepts only its exact canonical RFC1918 IPv4 literal
and the configured CA, verifies the certificate against that literal (IP SAN
required), and never falls back to public roots after failure.

`CONFIG_MBEDTLS_HAVE_TIME_DATE=y` and a qualified current clock are required for
local TLS. The adapter refuses local connections when date verification is absent
or system time precedes 2025-01-01. This does not establish trusted clock provenance.
It does not change Wi-Fi, NVS, USB pairing, boot consent, or production grants.
Socket shutdown joins the receive task before TLS/task state is destroyed; callers
must stop from their owner task, outside receive callbacks.

The shared production release helper is tested against a transport whose destructor
asserts it was joined, including when peer closure has already cleared the connected
flag. The write budget tests cover stop, WANT retry deadlines and partial progress
without extending the deadline. The release test failed before implementation and
passes with the shared production helper. These portable tests do not execute the
FreeRTOS driver: SDK compilation and task scheduling review remain necessary.

Receive callbacks must retain their owner's lifetime and never destroy the socket.
Callback-thread Close records closure and defers joining to a retaining non-callback
owner. Callback-thread destruction terminates unconditionally to prevent freeing
live task state; ordinary owner Close always joins before releasing its reference.

Writer-quiescence regression first failed for a queued send after closure. The
production shared helper now locks before checking live state and transport, and
release joins before acquiring that same writer lock. LocalTls additionally drains
its internal writer lock after receive join before freeing the TLS context.

Offline build-profile tests: `python3 -B test_build_profile.py`. They use the
pinned ESP-IDF public CA fixture (expires February 2029), rather than the temporary
LAN certificate. The build generator rejects an expired CA.

The full WebSocket data-frame construction, continuation-state transition and
transport send now run inside the writer helper's locked operation. Port parsing
rejects empty/nondecimal/overflow/out-of-range input before transport creation;
the malformed-port regression first reproduced an uncaught std::stoi exception.

The five-second monotonic check is a retry/progress budget between ESP-IDF write
calls, not a strict end-to-end deadline. The SDK config also sets a five-second
socket send timeout; a blocking write entered near the retry deadline can exceed
it. SDK record fragmentation can add more than one socket call. Stop uses socket
shutdown to wake in-flight I/O before joining/draining writers. Actual FreeRTOS
and lwIP timing still requires firmware qualification.
