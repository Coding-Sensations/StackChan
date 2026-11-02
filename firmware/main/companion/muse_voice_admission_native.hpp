// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include "muse_voice_admission.hpp"
#include "muse_direct_native_api.h"
namespace companion::muse {
using NativeVoiceAdmission=VoiceAdmission<NativeVmLookup,NoiseSocket>;
// Dormant constructor composition only. No caller/route/permission is registered.
inline std::unique_ptr<NativeVoiceAdmission> CreateNativeVoiceAdmission(voice_prep::Protocol& protocol,DeviceCredentialReader& reader,wire::CryptoBackend& crypto,muse_native_review::Clock& clock,muse_native_review::Custody& source){
    return NativeVoiceAdmission::Create(protocol,reader,crypto,clock,source,
        [](std::function<bool()> current){return std::unique_ptr<NativeVmLookup>(new(std::nothrow)NativeVmLookup(std::move(current)));},
        [](std::function<bool()> current){return std::unique_ptr<NoiseSocket>(new(std::nothrow)DirectMetaSocket(std::move(current)));});
}
}
#endif
