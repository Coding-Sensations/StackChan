#pragma once
#include <esp_lcd_panel_io.h>
#include "sd_status.hpp"
namespace stock_tools {
// Cached observation only: no SD transaction, remount, write or card-presence probe.
SdDiagnostic ReadStackChanSdDiagnostic();
const char* StackChanSdUnavailableText();
// Board-owned boot initialization. Existing SPI3 bus must have MISO GPIO35,
// MOSI37/SCK36; call before LCD initialization. No card formatting is performed.
bool MountStackChanIrSd();
// Bind after the display has been initialized. LVGL must own all later LCD IO.
void BindStackChanIrLcd(esp_lcd_panel_io_handle_t io);
} // namespace stock_tools
