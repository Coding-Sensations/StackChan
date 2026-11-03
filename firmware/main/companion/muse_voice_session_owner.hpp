// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include "muse_voice_admission.hpp"
#include "muse_voice_review_permit.hpp"
namespace companion::muse {
// Stable heap address, serialized callers. Borrowed Protocol/Clock/codec/native
// authority and original scope callback contexts outlive complete cleanup.
// Destruction is never a substitute for checked audio/transport/UI retirement.
template<class Admission>
class VoiceSessionOwner final {
 voice_prep::Protocol& protocol_;voice_codec_prep::Codec& codec_;voice_codec_prep::Permit& native_;muse_native_review::Clock& clock_;
 std::unique_ptr<Admission> admission_;
 std::unique_ptr<muse_native_review::ReviewedDirectionOwner> audio_;
 muse_native_review::Bridge* review_=nullptr;muse_native_review::Stamp last_{};
 bool spent_=false,active_=false,busy_=false,closing_=false,closed_=false,ending_=false,ending_transport_=false,cancel_attempted_=false,transport_joined_=false,direction_attempted_=false;
 unsigned borrows_=0;
 struct Busy{bool& flag;explicit Busy(bool& f):flag(f){flag=true;}~Busy(){flag=false;}};
 VoiceSessionOwner(voice_prep::Protocol&p,voice_codec_prep::Codec&c,voice_codec_prep::Permit&n,muse_native_review::Clock&k):protocol_(p),codec_(c),native_(n),clock_(k){}
 bool AdmissionAllowed()const{return spent_&&!closed_&&!closing_&&(!ending_||ending_transport_);}
 void RevokeDirections(){if(audio_)audio_->Revoke();if(review_)review_->Revoke();}
 void RequestClose(){closing_=true;ending_transport_=false;RevokeDirections();}
 void RequestEnd(){ending_=true;RevokeDirections();}
 bool Sample(muse_native_review::Stamp& n){
  if(!clock_.Sample(n)||n.mono_us<last_.mono_us||n.wall_ms<last_.wall_ms||n.mono_us<0||n.wall_ms<0)return false;
  last_=n;return true;
 }
 bool StopAudio(){
  muse_native_review::Stamp n{};(void)Sample(n); // clock loss cannot prevent original cleanup
  if(audio_&&!audio_->Stop(last_.mono_us))return false;
  audio_.reset();return true;
 }
 bool EndLocked(){
  if(!StopAudio()||closing_)return false;
  if(cancel_attempted_)return !closing_;
  ending_transport_=true;cancel_attempted_=true;
  if(!admission_->Cancel()||closing_){RequestClose();return false;}return true;
 }
 muse_native_review::Bridge* AvailableReview()const{return active_&&!closing_&&!ending_&&!closed_&&protocol_.Status()==voice_prep::State::Live?review_:nullptr;}
 void ReleaseBorrow(){assert(borrows_);--borrows_;} // no IO from UI destruction
 template<class Operation>bool Audio(Operation operation){
  if(busy_||!active_||closing_||ending_||!audio_)return false;
  Busy busy(busy_);muse_native_review::Stamp n{};
  if(!Sample(n)||closing_||ending_){RequestClose();return false;}
  const bool result=operation(n.mono_us);
  if(audio_->Status()!=voice_codec_prep::State::Ready)RequestClose();
  return result&&!closing_&&!ending_; // pending/empty Ready results are not failure
 }
public:
 static constexpr unsigned ReviewBorrowLimit=8;
 class ReviewBorrow final {
  VoiceSessionOwner* owner_=nullptr;muse_native_review::Bridge* review_=nullptr;
  friend class VoiceSessionOwner;
  ReviewBorrow(VoiceSessionOwner&owner,muse_native_review::Bridge&review):owner_(&owner),review_(&review){++owner_->borrows_;}
 public:
  ReviewBorrow()=default;ReviewBorrow(const ReviewBorrow&)=delete;ReviewBorrow&operator=(const ReviewBorrow&)=delete;
  ReviewBorrow(ReviewBorrow&& other)noexcept:owner_(other.owner_),review_(other.review_){other.owner_=nullptr;other.review_=nullptr;}
  ReviewBorrow&operator=(ReviewBorrow&& other)noexcept{if(this!=&other){Reset();owner_=other.owner_;review_=other.review_;other.owner_=nullptr;other.review_=nullptr;}return *this;}
  ~ReviewBorrow(){Reset();}
  void Reset(){if(owner_)owner_->ReleaseBorrow();owner_=nullptr;review_=nullptr;}
  muse_native_review::Bridge* Get()const{return owner_&&owner_->AvailableReview()==review_?review_:nullptr;}
  explicit operator bool()const{return Get()!=nullptr;}
 };
 static std::unique_ptr<VoiceSessionOwner> Create(voice_prep::Protocol&protocol,std::unique_ptr<Admission>&candidate,voice_codec_prep::Codec&codec,voice_codec_prep::Permit&native,muse_native_review::Clock&clock){
  if(!candidate||!candidate->Unused()||!candidate->UsesProtocol(protocol)||!candidate->UsesClock(clock))return {};
  auto result=std::unique_ptr<VoiceSessionOwner>(new(std::nothrow)VoiceSessionOwner(protocol,codec,native,clock));
  if(result){result->admission_=std::move(candidate);}
  return result;
 }
 VoiceSessionOwner(const VoiceSessionOwner&)=delete;VoiceSessionOwner&operator=(const VoiceSessionOwner&)=delete;
 ~VoiceSessionOwner(){assert((!spent_||closed_)&&!borrows_);}
 bool Open(const VoiceAdmissionScope&scope){
  if(busy_||spent_||closing_||closed_)return false;
  Busy busy(busy_);spent_=true;auto captured=scope;
  const auto wrap=[this](std::function<bool()> original){return [this,original=std::move(original)]{return AdmissionAllowed()&&original&&original()&&AdmissionAllowed();};};
  if(scope.credential_current)captured.credential_current=wrap(scope.credential_current);
  if(scope.network_current)captured.network_current=wrap(scope.network_current);
  if(scope.voice_current)captured.voice_current=wrap(scope.voice_current);
  active_=admission_->Open(captured)&&!closing_&&!ending_;
  if(!active_){RequestClose();return false;}review_=admission_->Review();return true;
 }
 bool Tick(){
  if(busy_||!active_||closing_||closed_)return false;
  Busy busy(busy_);
  if(ending_){if(!EndLocked())return false;}
  else if(audio_){muse_native_review::Stamp n{};if(!Sample(n)||closing_||ending_||!audio_->Poll(n.mono_us)){RequestClose();return false;}}
  if(!admission_->Tick()||closing_){RequestClose();return false;}
  review_=admission_->Review();return true;
 }
 ReviewBorrow BorrowReview(){auto* review=AvailableReview();return review&&borrows_<ReviewBorrowLimit?ReviewBorrow(*this,*review):ReviewBorrow{};}
 bool PrepareDirections(){
  if(busy_||!active_||closing_||ending_||direction_attempted_||!AvailableReview())return false;
  Busy busy(busy_);auto receipt=review_->Review().Receipt();const bool valid=receipt.valid;const bool selected=receipt.microphone||receipt.speaker;muse_review::Wipe(&receipt,sizeof receipt);
  if(closing_||ending_||!valid)return false;
  direction_attempted_=true;if(!selected)return false;
  audio_.reset(new(std::nothrow)muse_native_review::ReviewedDirectionOwner(protocol_,*review_,codec_,native_,clock_));
  if(!audio_)return false;
  if(!audio_->Prepare()){
   if(audio_->Stop(last_.mono_us))audio_.reset();else RequestClose();return false;
  }
  muse_native_review::Stamp n{};
  if(!Sample(n)||closing_||ending_||!audio_->Start(n.mono_us)){RequestClose();return false;}return true;
 }
 bool Talking(uint64_t ticket,bool held){return Audio([&](int64_t now){return audio_->Talking(ticket,held,now);});}
 bool Capture(){return Audio([&](int64_t now){return audio_->Capture(now);});}
 bool Play(){return Audio([&](int64_t now){return audio_->Play(now);});}
 bool End(){
  if(!spent_||closed_||closing_)return false;
  RequestEnd();if(busy_)return false;
  if(!active_){RequestClose();return false;}
  Busy busy(busy_);return EndLocked();
 }
 bool Close(){
  if(closed_)return true;
  RequestClose();if(busy_)return false;Busy busy(busy_);
  if(!StopAudio())return false;
  if(!transport_joined_){if(!admission_->Close())return false;transport_joined_=true;active_=false;}
  if(borrows_)return false;
  review_=nullptr;admission_.reset();closed_=true;return true;
 }
 bool CloseRequested()const{return closing_;}
 bool EndRequested()const{return ending_;}
 bool DirectionsBound()const{return audio_&&audio_->Status()==voice_codec_prep::State::Ready&&!closing_&&!ending_;} // resource state, not actual activity
 bool UsesAdmission(const Admission&a)const{return admission_.get()==&a;}
 bool UsesNativeAuthority(const voice_codec_prep::Permit&n)const{return &native_==&n;}
 bool UsesClock(const muse_native_review::Clock&clock)const{return &clock_==&clock;}
 bool TransportJoined()const{return transport_joined_;}
 bool CleanupComplete()const{return closed_;}
 bool EndConfirmed()const{return protocol_.EndConfirmed();}
 unsigned ReviewBorrows()const{return borrows_;}
};
}
#endif
