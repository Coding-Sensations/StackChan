// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include "muse_voice_session_owner.hpp"
#include "muse_voice_admission_native.hpp"
#include "muse_voice_core_codec_native.hpp"
namespace companion::muse {
using NativeVoiceSessionOwner=VoiceSessionOwner<NativeVoiceAdmission>;
// Constructor composition only. Caller supplies independently authorized native
// owner and retains borrowed ports through audio/transport/UI cleanup. No route.
inline std::unique_ptr<NativeVoiceSessionOwner> CreateNativeVoiceSessionOwner(voice_prep::Protocol&p,DeviceCredentialReader&r,wire::CryptoBackend&crypto,muse_native_review::Clock&clock,muse_native_review::Custody&custody,NativeCoreVoiceCodec&codec,voice_codec_prep::Permit&native){
 auto admission=CreateNativeVoiceAdmission(p,r,crypto,clock,custody);
 return NativeVoiceSessionOwner::Create(p,admission,codec,native,clock);
}
}
#endif
