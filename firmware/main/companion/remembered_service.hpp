#pragma once
#include "remembered_vault.hpp"
namespace companion {
// Trusted local firmware configuration entry only. Compile-time opt-in remains
// OFF. No network message may call this or supply approval/binding.
PeerReadStatus ConfigureRememberedRuntime(const RememberedPeerBinding&,bool configuration_approved);
void BootRememberedRuntime();
void TickRememberedRuntime();
bool RememberedRuntimePresent();
uint64_t CaptureRememberedListen();
void RegisterRememberedPhysicalControl(const void*,uint64_t view);
void RegisterRememberedCameraControls(const void* allow,const void* decline,uint64_t view);
std::string ShowRememberedCameraPermission();
void HideRememberedCameraPermission();
bool CaptureRememberedCameraChoice(const void* control,uint64_t view);
void CaptureRememberedLocalProcessingGesture(const void*,uint64_t view,uint64_t captured_generation);
bool RememberedPhysicalListen(uint64_t captured_generation);
void InterruptRememberedRuntimeImmediately();
bool RememberedTransportActive();
bool RememberedAudioArmed();
void MuteRememberedRuntime();
StoreResult RevokeRememberedRuntime();
bool AuthenticatedRememberedEnrollmentStored();
bool RememberedRuntimeStopped();
const char* RememberedStatusText();
}
