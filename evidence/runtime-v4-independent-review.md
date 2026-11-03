# Independent runtime review — v4 BLE delta

Outcome: no blocking regression found in the narrow v3-to-v4 delta. This supplements, and does not replace, the preserved v2 and v3 independent reviews.

Reviewed identity:

- Branch: `codex/current-navigation`
- Base: `832730492ab9ae68d693ce6c2e7b024aba3c8d82`
- Manifest: `evidence/runtime-review-v4-manifest.json`
- Manifest SHA256: `9cefb9d8b8cc72b180465aed8310107a8279fd9392bbce7896f8f7ebe1a99c93`
- Tracked patch SHA256: `16f63b3243d4df3c15ab3bf0e2b1e238a37f368ee465b12b714665d8f070179b`
- All 60 manifested source/test files matched their recorded hashes.

## Scope and source evidence

The manifest comparison against v3 changed only `firmware/main/hal/utils/bleprph/bleprph.c`, `firmware/tests/CMakeLists.txt`, and the new `firmware/tests/navigation/ble_stop_test.py`.

I independently read the pinned SDK implementation at `/Users/mikedemott/Developer/AgentWork/StackChan/.tools/sdk/esp-idf-v5.5.4/components/bt/host/nimble/nimble/porting/npl/freertos/src/nimble_port_freertos.c`. Its `esp_nimble_disable()` calls `vTaskDelete(host_task_h)` before setting `host_task_h = NULL`; `nimble_port_freertos_deinit()` delegates to it. Thus calling deinit from the host task itself can prevent the global-handle clearing statement from executing.

The revised host task publishes completion only after `nimble_port_run()` returns and then only parks itself. It performs no further NimBLE or callback-owner access. The external stop owner waits for that receipt, calls `nimble_port_freertos_deinit()` so task deletion returns to the caller and clears the SDK handle, then deinitializes the NimBLE port and revokes callbacks. A deletion between receipt publication and the first self-suspend is also safe because the publisher has no remaining stack-owner accesses. The runtime keeps ownership until this cleanup completes, preventing a replacement host from starting in this interval. Repeated stop remains harmless after the initialized flag is cleared.

## Independent verification

Ran `ctest --test-dir /tmp/stackchan-navigation-host --output-on-failure`: **9/9 passed**, with no skips (2.24 seconds). This includes the new BLE fixture, which extracts the production host-task and stop bodies and exercises 50 park/external-delete/deinit/callback-revocation cycles under ASan/UBSan, plus repeated stop.

The fixture is meaningful evidence of the actual source ordering and complements direct inspection of the pinned SDK. It does not execute the physical BLE controller or actual FreeRTOS scheduling. The final full firmware link was still running when this review was recorded; no final-link or hardware-validation claim is made here.

## Remaining limits

The previously documented saved-Mira network ownership handoff remains an explicit architecture limitation, with actionable inline state rather than an unsafe takeover. No new blocker was found in this BLE delta. No production source was edited, and no device action, flash, reset, settings change, or publication was performed for this review.
