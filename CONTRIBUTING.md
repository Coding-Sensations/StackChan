# Contributing to this fork

The upstream project is [m5stack/StackChan](https://github.com/m5stack/StackChan).
This fork's default branch is `main`. Desk companion development currently targets
`mira-desk-integration`; check each PR's base before opening or retargeting it.
Keep firmware repair, navigation and CI changes in separate branches/worktrees.

Before proposing a change, run the relevant offline checks:

```sh
python3 scripts/ci/prepare_host_dependencies.py
cmake -S firmware/tests -B /tmp/stackchan-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/stackchan-tests --parallel 2
ctest --test-dir /tmp/stackchan-tests --output-on-failure --no-tests=error
python3 -m compileall -q companion/host
python3 -m unittest discover -s companion/host/offline-public -v
python3 companion/host/numeric-ui/script/test_fixture_custody.py
# macOS with Xcode command-line tools:
STACKCHAN_OFFLINE_BUILD_ROOT=/tmp/stackchan-offline bash companion/host/numeric-ui/script/test_offline.sh --compile-only
/tmp/stackchan-offline/AudioEndpointDirectionChecks
/tmp/stackchan-offline/StatusChecks
/tmp/stackchan-offline/RememberedDeviceChecks
/tmp/stackchan-offline/MiraLifecycleChecks
/tmp/stackchan-offline/W1LifecycleChecks
```

CI compiles all eight Swift fixtures but executes the five pure model fixtures
above. Process lifecycle runtime qualification is separate; use the documented
bounded fixture runner after reviewing its prerequisites. See
[companion qualification](companion/host/numeric-ui/README.md).

Include the tested commit, results and remaining coverage limits in the PR.
Offline tests do not qualify a device image, physical clock timing, audio
capture/route restoration or physical camera cleanup. Changes to screensavers
need advancing-frame coverage at real elapsed times and the production clock
path, in addition to catalog/policy fixtures. ESP-IDF builds and device flashing
are separate operations. Never add real pairing bindings, private keys or tokens.

CI uses read-only tokens and disposable GitHub-hosted Ubuntu/macOS runners.
Do not run public fork PR code on a HomeLab/self-hosted runner or introduce
`pull_request_target` with PR code checkout. Workflow changes deserve explicit
maintainer inspection even when CI is green.

Review the latest head commit. A Codex bot comment/summary must identify that
commit; a thumbs-up reaction alone has no commit binding and can become stale.
Resolve findings and obtain a fresh review after code changes. Automated review
is evidence, not a substitute for required CI or a maintainer's merge decision.

For upstream updates, fetch without changing the working branch:

```sh
git remote add upstream https://github.com/m5stack/StackChan.git # once
git fetch upstream main
git log --oneline HEAD..upstream/main
```

Review upstream changes in a separate sync branch/PR. Do not reset or force-push
integration branches or automatically merge upstream into custom firmware.
