# Full-history and octopus content coverage

The secret lane keeps its pinned checkout with `fetch-depth: 0`, immutable Gitleaks8.30.1 archive/checksum, redaction, strict aggregate and full-history `--all --full-history --remerge-diff` scan. No commit is exempted by SHA or parent count. No source/history, permissions, branch protection or remote refs are changed by the scanner.

Git cannot reconstruct an octopus remerge diff. Per-parent addition patches alone are also insufficient: a multiline secret can combine unchanged lines from different parents without any addition patch containing the complete pattern. The real Git/Gitleaks fixture reproduces that miss.

The additional coverage enumerates every reachable commit, including other branches/tags and detached HEAD. For each merge with three or more parents it compares the result against **every parent**, without rename heuristics. The union of changed result paths selects complete resulting blobs; removed paths have no resulting content and remain covered by their original history. A blob unchanged against every parent introduces no merge-only content. Earlier octopus ancestors are inspected independently.

Selected regular/executable blobs and symlink target bytes are referenced by object ID in an isolated temporary Git index. They are never executed, followed as symlinks or extracted into a worktree. Parentless inspection commits make each entire selected file available to Gitleaks, including unchanged context. Forced-text diffs disable binary attributes, external diff and textconv shortcuts. Original objects are read through an alternate object directory; original commits/refs are not modified. Changed gitlinks and unsupported modes/formats fail instead of silently omitting unavailable content.

The selected current policy is retained. Historical policy files remain blob content, not an automatically loaded scanner configuration. Findings from inspection commits map back to the original merge and exact original path/line/rule fingerprint; only an exact existing `.gitleaksignore` fingerprint can suppress that mapped finding. There is no merge-wide allowlist. Raw tool reports and a coverage manifest retain inspection IDs, every parent edge and selected blob IDs. Console output includes only finding location/fingerprint metadata; Match, Secret and raw scanner/error output are never printed. All tool invocations request full redaction.

Preflight refuses shallow/grafted/replaced/incomplete object graphs and unsupported object formats. It probes required Git functionality and the exact Gitleaks version. Missing/malformed/inconsistent reports, tool errors, parser error logs, timeouts, incomplete parent graphs and changing refs fail closed. Exit0 means completed coverage without unsuppressed findings;1 means findings;2 means incomplete coverage. The existing required-check aggregate accepts only success.

Run the full fixture suite with `STACKCHAN_GITLEAKS_BIN` pointing to the checksum-verified8.30.1 binary:

```
python3 -m unittest discover -s scripts/ci -p test_secret_history.py -v
```

CI runs these real Git/Gitleaks fixtures before scanning the actual checkout. They generate only synthetic values and never print them. This covers clean merges, merge-only and multiline resolutions, every-parent contents, non-HEAD refs, binary attributes/NUL, ordinary two-parent remerge/history and failure paths. No device, credential service, listener or live runtime is involved.
