/* Source-only StackChan candidate; original component retains ESPRESSIF MIT. */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define ESP_VIDEO_CAPTURE_CLOCK_CONTRACT_REVISION 1
#define ESP_VIDEO_CAPTURE_CLOCK_DOMAIN "esp-monotonic-ms-v1"
/* DVP EOF/DMA transfer completion in esp_timer boot monotonic microseconds.
 * This is NOT exposure-start time and does not qualify physical shutdown.
 * The selected installed driver must export this API before opting in. */
uint32_t esp_video_capture_clock_contract_revision(void);
#ifdef __cplusplus
}
#endif
