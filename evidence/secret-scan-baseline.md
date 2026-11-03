# Exact historical secret-scan false positives

The accompanying JSON classifies exactly17Gitleaks v8.30.1 findings:14historical
build-receipt source_sha256 entries that equal same-commit Git blob digests,
2SOURCE-PINS file hashes that equal referenced cached source bytes, and1public
DES/TDEA unit-test-vector comment labelled NIST SP800-67. No credential value
is included in the evidence.

The .gitleaksignore entries bind exact commit:path:rule:line occurrences. No
path/rule/value wildcard is introduced. Full public-history scanning remains
enabled. The public123commit/27.35MB scan reproduces17findings; applying only
these fingerprints leaves zero findings. A synthetic non-authoritative key at
the same reviewed file and line421 in a NEW commit still produces a detection
and exit1. The canary is never published.

Gitleaks official Darwin arm64 v8.30.1 archive SHA256:
b40ab0ae55c505963e365f271a8d3846efbc170aa17f2607f13df610a9aeb6a5.
A separate reviewer independently checked all17entries, all16digest comparisons,
the test vector, complete-history zero-finding result and canary detection.

These exceptions do not authorize credential rotation, history rewrites,
scanner-rule changes, reduced history, repository settings changes or merges.
