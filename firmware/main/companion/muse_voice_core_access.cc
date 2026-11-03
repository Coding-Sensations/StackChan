// SPDX-License-Identifier: Apache-2.0
#include "muse_voice_core_access.h"
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include <atomic>
namespace companion::muse {
namespace {std::atomic<CoreS3AudioCodec*> existing{nullptr};}
bool PublishStaticVoiceCore(CoreS3AudioCodec&core){auto* expected=static_cast<CoreS3AudioCodec*>(nullptr);return existing.compare_exchange_strong(expected,&core)||expected==&core;}
CoreS3AudioCodec* ExistingVoiceCore(){return existing.load();}
}
#endif
