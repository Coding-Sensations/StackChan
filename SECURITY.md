# Security reports

Do not post credentials, live device bindings, private certificates, capture data
or a working exploit against someone's robot in a public issue or PR.

If the repository's Security tab offers private vulnerability reporting, use it.
Otherwise, ask the maintainer to establish a private reporting channel before
sending sensitive details. Do not assume private reporting is enabled. For an
upstream issue, follow the upstream project's reporting instructions.

Treat companion enrollment, consent, expiry, clock mapping, cleanup and device
provenance as security boundaries. Describe affected commits and a synthetic,
minimal reproduction where possible. Preserve notices and licenses in vendored
firmware and generated screensaver sources.

Public PR CI must remain isolated from private HomeLab infrastructure: hosted
runners, least-privilege tokens, no inherited secrets and no automatic device IO.
