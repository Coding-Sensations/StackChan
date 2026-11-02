// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include "muse_voice_core_codec.hpp"
#include "hal/board/cores3_audio_codec.h"
#include <memory>
#include <new>
namespace companion::muse {
using NativeCoreVoiceCodec=CoreVoiceCodec<CoreS3AudioCodec>;
// Constructor only: no grant, reservation, DSP handle, IO or route is created.
inline std::unique_ptr<NativeCoreVoiceCodec> CreateNativeCoreVoiceCodec(CoreS3AudioCodec& core){return std::unique_ptr<NativeCoreVoiceCodec>(new(std::nothrow)NativeCoreVoiceCodec(core));}
}
#endif
