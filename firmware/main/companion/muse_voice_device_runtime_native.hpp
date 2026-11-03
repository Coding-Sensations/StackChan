// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include "muse_voice_device_runtime.hpp"
#include "muse_voice_device_native.hpp"
#include "muse_voice_core_codec_native.hpp"
#include <esp_lvgl_port.h>
namespace companion::muse {
class M5VoiceUiAccess final:public VoiceUiAccess {
public:
 // HAL's legacy void wrapper discards timeout failures. Use the identical
 // initialized display mutex with a checked bounded acquisition instead.
 bool Enter()override{return GetHAL().lvTouchpad&&lvgl_port_lock(20);}
 void Leave()override{lvgl_port_unlock();}
};
inline VoiceUiAccess& NativeVoiceUiAccess(){static M5VoiceUiAccess access;return access;}
using NativeDeviceVoiceRuntime=DeviceVoiceRuntime<NativeVoiceAdmission,CredentialLease,M5VoiceDevice>;
// Dormant constructor composition; caller owns original acquired RAM lease,
// authenticated admission scope, crypto/clock/custody/device/codec and UI parent.
// Explicit HAL UI access selects queued callbacks; one protocol owner calls
// runtime methods outside the LVGL lock. Other actors may SignalClose only.
// No token load, Acquire, endpoint connection or task/route registration here.
inline std::unique_ptr<NativeDeviceVoiceRuntime> CreateNativeDeviceVoiceRuntime(voice_prep::Protocol&protocol,CredentialLease&reader,wire::CryptoBackend&crypto,muse_native_review::Clock&clock,muse_native_review::Custody&source,NativeCoreVoiceCodec&codec,M5VoiceDevice&device){
 auto admission=CreateNativeVoiceAdmission(protocol,reader,crypto,clock,source);
 return NativeDeviceVoiceRuntime::Create(protocol,admission,reader,codec,clock,device,&NativeVoiceUiAccess());
}
}
#endif
