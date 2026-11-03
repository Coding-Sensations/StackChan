# Installed-source reconciliation — 2026-10-22

The installed-image source is `7695fd98c3b7c4aeedcb630cce76c0e972737268`.
It descends from the previously recorded PR3 head
`a98b760110fbfe65fba66b53229382cf8ae2dc5e` through three commits:

- `4aac74a`: remove Moire with a stable-ID tombstone; retain Moire2.
- `3ed4d5f`: reuse bottom-swipe Home in Mira and Stackchan; remove the added footer.
- `7695fd9`: update stock-restoration checks for the reviewed Home extension.

The saved image is 4,728,720 bytes, SHA-256
`dc5c0b39926c3b0b6f97ba81f8ca6b19485dc3c7e93ea67b1a62f8e176cafd27`.
That digest was recomputed on October 22 and matches the October 19 saved
app-only flash/readback receipt. This is historical deployment evidence;
no fresh device readback or physical acceptance was performed.

The October 22 source checkout ran all 21 CTest host checks successfully,
without skips, after preparing the pinned SDK/vendor caches. Its initially
empty cache exposed two failures and two skipped tests, so dependency-free
CTest success must not be claimed. The sibling-worktree fallback in the nested
USB fixture was removed; the documented in-checkout dependency is required.
The initial reconciliation commit `fdac723` changes test/documentation only;
its firmware implementation is byte-for-byte identical to the installed-image source.

Physical face, Home touch/swipe, Moire/Moire2, voice, reconnect and device
acceptance remain unrun. No flash, enrollment, trusted-device profile, firmware
security flags or gateway process was changed for this reconciliation.

Publication is prepared locally. The recorded PR identity is historical;
current GitHub head/CI could not be read because API connectivity was unavailable.
No push or PR update is claimed, and no merge is authorized by this document.

## Source-only callback lifetime correction after publication

PR3's unresolved P1 review identified a real `stopEspNow()` ordering defect:
station callbacks could still borrow the netif/event group when teardown freed
them. The follow-up drains the default event loop after unregistering handlers
and stopping Wi-Fi, before destroying either borrowed object. A failed drain
retains both objects for the next checked-close attempt.

The new ASan/UBSan fixture executes the production method. It fails on the old
method when destruction occurs with queued callbacks, and passes the corrected
method, including timeout retention, retry, Wi-Fi stop failure and repeated stop.
All 22 firmware host CTests pass without skips. The new candidate therefore
contains a firmware implementation correction beyond the installed image.
No new binary is installed or claimed to match the saved installed bytes.

A full ESP-IDF configure/build was attempted with local cached dependencies.
Configure was blocked when idf_component_manager's psutil process lookup called
sandbox-denied sysctl (`PermissionError: Operation not permitted`). It was not
retried through a different access route. No full firmware build pass is claimed
for this additional source fix. Hosted CI and final exact-head review remain
separate merge prerequisites.
