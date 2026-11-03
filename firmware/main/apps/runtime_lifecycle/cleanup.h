#pragma once
#include <mooncake.h>
namespace runtime_lifecycle {
// Poll from the owner thread with no LVGL lock. Worker owns the old extension
// manager until completion; a failed/pending stop never permits UI destruction.
bool PollSourcesStopped(mooncake::Mooncake* shell);
void ResetCleanupJob();
bool VendorNetworkAvailable();
bool PrepareVendorNetwork();
void ClearNativeSignals();
void QuietHardware();
void ResetSessionUi();
}
