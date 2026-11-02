// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include "muse_voice_device_ui.hpp"
#include "muse_voice_admission_native.hpp"
#include "muse_direct_credential_custody.hpp"
#include "muse_direct_local_port.h"
#include <hal/hal.h>
namespace companion::muse {
class M5VoiceDevice final {
public:
 bool Foreground()const{return DeviceLocalPort().Foreground();}
 bool Node(std::string&out)const{LocalIdentity identity;if(!DeviceLocalPort().Identity(identity))return false;out=std::move(identity.node);return true;}
 lv_indev_t* Input()const{return GetHAL().lvTouchpad;}
};
using NativeDeviceVoiceConsent=DeviceVoiceConsent<NativeVoiceAdmission,CredentialLease,M5VoiceDevice>;
}
#endif
