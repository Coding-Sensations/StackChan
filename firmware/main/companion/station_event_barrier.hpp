#pragma once
#include <cstdint>
namespace companion::network {
// Retained stop-worker only, never an event-loop callback or LVGL owner.
// A timeout retains station custody. The acknowledgement owner is boot-lifetime.
bool DrainStationEvents(uint32_t timeout_ms=100);
}
