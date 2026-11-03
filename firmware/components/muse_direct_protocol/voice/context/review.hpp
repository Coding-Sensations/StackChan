#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <limits>
namespace muse_review {
struct Time {uint64_t mono,wall;};
// Data/correlation only; no credential or authority fields.
struct Target {char device[97]{},call[97]{},session[97]{},thread[97]{};uint64_t generation=0;Time expires{};};
struct Source {virtual ~Source()=default;virtual bool Read(Target&)=0;virtual Time Now()=0;};
struct Unavailable:Source {bool Read(Target&) override{return false;}Time Now()override{return {};}};
struct LocalReceipt {Target target{};bool valid=false,microphone=false,speaker=false;};
inline void Wipe(void* p,size_t n){volatile unsigned char* b=static_cast<volatile unsigned char*>(p);while(n--)*b++=0;}
class Model {
 Source& source_;Target target_{};LocalReceipt receipt_{};Time last_{};uint64_t revision_=1;unsigned page_=0,pages_=0;bool active_=false,busy_=false,mic_=false,speaker_=false;
 bool Advance(){if(revision_==UINT64_MAX){Cancel();return false;}++revision_;return true;}
 static bool Id(const char* s){size_t n=0;for(;n<97&&s[n];++n)if(s[n]<33||s[n]>126)return false;return n>0&&n<=96;}
 static bool Equal(const Target&a,const Target&b){return a.generation==b.generation&&a.expires.mono==b.expires.mono&&a.expires.wall==b.expires.wall&&!std::strcmp(a.device,b.device)&&!std::strcmp(a.call,b.call)&&!std::strcmp(a.session,b.session)&&!std::strcmp(a.thread,b.thread);}
 static bool Valid(const Target&t,Time n){return Id(t.device)&&Id(t.call)&&Id(t.session)&&Id(t.thread)&&t.generation&&t.expires.mono>n.mono&&t.expires.wall>n.wall&&t.expires.mono-n.mono<=120000&&t.expires.wall-n.wall<=120000;}
 bool Check(Time n){if(!active_||busy_)return false;uint64_t r=revision_;busy_=true;Target fresh{};bool ok=source_.Read(fresh);Time after=source_.Now();busy_=false;
 ok=ok&&r==revision_&&active_&&n.mono>=last_.mono&&n.wall>=last_.wall&&after.mono>=n.mono&&after.wall>=n.wall&&Valid(fresh,n)&&Valid(fresh,after)&&Equal(fresh,target_);Wipe(&fresh,sizeof fresh);if(!ok){Cancel();return false;}last_=after;return true;}
 bool CheckNow(){if(busy_||!active_)return false;auto r=revision_;busy_=true;Time n=source_.Now();busy_=false;if(r!=revision_||!active_)return false;return Check(n);}
 bool Event(uint64_t r){if(r!=revision_||busy_)return false;return CheckNow()&&r==revision_;}
 public:
 explicit Model(Source&s):source_(s){}~Model(){Cancel();}Model(const Model&)=delete;Model&operator=(const Model&)=delete;
 void Cancel(){active_=false;page_=pages_=0;mic_=speaker_=false;Wipe(&target_,sizeof target_);Wipe(&receipt_,sizeof receipt_);last_={};if(revision_!=UINT64_MAX)++revision_;}
 bool Open(Time n){if(busy_)return false;Cancel();if(revision_==UINT64_MAX)return false;auto r=revision_;busy_=true;Target fresh{};bool ok=source_.Read(fresh);Time after=source_.Now();busy_=false;ok=ok&&r==revision_&&after.mono>=n.mono&&after.wall>=n.wall&&Valid(fresh,n)&&Valid(fresh,after);if(ok){target_=fresh;last_=after;active_=true;for(unsigned f=0;f<6;++f){char v[97];Field(f,v);pages_+=unsigned((std::strlen(v)+17)/18);}}Wipe(&fresh,sizeof fresh);return ok;}
 bool Poll(Time n){return Check(n);}bool Active()const{return active_;}bool Reviewed()const{return active_&&page_==pages_;}uint64_t Revision()const{return revision_;}unsigned Page()const{return page_;}unsigned Pages()const{return pages_;}
 void Field(unsigned i,char (&out)[97])const{out[0]=0;if(!active_)return;switch(i){case 0:std::strcpy(out,target_.device);break;case 1:std::strcpy(out,target_.call);break;case 2:std::strcpy(out,target_.session);break;case 3:std::strcpy(out,target_.thread);break;case 4:std::snprintf(out,97,"%llu",(unsigned long long)target_.generation);break;case 5:std::snprintf(out,97,"mono:%llu wall:%llu",(unsigned long long)target_.expires.mono,(unsigned long long)target_.expires.wall);break;}}
 bool PageText(char (&out)[25],unsigned& field,unsigned& part,unsigned& count)const{out[0]=0;if(!active_||Reviewed())return false;unsigned p=page_;for(field=0;field<6;++field){char v[97];Field(field,v);count=unsigned((std::strlen(v)+17)/18);if(p<count){part=p+1;std::strncpy(out,v+p*18,18);out[18]=0;return true;}p-=count;}return false;}
 bool Acknowledge(uint64_t r,unsigned p){if(!Event(r)||Reviewed()||p!=page_)return false;++page_;return Advance();}
 bool OptIn(uint64_t r,bool microphone,bool speaker){if(!Event(r)||!Reviewed()||receipt_.valid)return false;mic_=microphone;speaker_=speaker;return Advance();}
 bool Finish(uint64_t r){if(!Event(r)||!Reviewed()||receipt_.valid)return false;receipt_.target=target_;receipt_.valid=true;receipt_.microphone=mic_;receipt_.speaker=speaker_;return Advance();}
 LocalReceipt Receipt(){if(!CheckNow())return {};return receipt_;}bool NativePermit()const{return false;}
};
}
