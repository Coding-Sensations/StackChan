// SPDX-License-Identifier: Apache-2.0
// Optional ABI boundary only. Compile explicitly in Muse-off after source filtering.
#ifndef STACKCHAN_BUILD_MUSE
#define STACKCHAN_BUILD_MUSE 1
#endif
#if !STACKCHAN_BUILD_MUSE
#include <companion/muse_direct_device.h>
#include <companion/muse_direct_live_view.h>
#include <companion/muse_room_transport.h>
namespace companion::muse {
struct SessionGrant;
void DeviceProviderVisible(bool){}
bool PrepareDevicePairing(const PairingGrant&,std::string_view){return false;}
bool DevicePairingPrepared(){return false;}
bool StartPreparedDevicePairing(){return false;}
uint32_t DevicePhysicalTicket(){return 0;}
bool QueueDevicePhysicalPress(uint32_t){return false;}
void StopDevicePairing(){}
bool DevicePairingCleanupComplete(){return true;}
void SessionWorkerProviderVisible(bool){}
bool PrepareNativeSession(const SessionGrant&){return false;}
bool NativeSessionPrepared(){return false;}
bool StartPreparedNativeSession(){return false;}
bool QueueNativeText(const std::string&,const std::string&,const std::string&){return false;}
SessionSnapshot ViewNativeSession(){return {};}
void StopNativeSession(){}
bool NativeSessionCleanupComplete(){return true;}
}
namespace companion::room {
bool StartMuseRoom(NetworkInterface&,const MuseRoomGrant&){return false;}
bool PrepareMuseRoom(NetworkInterface&,const MuseRoomGrant&){return false;}
bool MuseRoomPrepared(){return false;}
bool ConnectPreparedMuseRoom(){return false;}
void PollMuseRoom(){}
void StopMuseRoom(){}
bool MuseRoomAuthenticated(){return false;}
bool MuseRoomCleanupComplete(){return true;}
}
#endif
