# Independent source review

Reviewed base `832730492ab9ae68d693ce6c2e7b024aba3c8d82`, branch `codex/current-navigation`.

Exact final patch SHA256: `b3df6ae66efdbba852f421d196981374f02cc62d3b6da6799032bd8d22bb848f`.

Independently verified the patch hash and all 21 source/test hashes in `review-manifest.json`. Reviewed the full patch and relevant baseline implementations, including companion closure, direct USB actor/runtime ownership, launcher view teardown, settings scopes, and native runtime transfer.

## Result

No remaining blocking regression found in the final source diff. This is a source review, not approval to flash, publish, or claim the complete Home/device-freeze problem resolved.

- Companions is first and routes only Mira and Stackchan. USB Mira is nested under Mira rather than separately installed.
- USB entry waits for successful Mira/feature cleanup. USB return waits for `Runtime::Closed()`, which requires closed audio worker, closed TLS, and no USB driver custody, before reopening the native Mira view. Pair-code/stamp verification and explicit Match + Listen remain intact.
- Stackchan configuration gating directs an unconfigured user to Settings. Setup is explicitly reachable there; the patch removes automatic Welcome setup after panic/watchdog boot and preserves the setup preview/consent boundary.
- Launcher helpers are retired before new shared helpers are initialized. Category state resets on launcher open, not on each running frame.
- Raster work checks use clipped scanline spans rather than per-pixel checks for fills. The generator and runtime API agree. Deadline/work exhaustion stops repeat rendering; the failed preview canvas stays hidden and the failure notice survives selection/order changes until navigation restarts preview.

Earlier review observations about category reset in `onLauncherRunning` and failure notices disappearing after Select/Order are repaired in this final fingerprint.

## Independent checks

Re-ran `ctest --test-dir /tmp/stackchan-navigation-host --output-on-failure`: **5/5 passed**. This includes the actual launcher method bodies executed for ten open/category/app/close/Home cycles under ASan/UBSan, plus category, raster, catalog, and existing motion checks. The catalog test exercised 2,592 frames with framebuffer canaries; the earlier independent run reported zero work-limited frames. Inspected `target-check.json`: ten target object compilations report exit zero. Object compilation is not a successful full firmware link.

## Remaining limits

- Cloud/native feature Home paths still intentionally restart because their owners lack joined teardown. They are an outstanding user-requested behavior, not fixed by this patch.
- No device reset/panic evidence was gathered. Whole-device freezes cannot yet be classified as watchdog/panic, driver lockup, or another cause.
- The 12 ms clock check is a raster checkpoint cutoff, not a hard end-to-end frame deadline: bounded effect-level math continues after drawing exhausts its budget. The patch should not be described as a measured rendering speed improvement; the implementer's latest host comparison reports approximately 20% aggregate overhead for budget enforcement.
- The saved device playlist/deselection names remain unavailable; this review does not authorize guessing removals.
- USB child lifecycle was reviewed statically; it has no new dedicated executable integration fixture. No hardware, audio, microphone, motor, reset, flash, publication, or device settings action was performed by this reviewer.

No access denials or uncertain external actions occurred. Only this review evidence file was written.
