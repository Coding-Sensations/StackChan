// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <build_profiles/features.hpp>
#if STACKCHAN_BUILD_MUSE && STACKCHAN_BUILD_MUSE_VOICE_PROTOCOL
#include "muse_direct_credential_policy.hpp"
#include <atomic>
#include "muse_voice_admission.hpp"
#include "muse_voice_codec_owner.hpp"
namespace companion::muse {
template<class Session,class Consent>class DeviceVoiceReviewUi;
// Local device consent only. Admission/reader/clock/input are borrowed through
// session+UI cleanup. Generic injected inputs are test ports, not attestations.
template<class Admission,class Reader,class Device>
class DeviceVoiceConsent final:public voice_codec_prep::Permit {
 Admission& admission_;Reader& reader_;muse_native_review::Clock& clock_;Device& device_;
 mutable std::atomic<bool> revoke_signal_{false};
 mutable AdmittedVoiceContext context_;mutable muse_review::LocalReceipt approved_{};
 mutable muse_review::Model* approved_model_=nullptr;mutable uint64_t approved_revision_=0;
 bool spent_=false,approval_attempted_=false;
 mutable bool bound_=false,granted_=false,revoked_=false,checking_=false;
 mutable muse_native_review::Stamp last_{};
 struct Busy{bool& flag;explicit Busy(bool&f):flag(f){flag=true;}~Busy(){flag=false;}};
 static bool BudgetEqual(const muse_native_review::Budget&a,const muse_native_review::Budget&b){return a.generation==b.generation&&a.expires_us==b.expires_us&&a.expires_wall_ms==b.expires_wall_ms;}
 static bool Same(const AdmittedVoiceContext&a,const AdmittedVoiceContext&b){return a.node==b.node&&a.vm==b.vm&&a.call==b.call&&BudgetEqual(a.budget,b.budget)&&a.target.generation==b.target.generation&&a.target.admitted_device_id==b.target.admitted_device_id&&a.target.call_id==b.target.call_id&&a.target.session_id==b.target.session_id&&a.target.thread_id==b.target.thread_id;}
 static bool ReviewEqual(const muse_review::Target&r,const AdmittedVoiceContext&c){return r.generation==c.budget.generation&&r.generation==c.target.generation&&
  r.expires.mono==uint64_t(c.budget.expires_us/1000)&&r.expires.wall==uint64_t(c.budget.expires_wall_ms)&&
  !std::strcmp(r.device,c.target.admitted_device_id.data())&&!std::strcmp(r.call,c.target.call_id.data())&&!std::strcmp(r.session,c.target.session_id.data())&&!std::strcmp(r.thread,c.target.thread_id.data());}
 bool SourceCurrent()const{
  if(revoke_signal_.load()){Revoke();return false;}
  if(!bound_||revoked_||checking_)return false;
  if(granted_&&(!approved_model_||!approved_model_->Active()||approved_model_->Revision()!=approved_revision_)){Revoke();return false;}
  Busy checking(checking_);AdmittedVoiceContext fresh;muse_native_review::Stamp n{};std::string node;
  bool ok=STACKCHAN_MUSE_RAM_ONLY&&admission_.UsesReader(reader_)&&admission_.UsesClock(clock_)&&reader_.Current()&&device_.Foreground()&&
   admission_.ReadAuthorizedContext(fresh)&&!revoked_&&Same(fresh,context_)&&device_.Node(node)&&node==context_.node&&
   clock_.Sample(n)&&!revoked_&&n.mono_us>=last_.mono_us&&n.wall_ms>=last_.wall_ms&&
   n.mono_us>=fresh.stamp.mono_us&&n.wall_ms>=fresh.stamp.wall_ms&&n.mono_us>=0&&n.wall_ms>=0&&
   n.mono_us<context_.budget.expires_us&&n.wall_ms<context_.budget.expires_wall_ms&&reader_.Current()&&device_.Foreground();
  if(ok&&granted_)ok=uint64_t(n.mono_us)<approved_.target.expires.mono*1000&&uint64_t(n.wall_ms)<approved_.target.expires.wall;
  ok=ok&&!revoked_&&bound_&&!revoke_signal_.load();
  if(ok&&granted_)ok=approved_model_&&approved_model_->Active()&&approved_model_->Revision()==approved_revision_;
  Wipe(node);if(!ok){Revoke();return false;}last_=n;return true;
 }
 // Only the native-input-validated UI may issue selected local permissions.
 bool Allow(const muse_review::LocalReceipt& receipt,muse_review::Model& model,uint64_t revision){
  if(approval_attempted_||granted_||!receipt.valid||(!receipt.microphone&&!receipt.speaker))return false;
  approval_attempted_=true;
  if(!SourceCurrent()||!ReviewEqual(receipt.target,context_)||revoked_||revoke_signal_.load()||!model.Active()||model.Revision()!=revision)return false;
  approved_=receipt;approved_model_=&model;approved_revision_=revision;granted_=true;return true;
 }
 template<class Session,class Consent>friend class DeviceVoiceReviewUi;
public:
 DeviceVoiceConsent(Admission&a,Reader&r,muse_native_review::Clock&clock,Device&device):admission_(a),reader_(r),clock_(clock),device_(device){}
 DeviceVoiceConsent(const DeviceVoiceConsent&)=delete;DeviceVoiceConsent&operator=(const DeviceVoiceConsent&)=delete;
 ~DeviceVoiceConsent(){Revoke();}
 void SignalRevoke()const{revoke_signal_.store(true);} // GUI callback: no owner-state mutation/IO
 bool RevocationRequested()const{return revoke_signal_.load();}
 void Revoke()const{SignalRevoke();revoked_=true;bound_=granted_=false;approved_model_=nullptr;approved_revision_=0;context_.Clear();muse_review::Wipe(&approved_,sizeof approved_);last_={};}
 bool Bind(){
  if(spent_||revoked_||checking_||revoke_signal_.load())return false;
  spent_=true;Busy checking(checking_);std::string node;
  bool ok=STACKCHAN_MUSE_RAM_ONLY&&admission_.UsesReader(reader_)&&admission_.UsesClock(clock_)&&reader_.Current()&&device_.Foreground()&&
   admission_.ReadAuthorizedContext(context_)&&!revoked_&&device_.Node(node)&&node==context_.node&&reader_.Current()&&device_.Foreground();
  Wipe(node);if(!ok||revoke_signal_.load()){Revoke();return false;}last_=context_.stamp;bound_=true;return true;
 }
 bool BoundCurrent()const{return SourceCurrent();}
 bool VoiceContext(uint64_t g,int64_t now)const override{return granted_&&g==context_.budget.generation&&SourceCurrent()&&now>=0&&now<=last_.mono_us;}
 bool Provider(uint64_t g,int64_t now)const override{return VoiceContext(g,now);}
 bool Microphone(uint64_t g,int64_t now)const override{return approved_.microphone&&VoiceContext(g,now);}
 bool Output(uint64_t g,int64_t now)const override{return approved_.speaker&&VoiceContext(g,now);}
 bool Hold(uint64_t,uint64_t,int64_t)const override{return false;} // no invented native hold proof
 bool MatchesReviewedContext(const muse_review::Target&t,Directions d,int64_t now)const override{return granted_&&d.microphone==approved_.microphone&&d.output==approved_.speaker&&VoiceContext(t.generation,now)&&ReviewEqual(t,context_);}
};
}
#endif
