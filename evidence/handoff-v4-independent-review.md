# Independent saved/vendor handoff review — v4

Outcome: no remaining blocking source regression found in the reviewed handoff delta. The queued-event P2 from v3 is addressed; prior review reports are preserved.

Reviewed `evidence/handoff-review-v4-manifest.json`, SHA256 `9a8a7380506d4d48e9bf81037adf40b95de0e53409dab950bbb2a73543c7340f`, with 74 manifested source/test files and 26 delta files against runtime v4. Base `832730492ab9ae68d693ce6c2e7b024aba3c8d82`; branch `codex/current-navigation`. All 74 hashes matched at the initial independent check.

## Queued-event repair

The saved and phone station adapters unregister their owned callbacks, stop the radio and verify NULL mode, drain default-loop events, destroy the old netif, then drain again before releasing resources/custody. The vendor cleanup drains after its adapters stop, netifs are torn down and the driver is checked NULL. This prevents the old queued events identified in v3 from reaching a replacement owner's newly registered handlers.

The drain handler, acknowledgement counter and mutex have boot lifetime, so publication of the acknowledgement does not permit a callback-owner allocation to be freed while the event handler returns. Serializing drain requests with the mutex and relying on FIFO default-loop dispatch keeps old delayed acknowledgements from satisfying later requests. Posting failures and acknowledgement timeouts return false; the calling stop paths retain authority and retry. A failed post may skip a ticket, but a later acknowledged ticket still proves consumption of all earlier queued events.

The saved GOT_IP callback also checks event netif identity and current AP association. This is useful validation in addition to the queue drain.

## No-default-loop rule

Read the actual pinned `components/esp_event/default_event_loop.c`: its default register and post wrappers return `ESP_ERR_INVALID_STATE` when the default loop is absent. The production fallback accepts that condition only if `esp_wifi_get_mode()` also returns `ESP_ERR_WIFI_NOT_INIT`. Other failures and absent-loop cases with an initialized driver remain failed cleanup. No default-loop deletion path was found in the application sources, consistent with the boot-lifetime drain registration.

## Independent verification

- `ctest --test-dir /tmp/stackchan-navigation-host --output-on-failure`: **13/13 passed**, no skips, 3.49 seconds.
- The new fixture executes production barrier code with dispatch-time handler resolution, 100 delayed old-GOT_IP transitions, delayed acknowledgement and failed posting, under ASan/UBSan.
- Independently extended the fixture in memory, without editing its saved source, to inject absent-loop errors from both registration and posting. Both paths reject an initialized driver and accept only an uninitialized driver. This additional ASan/UBSan run passed.

Tests use inert SDK stand-ins; they do not certify the physical controller, actual scheduling or final firmware link. No production source edits, device actions, settings changes, flash, reset or publication were performed for this review.
