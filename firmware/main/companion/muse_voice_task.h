// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
namespace muse_native_review {struct Stamp;}
#include <string>
#include <cstdint>
class CoreS3AudioCodec;
namespace companion::muse {
struct VoiceAdmissionScope;
enum class VoiceTaskStage{Off,Prepared,Starting,Reviewing,DirectionsPrepared,Closing};
struct VoiceTaskSnapshot{VoiceTaskStage stage=VoiceTaskStage::Off;bool end_confirmed=false;}; // lifecycle only
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
// Trusted local owner, not a remote receiver or scope issuer. Original grant
// contexts and initialized Core borrow must outlive true cleanup. No autostart.
bool SampleNativeVoiceClock(muse_native_review::Stamp&);
bool AllocateNativeVoiceIdentity(uint64_t&,std::string&);
void StopNativeVoiceTaskScope(uint64_t);
void VoiceTaskProviderVisible(bool);
bool PrepareNativeVoiceTask(const VoiceAdmissionScope&,CoreS3AudioCodec&);
bool StartPreparedNativeVoiceTask();
bool NativeVoiceTaskPrepared();
bool QueueNativeVoiceReview();
VoiceTaskSnapshot ViewNativeVoiceTask();
void StopNativeVoiceTask();
void PollNativeVoiceTaskCleanup(); // retries cleanup dispatch only, never Start
bool NativeVoiceTaskCleanupComplete();
#else
inline bool SampleNativeVoiceClock(muse_native_review::Stamp&){return false;}
inline bool AllocateNativeVoiceIdentity(uint64_t&,std::string&){return false;}
inline void StopNativeVoiceTaskScope(uint64_t){}
inline void VoiceTaskProviderVisible(bool){}
inline bool PrepareNativeVoiceTask(const VoiceAdmissionScope&,CoreS3AudioCodec&){return false;}
inline bool StartPreparedNativeVoiceTask(){return false;}
inline bool NativeVoiceTaskPrepared(){return false;}
inline bool QueueNativeVoiceReview(){return false;}
inline VoiceTaskSnapshot ViewNativeVoiceTask(){return {};}
inline void StopNativeVoiceTask(){}
inline void PollNativeVoiceTaskCleanup(){}
inline bool NativeVoiceTaskCleanupComplete(){return true;}
#endif
}
