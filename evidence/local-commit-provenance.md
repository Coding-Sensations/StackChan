# Local reviewed navigation repair provenance

This commit adds the exact 74 source/test files listed in `handoff-review-v4-manifest.json`, plus selected evidence records. Their reviewed SHA256 values were rechecked before staging. No production source changed after independent review or the successful full firmware build.

## Identity and scope

- Branch: `codex/current-navigation`.
- Direct commit parent / repair base: `832730492ab9ae68d693ce6c2e7b024aba3c8d82` (Add inactive checked adapters for finite robot acceptance).
- Base parent: `5c8228d27fff8488403b98d07783ccef969c8943`.
- Local main ancestry anchor: `1b5765599fba8aaad1811d9a79358ccc7051f5f3`. The repair base has seven existing commits above that anchor; this commit preserves all of them.
- Source scope: Companions/Mira/Stackchan routes and labels; direct companion setup state with consent retained; Home routing and checked runtime/transport/audio/UI cleanup; bounded screensaver preview work; exclusive saved/vendor/phone Wi-Fi ownership with joined consumer cleanup and default-loop event barriers; focused host tests and pinned generated teardown adaptations. No screensaver assets removed.
- Exact source file list and hashes: `handoff-review-v4-manifest.json` (74 files). Complete precommit source patch retained as local evidence: `handoff-final.patch`, SHA256 `22e7c01be69071f6a8181b134677411b07152d1119edfd3f5bd97cdba205418e`. The patch is not duplicated in the commit because patch context contains intentional whitespace; the immutable committed source and manifest provide the same reviewed content.
- The separate Companion mute fix remains unchanged and unpublished at `ba73fd6baf1bb394c286a9919b22c7b9f2fffa2b` on `codex/fix-companion-mute`; it is not incorporated into this commit.

## Validation and limits

- Full ESP32-S3 firmware configure, compile and link passed before commit. Binary: 4,725,552 bytes; SHA256 `f713520c466e071f84281787f71aeab7b4bddc6b9b3a3f3dac18fa4188965e2f`. Build receipt and final log are retained.
- All 13 host tests passed with no skips, independently rerun during review and rerun before commit. Relevant production-body fixtures run under ASan/UBSan.
- Independent review checked all 74 hashes and found no remaining blocking source regression. Earlier navigation and runtime review records are retained alongside the final handoff review.
- Normal `git commit` hook handling is used without bypass flags, hook-related environment overrides or hook changes. No custom hooksPath is configured and only default sample hooks were present. The first commit attempt stopped before writing a commit because this worktree had no author identity configured. The successful command uses only the existing source repository identity (`mrkillbob`, GitHub noreply address) through command-scoped Git identity configuration; global and repository identity settings are unchanged.
- No publication, flash, reset, device settings change, microphone/audio/motor activation or stress test occurred. Actual installed firmware identity, physical Home behavior, real shutdown timing and the reported device freeze cause remain unverified.
- Historical October 15 source/readback provenance is retained separately and is not evidence of today's installed image.
- Screensaver removals await the user's deselected names/photos or the precise saved playlist value; no names were guessed and no assets were removed.

## Proposed publication destination (not performed)

Publish this branch, preserving its full ancestry, to a new `refs/heads/codex/current-navigation` in `https://github.com/mrkillbob/StackChan.git`. That URL is the configured origin of the local source repository. This worktree's own origin points to the local seed path, so a later authorized publication must explicitly select the GitHub URL or a correctly configured remote. Do not reset/rebase the source base, rewrite existing branches, or force-push. Remote branch availability has not been queried; if the proposed name already exists and is not an ancestor, use a fresh branch name.

A future review must account for the seven preserved base commits if the destination's main branch is still at the local ancestry anchor. Do not represent the full branch-to-main comparison as only this repair's 74-file delta. No push or PR was created.
