// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
namespace companion::muse {
// Internal local-entry API. The caller owns three separate action-time grants,
// must not log token/input, and retains callbacks until CleanupComplete. No
// receiver, token lookup or durable SDK-token storage is implied by this API.
struct PairingGrant {
    std::string node,device,mac,version;
    uint64_t expires_ms=0;
    std::function<bool()> enrollment_current,network_current,storage_current;
};
void DeviceProviderVisible(bool visible);
bool PrepareDevicePairing(const PairingGrant&,std::string_view privately_entered_sdk_token);
bool DevicePairingPrepared();
bool StartPreparedDevicePairing();
uint32_t DevicePhysicalTicket();
bool QueueDevicePhysicalPress(uint32_t captured);
void StopDevicePairing();
bool DevicePairingCleanupComplete();
}
