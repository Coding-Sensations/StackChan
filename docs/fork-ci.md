# Fork CI scope

`StackChan CI` runs for PRs to every base and for pushes to `main`,
`mira-desk-integration` `claude/todo20-companion-lifecycle`, `codex/w1-consent-revocation` and
`codex/combined-navigation-p2`. The reconciliation preserves H01, installed-source
and the earlier fork-CI ancestry. Check each stacked PR base and dependency order;
leaf merges alone do not integrate all source into `main`.

The `All required checks pass` job requires host firmware tests, the ESP32-S3
firmware build, Python, Swift, macOS app and full-history secret-scan lanes to
succeed, including on documentation changes. Failed, skipped or cancelled lanes
fail the aggregate. There are no path filters that strand a required check.
The check becomes a merge gate only after a separately approved GitHub ruleset
requires it. GitHub Actions' app integration ID is 15368.

The firmware lane prepares 16 exact vendor source files from immutable GitHub
revisions, applies only the existing reviewed application patch, and verifies
every resulting file against the generator SHA256 pins. No complete SDK, local
cache or hardware setup is required. Different existing files or symlinked
destinations are rejected. This subset is for host tests, not a firmware build.
CTest skips fail the job.

The seven additional host fixtures cover: advanced
configuration, firmware install policy, DMA completion fencing, camera authority,
camera wire lifecycle, screensaver selection policy and the actual generated
catalog renderer. Existing motion/navigation tests remain registered. Together
with the installed callback-lifetime regression and navigation checks this is 29 CTests. Assertions stay enabled in Release too.

The Python lane runs the standalone numeric worker fixtures with synthetic pins,
public trust/resource models and fixture-custody models. The full streaming suite
requires private HomeLab source and stays in its separately approved trusted CI.
The macOS lane compiles eight Swift fixtures and runs five pure models; process
lifecycle runtime fixtures remain a separate qualification.

The firmware lane also builds the actual ESP32-S3 image in a digest-pinned
ESP-IDF container without device access; the app lane builds the actual unsigned
macOS Companion. These lanes do not build Android/iOS or the Go service. The Flutter app's
SDK constraint is Dart ^3.11.5. A proposed Flutter 3.47.7 tag exists, but the
previous proposal's Dart 3.13.5 pairing and hosted Android build have not been
qualified here. Add those jobs after reproducing dependency resolution, analysis
and debug signing/build on a clean SDK. Do not turn an unverified app lane into
a required gate or copy Hermes-specific jobs.

The correction's actual playback/preview consumer fixture exercises all 215
selectable effects (the Moire stable-ID tombstone remains unavailable) at 0, 1,000 and 29,999 ms with a synthetic ESP-clock boundary.
This candidate additionally verifies playback time advances and Gears frames
change through both consumers. Existing bounded-clock/catalog tests remain.
These checks catch the clock rejection regression and a visible frozen consumer;
they do not measure real ESP32 frame latency or certify the reported device freeze.

Review policy is currently manual. A Codex summary that names the latest commit,
its no-unresolved-findings state and a bot-authored reaction establish different
parts of review provenance. A reaction by itself is not a SHA-bound approval.
Smart Detect's private integration setting cannot be established by repository
source or a PR reaction. Verify it in the Codex integration settings before
claiming parity. Do not manufacture bot approval with labels or reactions.

Upstream sync is a reviewed branch/PR operation (see CONTRIBUTING.md), not an
unattended scheduled merge. This workflow has no secrets, publishing, self-hosted
runner or write permissions. Protection, Actions policy and review integration
settings are separate administrative decisions.

The secret scan includes ordinary commits and resolved merge-result changes with
`--all --full-history --remerge-diff`. Synthetic merge-only and same-location new
key canaries detect additions that the default non-merge scan misses. Unsupported
merge topology must fail closed. The existing 17 exact historical false-positive
fingerprints remain unchanged; no broad ignore or new exception is introduced.
