#pragma once
#include "voice_protocol.hpp"
#include "context/review.hpp"
namespace muse_native_review {
// Local owner budget/correlation only. None of these ports grants native rights.
struct Stamp {int64_t mono_us=0,wall_ms=0;};
struct Budget {uint64_t generation=0;int64_t expires_us=0,expires_wall_ms=0;};
struct Clock {virtual ~Clock()=default;virtual bool Sample(Stamp&)=0;};
struct Custody {virtual ~Custody()=default;virtual bool ReadBudget(Budget&)=0;virtual bool Current(const Budget&)=0;};
struct UnavailableClock final:Clock {bool Sample(Stamp&)override{return false;}};
struct UnavailableCustody final:Custody {bool ReadBudget(Budget&)override{return false;}bool Current(const Budget&)override{return false;}};
class Bridge final:public muse_review::Source {
 voice_prep::Protocol& protocol_;UnavailableClock unavailable_clock_;UnavailableCustody unavailable_custody_;Clock& clock_;Custody& custody_;muse_review::Model model_{*this};
 Budget pinned_{};bool pinned_budget_=false;Stamp last_{};bool sampled_=false,revoked_=false,busy_=false;uint64_t revision_=1;
 static bool Same(const Budget&a,const Budget&b){return a.generation==b.generation&&a.expires_us==b.expires_us&&a.expires_wall_ms==b.expires_wall_ms;}
 static bool Forward(Stamp a,Stamp b){return b.mono_us>=a.mono_us&&b.wall_ms>=a.wall_ms;}
 static bool Valid(Stamp n,const Budget&b){return n.mono_us>=0&&n.wall_ms>=0&&b.generation&&b.expires_us>n.mono_us&&b.expires_wall_ms>n.wall_ms&&b.expires_us-n.mono_us<=120000000&&b.expires_wall_ms-n.wall_ms<=120000;}
 template<size_t N>static bool Copy(char(&to)[97],const std::array<char,N>&from){size_t n=0;while(n<N&&from[n])++n;if(!n||n==N||n>96)return false;std::memcpy(to,from.data(),n);to[n]=0;return true;}
 bool Project(muse_review::Target&out,Stamp&after){
  muse_review::Wipe(&out,sizeof out);after={};if(revoked_||busy_)return false;
  busy_=true;auto r=revision_;Stamp before{};Budget budget{};voice_prep::TargetSnapshot returned{};
  bool ok=clock_.Sample(before);ok=ok&&r==revision_&&!revoked_&&(!sampled_||Forward(last_,before));
  if(ok)ok=custody_.ReadBudget(budget);
  ok=ok&&r==revision_&&!revoked_&&Valid(before,budget)&&(!pinned_budget_||Same(pinned_,budget));
  if(ok)ok=custody_.Current(budget);
  if(ok)ok=clock_.Sample(after);
  ok=ok&&r==revision_&&!revoked_&&Forward(before,after)&&Valid(after,budget);
  if(ok)ok=protocol_.ReturnedTarget(budget.generation,after.mono_us,returned);
  if(ok){out.generation=returned.generation;out.expires={uint64_t(budget.expires_us/1000),uint64_t(budget.expires_wall_ms)};
   ok=returned.generation==budget.generation&&Copy(out.device,returned.admitted_device_id)&&Copy(out.call,returned.call_id)&&Copy(out.session,returned.session_id)&&Copy(out.thread,returned.thread_id)&&out.expires.mono>uint64_t(after.mono_us/1000);}
  if(ok){pinned_=budget;pinned_budget_=true;}muse_review::Wipe(&returned,sizeof returned);muse_review::Wipe(&budget,sizeof budget);busy_=false;
  if(!ok){muse_review::Wipe(&out,sizeof out);after={};Revoke();return false;}last_=after;sampled_=true;return true;
 }
 public:
 explicit Bridge(voice_prep::Protocol&p):protocol_(p),clock_(unavailable_clock_),custody_(unavailable_custody_){}
 Bridge(voice_prep::Protocol&p,Clock&clock,Custody&custody):protocol_(p),clock_(clock),custody_(custody){}
 ~Bridge(){Revoke();}Bridge(const Bridge&)=delete;Bridge&operator=(const Bridge&)=delete;
 bool UsesProtocol(const voice_prep::Protocol& p)const{return &protocol_==&p;}
 bool UsesClock(const Clock& c)const{return &clock_==&c;}
 bool Read(muse_review::Target&out)override{Stamp after;return Project(out,after);}
 muse_review::Time Now()override{muse_review::Target discard{};Stamp after;bool ok=Project(discard,after);muse_review::Wipe(&discard,sizeof discard);if(!ok)return {};return {uint64_t(after.mono_us/1000),uint64_t(after.wall_ms)};}
 muse_review::Model& Review(){return model_;}
 bool Open(){if(revoked_||busy_)return false;auto n=Now();return !revoked_&&model_.Open(n);}
 bool Poll(){if(revoked_||busy_)return false;auto n=Now();return !revoked_&&model_.Poll(n);}
 void Revoke(){revoked_=true;sampled_=false;last_={};pinned_budget_=false;muse_review::Wipe(&pinned_,sizeof pinned_);if(revision_!=UINT64_MAX)++revision_;model_.Cancel();}
 void Close(){Revoke();}
};
}
