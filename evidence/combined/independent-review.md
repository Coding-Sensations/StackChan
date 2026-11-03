# Independent combined navigation/P2 integration review

Outcome: no blocking integration regression found in the manifested working-tree snapshot. The renderer overlap preserves the reviewed navigation behavior and the P2 absolute-value repair. The two checker adaptations retain meaningful compatibility and negative checks.

## Exact reviewed identity

- Worktree: `/Users/mikedemott/Developer/AgentWork/Codex/2026-10-19/task-9/stackchan-combined-navigation-p2`
- Branch: `codex/combined-navigation-p2`
- HEAD/base: `4a24491761ffab19f3df758c93ebbdada4c29ddc`
- Navigation candidate: `f48377d40178c263186d067f47deb0daf49fa330`
- P2 candidate: `f558a5deaa3b2532764a83e7096841b0ccc2f7ac`
- Manifest: `evidence/combined/review-manifest.json`
- Manifest SHA256: `f4c29386e42e947eda8101c69594447ddccf78e132f30a39fca1972ef5a52719`
- All 79 source/test hashes matched, including at the final recheck.

The two semantic checker adaptations were unstaged when inspected. This review covers the exact manifested working-tree bytes, not the older index versions of those files; any future commit must stage the reviewed versions. No source or index mutation was performed by this reviewer.

## Renderer overlap

Independently proved that the combined catalog C++ is exactly navigation candidate f483's C++ with 40 bare `abs` call qualifications to `std::abs`. Relative to f483, the combined generator adds only the reviewed normalization rule and its explanatory comment. No unexpected changes to other manifested navigation source/test files were found. Nonoverlapping P2 files also match their reviewed P2 candidate; the two checker adaptations are explicitly accounted for.

Regenerated the complete catalog in a temporary directory: both C++ and header outputs are byte-identical to the combined checked-in versions. The combined P2 tests execute both checked-in and regenerated full catalogs with the target-like integer-only fallback, retaining the hand-derived Euler pixel assertions. The navigation catalog bounds/canary tests run against the combined renderer, so the restored floating-point calculations are covered together with the bounded renderer.

## Checker adaptations

The expected menu retains exact ordering, the one Screensaver row before Brightness, and only the requested `AI.Agent` -> `Stackchan` label change. The three inline screensaver/toast SHA256 pins match both the combined files and the prior independently reviewed navigation manifest. `view.h` remains pinned to published ancestor 4a24491, while other upstream view comparisons remain exact. This requires no unpublished f483 Git object in a fresh clone: the checker reads only the published ancestor/upstream revisions and inline byte hashes.

The prior toast suffix-stripping exception is replaced by a stronger exact reviewed-file hash. Joint-companion single-factory, Tools single-instance, native registration order, dedicated-page membership and no-direct-tool-install checks remain. The negative fixture now rejects an unrelated row, duplicate Screensaver row, restoration of the old agent label, and reversion of the reviewed screensaver source to 4a24491. These changes adapt constraints to intended integration behavior rather than broadly disabling assertions.

## Independent verification

- `ctest --test-dir /tmp/stackchan-combined-host --output-on-failure`: **13/13 passed**, no skips, 3.46 seconds.
- Documented stock runner on the combined working tree: **13/13 check groups passed**, exit status 0, including all four P2 regression tests. Receipt: `/tmp/combined-independent-stock-receipt.json`; log: `/tmp/combined-independent-stock.log`.
- Full catalog regeneration byte equality: passed.
- All 21 inherited `.S` assembly inputs in this local build match their `SOURCE-PINS.json` entries.

## Limits

The full combined firmware build was still running when this review was recorded; no successful link or whole-image claim is made. The known inherited reproducibility limit remains: the 21 restored assembly inputs are not included in the tracked base/candidate trees. Prior candidate evidence files remain historical receipts and do not certify the combined build. Physical Home behavior, real shutdown timing and device freeze symptoms require separately authorized device validation.

No production source edits, index changes, commits, publication, device IO, flash, reset, settings changes or hardware activation were performed. Existing navigation, P2, original and mute branches were not changed.
