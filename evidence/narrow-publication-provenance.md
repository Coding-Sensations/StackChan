# Narrowed navigation publication candidate

Base: published `mira-desk-integration` commit `4a24491761ffab19f3df758c93ebbdada4c29ddc`.
Branch: `codex/narrow-navigation`, isolated worktree `stackchan-narrow-navigation`.
Origin repair: preserved `66b81c1eba1633374423bbd7f2b4443ebc247ea7`, whose direct base was `832730492ab9ae68d693ce6c2e7b024aba3c8d82`.

## Source equivalence and exclusion

Applied only the origin repair using `git cherry-pick --no-commit`, with zero conflicts and no source edits. All 74 source/test hashes match the final reviewed origin manifest. The entire tracked firmware tree equals the preserved candidate. The host/bridge trees equal published 4a24491.

The following unrelated local commits are excluded: 51d0c05 (camera timestamp propagation), 5c8228d (finite acceptance owner), and 8327304 (inactive acceptance adapters). Their seven host paths are not included in this candidate. No dependency on those changes is demonstrated. None changes firmware; the firmware trees at 4a24491 and 8327304 are identical.

The 12 original evidence records are preserved as historical origin evidence. Six new narrowed-candidate provenance/review/build records supplement them; the final local commit contains 74 source/test files and 18 evidence records. Their branch/base identity and build hashes describe the origin candidate; `narrow-review-manifest.json` and the new narrow build receipt describe this candidate. The committed repair remains the same source semantics, including required authorization/consent, checked Home cleanup and exclusive Wi-Fi ownership.

## Validation and inherited build inputs

Fresh configure and full firmware compile/link passed from this isolated worktree with a separate build directory. The binary is 4,725,136 bytes, SHA256 `81b74d1a460e1303526c693d3eb782c893de9f758ac25428197615907010ce8c`; it fits the 0x4f0000 app partition with 9% remaining. This build has its own artifact identity; no byte-identical binary claim is made against the origin build. Host tests run in `/tmp/stackchan-narrow-host`; all 13 passed with no skips. The same SDK, reviewed dependency cache and sdkconfig bytes are used, without activating device features.

Fresh configure exposed 21 inherited ESP-NN assembly files absent from both published 4a24491 and tracked origin66. The vendored `.gitignore` pattern `*.s` also excludes `.S` on this case-insensitive filesystem. Unchanged files were restored only as local build inputs from the preserved build; all 21 match the committed `firmware/components/mira_wake/SOURCE-PINS.json`. No assembly files or unrelated source repair are added here. Consequently, build reproducibility requires these pinned dependency inputs; a bare clone alone is insufficient until that inherited packaging issue is addressed separately. This is not a dependency on the excluded host commits.

## Published base review status

PR #1 remains open; the Codex review at 4a24491 is COMMENTED, not approval. Current threads are unresolved:

- P2, generated effect catalog: floating-point animation values must use the floating overload (`std::abs`) rather than integer truncation. https://github.com/mrkillbob/StackChan/pull/1#discussion_r4289943058
- P2, `companion/host/boot-tests/verify_stock_menu_hierarchy.py`: the setup-menu assertion omits Screensaver and prevents the documented stock-restoration runner from reaching later checks. https://github.com/mrkillbob/StackChan/pull/1#discussion_r4289943064

These are identified as inherited PR review findings; no new unrelated implementation or remote thread resolution was performed during narrowing. Earlier repair source remains unchanged, including its existing catalog normalization.

## Limits and preservation

No push, PR, merge, flash, reset, device settings change, microphone/audio/motor activation or device stress test occurred. The installed image and physical Home/preview/Wi-Fi behavior remain unverified. Historical October15 readback is not today's deployment proof. Screensaver removal awaits identified deselected names/photos or the saved playlist; no assets were removed.

The preserved origin branch remains at66b81c1. The separate mute fix remains clean and unchanged at ba73fd6baf1bb394c286a9919b22c7b9f2fffa2b. A future publication would use this narrowed branch against `mira-desk-integration`, not main, and would disclose that PR1 is an unmerged, incompletely cleared dependency. No publication is authorized or performed here.
