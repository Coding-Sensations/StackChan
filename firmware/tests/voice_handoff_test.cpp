#define main original_bridge_fixture_main
#include "voice_bridge_test.cpp"
#undef main
#include "muse_voice_review_handoff.hpp"
struct RejectCodec:voice_codec_prep::Codec {
 unsigned operations=0;
 bool Reserve(uint64_t)override{++operations;return false;}
 voice_codec_prep::Observation Observe(uint64_t)override{++operations;return {};}
 bool EnableMicrophone(uint64_t)override{++operations;return false;}bool EnableOutput(uint64_t)override{++operations;return false;}
 size_t Read16k(uint64_t,int16_t*,size_t)override{++operations;return 0;}size_t Write16k(uint64_t,const int16_t*,size_t)override{++operations;return 0;}
 bool StopMicrophone(uint64_t)override{++operations;return false;}bool StopOutput(uint64_t)override{++operations;return false;}
 bool FlushOutput(uint64_t,uint64_t)override{++operations;return false;}bool Join(uint64_t)override{++operations;return false;}bool Release(uint64_t)override{++operations;return false;}
};
int main(){for(unsigned mask=0;mask<4;++mask){voice_prep::Protocol p;Live(p);TestClock c;TestCustody custody;Bridge b(p,c,custody);RejectCodec codec;UnavailableDirectionHandoff handoff(p,codec);assert(handoff.Prepare(b)==HandoffResult::NoCurrentReview);assert(b.Open());Complete(b,mask&1,mask&2);assert(handoff.Prepare(b)==(mask?HandoffResult::NativeOwnerUnavailable:HandoffResult::NeitherSelected));assert(!handoff.NativePermitAvailable()&&codec.operations==0&&companion::CurrentAudioOwner()==companion::AudioOwner::None);b.Revoke();assert(handoff.Prepare(b)==HandoffResult::NoCurrentReview&&codec.operations==0);}std::puts("PASS actual direction Owner handoff: four reviewed masks, neither no-op, unavailable native permit, zero codec/common-owner operations");}
