// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
class CoreS3AudioCodec;
namespace companion::muse {
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
// Board publishes only its successfully constructed, program-lifetime codec.
// Lookup does not construct Board/codec, reserve audio or perform hardware IO.
bool PublishStaticVoiceCore(CoreS3AudioCodec&);
CoreS3AudioCodec* ExistingVoiceCore();
#else
inline bool PublishStaticVoiceCore(CoreS3AudioCodec&){return false;}
inline CoreS3AudioCodec* ExistingVoiceCore(){return nullptr;}
#endif
}
