# Independent narrowed navigation preparation review

Outcome: no narrowing regression or dependency on the three excluded host-only commits found. This is a source-equivalence and dependency review, not approval of the inherited base or authorization to publish.

## Exact identity and equivalence

- Worktree: `/Users/mikedemott/Developer/AgentWork/Codex/2026-10-19/task-9/stackchan-narrow-navigation`
- Branch: `codex/narrow-navigation`
- HEAD/new base: `4a24491761ffab19f3df758c93ebbdada4c29ddc`
- Preserved candidate comparison: `66b81c1eba1633374423bbd7f2b4443ebc247ea7`
- Reviewed manifest: `evidence/handoff-review-v4-manifest.json`, SHA256 `9a8a7380506d4d48e9bf81037adf40b95de0e53409dab950bbb2a73543c7340f`
- All 74 source/test file hashes match the reviewed manifest. The complete staged tracked `firmware/` tree is identical to preserved candidate 66b81c1, not just the manifested files.
- Exactly 86 staged files: 74 source/test files and 12 historical evidence/provenance records. No unstaged tracked changes.
- Exact full staged binary/full-index patch SHA256: `86d8818e92c8230b60c0428218c324bbdd2babe051ef304573a19fd8c308cd04`
- Staged firmware-only binary/full-index patch SHA256: `71bd215122f3e556959171077a5b76c7a5b327f8464684d6dd0f59618bb8e60b`

## Exclusions and dependencies

Independently compared all paths changed by `4a24491..8327304`. All seven match the new base's Git blobs or absence, and all differ from the local 8327304 state as intended:

| Excluded host path under `companion/host/streaming/` | Narrowed state |
| --- | --- |
| `WEBCAM-WORKER-EMITTER.md` | Exact 4a24491 blob |
| `webcam_transport.py` | Exact 4a24491 blob |
| `webcam_worker_emitter.py` | Exact 4a24491 blob |
| `test_webcam_worker_emitter.py` | Exact 4a24491 blob |
| `finite_transport_acceptance.py` | Absent, as at 4a24491 |
| `test_finite_transport_acceptance.py` | Absent, as at 4a24491 |
| `finite_acceptance_runtime.py` | Absent, as at 4a24491 |

Inspected the scopes and relevant source of excluded commits `51d0c05`, `5c8228d`, and `8327304`. The first retains producer capture timestamps through host webcam delivery; the other two add inactive host acceptance owners/adapters. They change no firmware files. No firmware/build/test dependency on their new modules or webcam emitter API was found. The firmware's explicit host-side trust-generation input, `companion/host/streaming/public_config.py`, has the same blob `194a895a6bf3e76fa1a0b57fff86724fc1fc7cf2` at 4a24491, 8327304, preserved 66b81c1 and the narrowed index. The older retained emitter and its corresponding fixture remain consistent.

## Independent checks

- `ctest --test-dir /tmp/stackchan-narrow-host --output-on-failure`: **13/13 passed**, no skips, 3.46 seconds. The production-body fixtures execute within the narrowed checkout.
- `python3 -B -m unittest test_webcam_worker_emitter` from the narrowed host streaming directory: **14/14 passed**, 0.005 seconds. These are pure synthetic tests, with no hardware/native runtime activation.
- Rechecked all manifest hashes, firmware-tree equivalence, staged count and absence of unstaged changes after the parent restored inherited ignored build inputs; all remained unchanged.

## Inherited reproducibility limitation

The parent discovered that a fresh firmware configure needs 21 ESP-NN `.S` inputs absent from both the published base's and preserved candidate's tracked firmware trees. On this case-insensitive filesystem, the vendored `*.s` ignore pattern also ignores these `.S` files. This limitation predates the narrowing and is independent of the excluded host commits.

I independently checked the restored local inputs listed in `NARROW-INHERITED-ASSEMBLY-INPUTS.json` (receipt SHA256 `4958f94ef8e189b35681cbfac4ef17219479d841668d3de269fac06c7f67bf58`): all 21 match their recorded SHA256 and corresponding entries in `firmware/components/mira_wake/SOURCE-PINS.json`; none is tracked in either 4a24491 or 66b81c1. A successful local build with these inputs must not be represented as a self-contained fresh-checkout build. No assembly files were added to the candidate by this review. The new full target build was still running when this review was recorded.

## Scope and remaining findings

PR1's current base review is COMMENTED, with the two reported unresolved P2 findings: floating-point `abs` in the generated catalog and the stock-menu fixture missing Screensaver. They remain outside this narrowing review and were not fixed or treated as cleared. The preserved evidence files describe the old branch/base/build and must be read as historical provenance, not current narrowed-branch receipts.

No source edits, commits, publication, hardware activation, flashing, resets or device settings changes were performed. Preserved candidate 66b81c1 and the separate mute fix were not modified.
