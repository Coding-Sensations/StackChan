# Tools and selectable agents

The Tools screen uses full task pages at 320×240: Infrared → Learn, Review & Send or Saved remotes; NFC URL → Edit or Emulate; UART Terminal → explicitly labeled local demo and line editor. Touch targets are at least 44 pixels tall. IR capture lasts ten seconds; transmission is one explicit action after review. Up to eight saved remotes, the learned signal and URL stay in RAM and clear when Tools closes. SD storage and Grove UART hardware adapters are unavailable. NFC emulation lasts sixty seconds or until Stop/Back. Back completes resource shutdown before leaving. See `TOOLS-EXTRAS-OFFLINE.md` for limits, recovered decisions and remaining hardware qualification.

Open `tools-preview.html` for an interactive design simulation. It does not operate hardware; it is not an actual device screenshot. Portable checks cover target bounds, editor key layout, navigation scope, and mutually exclusive audio ownership.

AI.AGENT opens a stopped preview. Start activates the original vendor network, activation, wake-word and audio flow. The identity bar stays visible and Stop / Home restarts into the stopped launcher because the vendor runtime has no complete joined teardown. If Companion or catalog browsing has initialized shared Wi-Fi, the first Start restarts to a stopped Stock preview; Start is required again. Camera and motion remain disabled. Existing persistent settings are preserved; protocol and newly provisioned Wi-Fi settings are session-only. Firmware and asset download/update paths are disabled.

Mira Companion remains a separate selectable app. Its existing trusted-host grant and physical conversation consent remain required. Neither selecting Mira nor selecting Stock grants the other audio ownership. The current Mac route/native Mira stream remains unqualified; this source does not establish a working connection to the existing Mira conversation.

App Center opens actual installed apps and, after an explicit Fetch, browses the official vendor catalog using saved Wi-Fi. The official catalog contains whole replacement firmware, not installable plugins. This adapter does not install it. Metadata is treated as untrusted and bounded; there are no credential-bearing catalog requests or remote-icon loads.

## Verification

From the repository root:

```sh
clang++ -std=c++17 -Wall -Wextra -Werror -Ifirmware/main/apps/stock_shell -Ifirmware/main/companion companion/host/boot-tests/test_tools_ui.cpp -o /tmp/stackchan-tools-test
/tmp/stackchan-tools-test
clang++ -std=c++17 -Icompanion/host/boot-tests -Ifirmware/xiaozhi-esp32/main -Ifirmware/main companion/host/boot-tests/test_readonly.cpp firmware/main/companion/readonly_settings.cc -o /tmp/stackchan-readonly-test
/tmp/stackchan-readonly-test
```

Use the pinned SDK and setup instructions in the project README for firmware builds. These host checks and a successful compile do not prove physical IR/NFC, pairing, audio, or vendor service behavior. Flash and device testing require a reviewed image and fresh download-mode coordination.
