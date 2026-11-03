# Combined navigation and PR1 P2 candidate

Published base: `4a24491761ffab19f3df758c93ebbdada4c29ddc`. Branch/worktree: `codex/combined-navigation-p2` / `stackchan-combined-navigation-p2`.

Applied reviewed navigation `f48377d40178c263186d067f47deb0daf49fa330` and reviewed P2 repair `f558a5deaa3b2532764a83e7096841b0ccc2f7ac` without committing either separately. Both existing branches, origin 66b81c1, and the separate mute fix remain unchanged. No history rewrite or branch merge occurred; the final combined commit is based directly on the published base, excluding the three unrelated local host commits.

## Overlap and semantic resolution

The only production overlap is the catalog C++ and generator. Git auto-merged both without textual conflicts. The combined renderer is exactly the navigation renderer with the 40 P2 bare-abs qualifications across 37 lines. The generator adds only P2 normalization to the navigation version. Independent regeneration produces byte-identical C++ and header outputs, preserving work limits, clock checks, reset behavior and float magnitudes.

The combined P2 menu regression failed before adaptation because its old AI.Agent and view expectations conflict with the intended navigation source. Only two verification files were adapted: the menu checker now expects Stackchan, retains exactly one Screensaver row, and pins the three reviewed navigation lifecycle/view files by their exact SHA256. This avoids requiring an unpublished candidate Git object in a future clone. The existing integration view header still references the published 4a ancestor, and unrelated upstream view/order/Tools constraints remain intact. Negative tests reject unrelated/duplicate rows, reverting the Stackchan label, and reverting the screensaver implementation to its pre-navigation source. No additional production code or features were introduced during this resolution.

All other source/test files match their respective reviewed origins byte for byte. The final 79-file manifest records actual combined content. Historical candidate receipts are retained as historical evidence and do not substitute for combined validation.

## Validation and build inputs

Both parent and independent reviewer passed all 13 navigation host CTests without skips and all 13 stock-restoration check groups, including the four focused P2 renderer/generator/menu cases. Renderer and lifecycle production-body fixtures use ASan/UBSan where recorded in their suites. The earlier semantic failure is retained in `semantic-red.log`. Independent review found no blocking integration regression.

A fresh full ESP32-S3 configure/compile/link uses a separate build directory, the same reviewed SDK/dependency cache and unchanged sdkconfig. The fresh full configure, compile and link passed. The binary is 4,725,152 bytes, SHA256 `9d0aa3d6e813097bc245499e0cd11c7f9a9457d322a8eb91cada1bfb882b8d1c`, with 452,192 bytes remaining in the app partition. Artifact hashes and full-log hashes are recorded in `build-receipt.json`. The 21 inherited ESP-NN assembly inputs are still local pinned dependencies, all matching committed SOURCE-PINS; no asset or source was guessed or added to bypass that provenance. A bare checkout alone is not claimed to contain all build dependencies.

## Limits

No push, PR, remote review-thread change, merge, flash, reset, hardware activation, microphone/audio/motor use, device settings change, asset removal, permission change or device stress test occurred. Build/test subprocesses are local and inert with respect to the device. Historical October 15 readback is not current-device proof. Current installed firmware identity and physical Home, preview/freeze, Wi-Fi handoff/reconnect and teardown behavior remain unverified. Required setup, authentication and physical consent checks remain in the reviewed source.

Screensaver removal remains pending identified deselected names/photos or the saved playlist. The separate mute fix has not been incorporated into this candidate. Physical deployment/acceptance and any further source integration remain separate authorized work.
