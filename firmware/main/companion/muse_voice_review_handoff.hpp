#pragma once
#include <voice/review_bridge.hpp>
#include "muse_voice_codec_owner.hpp"
namespace muse_native_review {
enum class HandoffResult {NoCurrentReview,NeitherSelected,NativeOwnerUnavailable};
// This adapter deliberately constructs the retained direction Owner with its
// real unavailable permit. A local receipt can never upgrade this permit.
class UnavailableDirectionHandoff {
 voice_codec_prep::NativeUnavailablePermit unavailable_;
 voice_codec_prep::Owner owner_;
 public:
 UnavailableDirectionHandoff(voice_prep::Protocol&p,voice_codec_prep::Codec&codec):owner_(p,codec,unavailable_){}
 UnavailableDirectionHandoff(const UnavailableDirectionHandoff&)=delete;
 UnavailableDirectionHandoff&operator=(const UnavailableDirectionHandoff&)=delete;
 HandoffResult Prepare(Bridge&bridge){
  auto receipt=bridge.Review().Receipt();
  if(!receipt.valid)return HandoffResult::NoCurrentReview;
  if(!receipt.microphone&&!receipt.speaker){muse_review::Wipe(&receipt,sizeof receipt);return HandoffResult::NeitherSelected;}
  auto now=bridge.Now();muse_review::Wipe(&receipt,sizeof receipt);receipt=bridge.Review().Receipt();
  if(!receipt.valid){muse_review::Wipe(&receipt,sizeof receipt);return HandoffResult::NoCurrentReview;}
  // Bounds come from the checked projection; expiry is conservatively floored.
  const auto g=receipt.target.generation;
  const auto expiry=static_cast<int64_t>(receipt.target.expires.mono)*1000;
  const auto stamp=static_cast<int64_t>(now.mono)*1000;
  const voice_codec_prep::Directions selected{receipt.microphone,receipt.speaker};
  muse_review::Wipe(&receipt,sizeof receipt);
  (void)owner_.Prepare(g,expiry,stamp,selected);
  return HandoffResult::NativeOwnerUnavailable;
 }
 bool NativePermitAvailable()const{return false;}
};
}
