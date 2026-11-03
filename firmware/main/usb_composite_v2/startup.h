#pragma once
#include "tinyusb_bridge.h"
#include "qualified_boot_entry.hpp"
namespace usb_composite_v2 {
// Exactly one serialized boot caller before companion/console owner creation.
// Legacy remains default. No persistence or automatic recovery-to-composite selection.
bool StartCompositeAtBoot(Integration binding,uint16_t vid,uint16_t pid,bool physical_qualification);
BootEntryResult StartQualifiedCompositeAtBoot(const BootQualification&);
bool CompositeRequested();TinyUsbBridge* CompositeBridge();
bool CloseComposite();
}
