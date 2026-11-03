# Independent PR1 P2 fix review

Outcome: no blocking regression found in the exact seven-file fix. Both supplied P2 findings are addressed by source changes and focused regression coverage. This does not certify unrelated PR1 code or physical behavior.

## Reviewed identity

- Worktree: `/Users/mikedemott/Developer/AgentWork/Codex/2026-10-19/task-9/stackchan-pr1-p2-fixes`
- Branch: `codex/pr1-p2-fixes`
- HEAD/base: `4a24491761ffab19f3df758c93ebbdada4c29ddc`
- Final manifest: `evidence/pr1-p2/review-manifest.json`
- Manifest SHA256: `0654fb54be972846b2101a0496bcaf66736db915a2d32b54d22f1069cb6ebcfb`
- All seven source/test hashes matched. Review used the updated Tools fixture hash `de9b74ca3b4116b76ebe65cc3cf76c761bb03cb039c2998c7f655b644224dd8b`.

Read the supplied raw finding texts for PR1 discussions 4289943058 and 4289943064, inspected the full tracked diff and new regression test, and checked relevant unchanged production dependencies.

## Findings addressed

### Floating-point absolute values

The generator qualifies bare `abs` calls as `std::abs`, preserving floating-point overloads while leaving already qualified names alone. The checked-in catalog is exactly its base content with 40 bare-call qualifications across 37 affected source lines; no other renderer change was found. Independent regeneration in a temporary directory produced byte-identical catalog C++ and header outputs.

The regression fixture executes the actual full catalog after introducing an integer-only fallback at the enclosing namespace's standard-library boundary. This deliberately removes macOS libc++'s otherwise misleading global floating overloads from unqualified lookup while allowing qualified `std::abs` to resolve normally. It is a credible model of the actual Xtensa overload issue; it does not substitute a reimplementation of Euler2d. Independently calculated the first-frame colors: `(12,0,139)` and `(12,40,138)`, giving RGB565 `0x0811` and `0x0951`, matching its assertions.

Separately ran the actual Xtensa ESP32-S3 compiler on a source-derived `std::abs(-0.5f)` floating-return static assertion and on the complete checked-in catalog translation unit. Both compiled successfully. The saved original-target red log documents that unqualified `abs(-0.5f)` fails the floating-return assertion on that compiler.

### Screensaver/menu restoration checker

The checker now allows exactly one Screensaver row immediately before Brightness, preserves every other official menu label and ordering requirement, and compares only the two deliberately changed screensaver view files against fixed revision 4a24491. Other view comparisons remain pinned to the original upstream baseline. The joint-companion factory expectation matches the current production registration and retains its single-instance and ordering checks.

The negative regressions execute the real checker against temporary controlled inputs and continue to reject both an unrelated renamed row and a duplicate Screensaver row. The changes do not broadly suppress view or menu assertions.

## Necessary fixture migrations

The documented stock runner exposed old enum-only audio API calls in two existing fixtures. Their replacements reserve and release typed in-memory leases with valid purposes and nonzero epochs, retaining mutual-exclusion and foreign/invalid-release rejection checks. No audio production code, hardware direction, worker or transport is changed or opened.

My first complete runner invocation independently reproduced the stale eight-slot Tools assertion after those compile fixes. The final fixture checks index 63 accepted/index 64 rejected, matching unchanged `NamedIrLibrary::MaxRecords=64`, `tools_ui.hpp::Allowed`, and `stock_apps.cc`'s actual consumer bound. This is an evidence-backed fixture correction rather than a production-bound expansion. The parent preserved the earlier failure receipts.

## Independent verification

- Four new focused regressions: **4/4 passed**, including checked-in and regenerated full catalogs under ASan/UBSan and menu positive/negative coverage.
- Final documented stock runner: **13/13 check groups passed**, exit status 0. Receipt: `/tmp/pr1-p2-independent-stock-receipt-final.json`; log: `/tmp/pr1-p2-independent-stock-final.log`.
- Actual Xtensa source-derived float-return probe and full catalog object compile: passed.
- Regenerated C++ and header byte equality: passed.

No full firmware link or new whole-image claim is needed or made for this review. No source edits, commits, publication, device IO, audio/microphone/motor activation, flash or reset were performed. Navigation candidate f483, original candidate 66b81c1 and separate mute fix ba73 were not modified. Generated untracked `manifest.json` and evidence files are outside the seven reviewed source/test files.
