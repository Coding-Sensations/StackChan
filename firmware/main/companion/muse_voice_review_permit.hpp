// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include "muse_voice_codec_owner.hpp"
namespace muse_native_review {
// Serialized, single-use validation of an independently supplied native owner.
// Borrowed Bridge/Clock/native authority must outlive retained audio cleanup.
// BindReview is reviewed data only, never issuance of native rights.
class ReviewBoundPermit final:public voice_codec_prep::Permit {
 voice_prep::Protocol& protocol_;Bridge& bridge_;Clock& clock_;voice_codec_prep::Permit& native_;
 mutable muse_review::LocalReceipt pinned_{};mutable Stamp last_{};mutable int64_t requested_=0;
 uint64_t revision_=0;bool spent_=false;
 mutable bool active_=false,checking_=false,sampled_=false,revoked_=false;
 struct Busy{bool& flag;explicit Busy(bool& f):flag(f){flag=true;}~Busy(){flag=false;}};
 struct WipedReceipt{muse_review::LocalReceipt value{};~WipedReceipt(){muse_review::Wipe(&value,sizeof value);}};
 static bool Same(const muse_review::Target&a,const muse_review::Target&b){return a.generation==b.generation&&a.expires.mono==b.expires.mono&&a.expires.wall==b.expires.wall&&
  !std::strcmp(a.device,b.device)&&!std::strcmp(a.call,b.call)&&!std::strcmp(a.session,b.session)&&!std::strcmp(a.thread,b.thread);}
 bool Bound()const{return active_&&!revoked_&&bridge_.UsesProtocol(protocol_)&&bridge_.UsesClock(clock_)&&bridge_.Review().Active()&&bridge_.Review().Revision()==revision_;}
 bool StampCurrent(Stamp n,int64_t required)const{return n.mono_us>=0&&n.wall_ms>=0&&n.mono_us>=required&&
  (!sampled_||(n.mono_us>=last_.mono_us&&n.wall_ms>=last_.wall_ms))&&
  uint64_t(n.mono_us)<pinned_.target.expires.mono*1000&&uint64_t(n.wall_ms)<pinned_.target.expires.wall;}
 bool ReviewCurrent()const{
  WipedReceipt fresh;fresh.value=bridge_.Review().Receipt();
  return Bound()&&fresh.value.valid&&fresh.value.microphone==pinned_.microphone&&fresh.value.speaker==pinned_.speaker&&Same(fresh.value.target,pinned_.target);
 }
 bool Current(uint64_t generation,int64_t required,Stamp& after)const{
  if(!Bound()||generation!=pinned_.target.generation||required<requested_)return false;
  Stamp before{};
  if(!clock_.Sample(before)||!Bound()||!StampCurrent(before,required)||!ReviewCurrent()||
     !clock_.Sample(after)||!Bound()||after.mono_us<before.mono_us||after.wall_ms<before.wall_ms||!StampCurrent(after,required)||!ReviewCurrent())return false;
  last_=after;sampled_=true;requested_=required;return true;
 }
 enum class Query{Context,Microphone,Output,Provider,Hold};
 bool Check(Query query,uint64_t generation,int64_t required,uint64_t ticket=0)const{
  if(checking_||!active_||generation!=pinned_.target.generation)return false;
  if((query==Query::Microphone||query==Query::Hold)&&!pinned_.microphone)return false;
  if(query==Query::Output&&!pinned_.speaker)return false;
  if(query==Query::Hold&&!ticket)return false;
  Busy checking(checking_);Stamp stamp{};
  const auto failed=[&]{Revoke();return false;};
  const companion::muse::Directions selected{pinned_.microphone,pinned_.speaker};
  if(!Current(generation,required,stamp)||!native_.MatchesReviewedContext(pinned_.target,selected,stamp.mono_us)||!Current(generation,required,stamp))return failed();
  bool allowed=false;
  switch(query){
   case Query::Context:allowed=native_.VoiceContext(generation,stamp.mono_us);break;
   case Query::Microphone:allowed=native_.Microphone(generation,stamp.mono_us);break;
   case Query::Output:allowed=native_.Output(generation,stamp.mono_us);break;
   case Query::Provider:allowed=native_.Provider(generation,stamp.mono_us);break;
   case Query::Hold:allowed=native_.Hold(generation,ticket,stamp.mono_us);break;
  }
  if(!allowed||!Current(generation,required,stamp)||!native_.MatchesReviewedContext(pinned_.target,selected,stamp.mono_us)||!Current(generation,required,stamp))return failed();
  return true;
 }
 friend class ReviewedDirectionOwner;
public:
 ReviewBoundPermit(voice_prep::Protocol&p,Bridge&b,Clock&c,voice_codec_prep::Permit&native):protocol_(p),bridge_(b),clock_(c),native_(native){}
 ReviewBoundPermit(const ReviewBoundPermit&)=delete;ReviewBoundPermit&operator=(const ReviewBoundPermit&)=delete;
 ~ReviewBoundPermit(){Revoke();}
 void Revoke()const{revoked_=true;active_=false;sampled_=false;last_={};requested_=0;muse_review::Wipe(&pinned_,sizeof pinned_);}
 bool BindReview(){
  if(spent_||checking_||revoked_)return false;
  spent_=true;Busy checking(checking_);
  if(!bridge_.UsesProtocol(protocol_)||!bridge_.UsesClock(clock_))return false;
  WipedReceipt fresh;fresh.value=bridge_.Review().Receipt();
  if(revoked_||!fresh.value.valid||(!fresh.value.microphone&&!fresh.value.speaker)||!fresh.value.target.generation||
     fresh.value.target.expires.mono>uint64_t(INT64_MAX/1000)||fresh.value.target.expires.wall>uint64_t(INT64_MAX))return false;
  pinned_=fresh.value;revision_=bridge_.Review().Revision();active_=true;Stamp stamp{};
  if(!Current(pinned_.target.generation,0,stamp)){Revoke();return false;}return true;
 }
 bool VoiceContext(uint64_t g,int64_t now)const override{return Check(Query::Context,g,now);}
 bool Microphone(uint64_t g,int64_t now)const override{return Check(Query::Microphone,g,now);}
 bool Output(uint64_t g,int64_t now)const override{return Check(Query::Output,g,now);}
 bool Provider(uint64_t g,int64_t now)const override{return Check(Query::Provider,g,now);}
 bool Hold(uint64_t g,uint64_t ticket,int64_t now)const override{return Check(Query::Hold,g,now,ticket);}
};
// Actual existing retained Owner composition. This does not supply its native
// authority: unavailable/boolean-only implementations cannot acquire resources.
class ReviewedDirectionOwner final {
 ReviewBoundPermit permit_;voice_codec_prep::Owner owner_;uint64_t generation_=0;bool spent_=false;
public:
 ReviewedDirectionOwner(voice_prep::Protocol&p,Bridge&b,voice_codec_prep::Codec&codec,voice_codec_prep::Permit&native,Clock&clock):permit_(p,b,clock,native),owner_(p,codec,permit_,clock){}
 ReviewedDirectionOwner(const ReviewedDirectionOwner&)=delete;ReviewedDirectionOwner&operator=(const ReviewedDirectionOwner&)=delete;
 bool Prepare(){
  if(spent_)return false;
  spent_=true;if(!permit_.BindReview())return false;
  generation_=permit_.pinned_.target.generation;
  const companion::muse::Directions selected{permit_.pinned_.microphone,permit_.pinned_.speaker};
  const auto expires=static_cast<int64_t>(permit_.pinned_.target.expires.mono*1000);const auto now=permit_.last_.mono_us;
  if(!owner_.Prepare(generation_,expires,now,selected)){permit_.Revoke();return false;}return true;
 }
 void Revoke(){permit_.Revoke();} // pure cancellation, never hardware IO
 voice_codec_prep::State Status()const{return owner_.Status();}
 uint64_t Generation()const{return generation_;}
 bool Start(int64_t now){return generation_&&owner_.Start(generation_,now);}
 bool Talking(uint64_t ticket,bool held,int64_t now){return generation_&&owner_.Talking(generation_,ticket,held,now);}
 bool Capture(int64_t now){return generation_&&owner_.Capture(generation_,now);}
 bool Play(int64_t now){return generation_&&owner_.Play(generation_,now);}
 bool Poll(int64_t now){return generation_&&owner_.Poll(generation_,now);}
 bool Stop(int64_t now){permit_.Revoke();return owner_.Status()==voice_codec_prep::State::Dormant||owner_.Status()==voice_codec_prep::State::Closed||owner_.Stop(generation_,now);}
 voice_codec_prep::Snapshot View(int64_t now){return owner_.View(generation_,now);}
};
}
#endif
