# PR1 P2 source repair

Verified live PR1 head twice: `4a24491761ffab19f3df758c93ebbdada4c29ddc` on `mira-desk-integration`; main base `1b5765599fba8aaad1811d9a79358ccc7051f5f3`. This separate worktree/branch is `stackchan-pr1-p2-fixes` / `codex/pr1-p2-fixes`. No original candidate was rewritten.

## Verified findings and changes

1. Review comment4289943058: generated catalog unqualified floating-point `abs` resolves to integer abs under the actual ESP32-S3 libstdc++ compiler. The Mac host's libc++ supplies a floating overload, so initial host pixel tests alone did not reproduce this target bug. A real target compiler return-type probe failed, confirming truncation. The portable ASan/UBSan regression models the verified integer fallback at the standard-library boundary, executes the actual full renderer, and checks hand-derived Euler2d RGB565 pixels0811/0951. Both checked-in and regenerated catalog initially returned0011/0011. The generator now qualifies40 bare calls across37 source lines as `std::abs`, retaining integer as well as floating overloads. No raw upstream source, slide IDs, exclusions or assets changed.

2. Review comment4289943064: the stock-menu assertion rejects the intentionally added Screensaver row. The checker now admits exactly one Screensaver immediately before Brightness while preserving all upstream labels/order. Running beyond that assertion exposed the same checker's stale original-byte comparisons for the two intentionally extended screensaver views and its former Companion constructor expectation. The checker pins those two files to the exact PR1 integration base, keeps every other upstream view comparison, and checks the actual single joint Companion factory. Negative fixtures still reject an unrelated or duplicate menu row. No production menu or registration source was changed.

The new four-case regression suite is part of the documented stock-restoration runner. Once unblocked, that full runner exposed two inherited test-only users of the removed enum-only audio ownership API (`test_readonly.cpp` and `test_tools_ui.cpp`). Both now use current typed lease/purpose/epoch acquisition and checked release with in-memory-only custody; mutual exclusion and foreign-release rejection remain tested. The tools fixture also had an old eight-slot remote boundary; its63/64 boundary now matches the existing64-record named IR library and actual consumer. These changes repair stale verification fixtures, with no production audio/UI behavior or new feature.

## Validation

- Saved failing receipts: initial host menu failure; actual target integer-overload assertion; portable target-boundary renderer/menu regressions (three failures); inherited fixture compile failures and stale tools boundary. No failure is hidden.
- Four focused regression cases pass under ASan/UBSan renderer execution. Negative menu mutations continue to fail as expected.
- Full documented `python3 companion/host/boot-tests/run_stock_restoration_checks.py` passes all13 check groups, including the new regressions and11 C++ fixture groups. The runner uses inert NVS/SDK stand-ins and no device/service actions.
- Actual ESP32-S3 GCC compiles both a source-derived floating-return assertion and the entire regenerated catalog translation unit. This is target compilation, not execution or a full firmware link.
- Existing firmware host CTest:1/1 motion test passes. Catalog regeneration is checked independently by the new generator test.
- Exact seven source/test hashes are in `review-manifest.json`; independent review is recorded separately.

## Preservation and limits

No publication, GitHub comment/thread resolution, merge, flash, reset, device settings change, microphone/audio/motor activation or device stress test occurred. PR1 remains open with its remote review threads unchanged. Local source fixes do not constitute remote approval or physical acceptance. No new complete firmware image was built for this P2 repair; current installed-image identity and physical behavior remain unverified.

Preserved navigation candidate `f48377d40178c263186d067f47deb0daf49fa330`, origin `66b81c1eba1633374423bbd7f2b4443ebc247ea7` and separate mute fix `ba73fd6baf1bb394c286a9919b22c7b9f2fffa2b` remain unchanged. These source repairs have not been merged with either candidate and combined-tree validation remains separate. Screensaver names/removals remain pending, and no assets were removed.

Provenance correction: the earlier narrowed-candidate note's phrase “including its existing catalog normalization” was inaccurate. That preserved candidate changed rendering budgets, not abs normalization; this separate repair supplies the abs fix. The preserved commit is untouched and its historical record is not silently rewritten.
