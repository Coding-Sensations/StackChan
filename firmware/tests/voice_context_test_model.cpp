#include "review.hpp"
#include <cassert>
#include <cstdio>
using namespace muse_review;
struct Fake:Source { Target t{}; bool available=true; Time now{1000,2000}; Time Now() override{return now;} Model* reenter=nullptr; bool Read(Target& out) override { if(reenter) {auto*m=reenter;reenter=nullptr;m->Cancel();} out=t;return available;} };
static void Fill(Fake& f){f.t={};std::strcpy(f.t.device,"device-exact");std::strcpy(f.t.call,"call-exact");std::strcpy(f.t.session,"session-returned");std::strcpy(f.t.thread,"thread-returned");f.t.generation=7;f.t.expires={1100,2100};}
static void Review(Model& m){while(!m.Reviewed()){assert(m.Acknowledge(m.Revision(),m.Page()));}}
int main(){
 Unavailable unavailable;Model none(unavailable);assert(!none.Open({1000,2000}));
 Fake f;Fill(f);Model m(f);assert(m.Open(f.now=Time{1000,2000}));assert(!m.OptIn(m.Revision(),true,true));assert(!m.NativePermit());
 auto stale=m.Revision();Review(m);assert(!m.OptIn(stale,true,true));assert(m.OptIn(m.Revision(),true,false));assert(m.Finish(m.Revision()));assert(m.Receipt().microphone&&!m.Receipt().speaker);assert(!m.NativePermit());
 f.t.generation++;assert(!m.Poll({1001,2001}));assert(!m.Active()&&!m.Receipt().valid);assert(m.Open(f.now=Time{1002,2002}));assert(!m.OptIn(stale,true,true));
 m.Cancel();assert(!m.Active()&&!m.Receipt().valid);assert(m.Open(f.now=Time{1003,2003}));assert(!m.Poll({1002,2004}));assert(!m.Active());
 assert(m.Open(f.now=Time{1004,2004}));assert(!m.Poll({1100,2100}));assert(!m.Active());
 assert(m.Open(f.now=Time{1005,2005}));f.reenter=&m;assert(!m.Poll({1006,2006}));assert(!m.Active());
 assert(m.Open(f.now=Time{1007,2007}));f.available=false;assert(!m.Poll({1008,2008}));assert(!m.Active());
 f.available=true;Fill(f);f.t.session[0]='\0';assert(!m.Open(f.now=Time{1009,2009}));

 f.available=true;Fill(f);f.now={1000,2000};assert(m.Open(f.now));f.now={1100,2100};assert(!m.Acknowledge(m.Revision(),m.Page()));assert(!m.Active());
 f.now={1000,2000};Fill(f);assert(m.Open(f.now));Review(m);assert(m.OptIn(m.Revision(),false,true));assert(m.Finish(m.Revision()));assert(!m.Receipt().microphone&&m.Receipt().speaker);f.t.thread[0]='X';assert(!m.Receipt().valid&&!m.Active());
 for(unsigned field=0;field<6;++field){Fill(f);assert(m.Open(f.now));switch(field){case 0:f.t.device[0]='X';break;case 1:f.t.call[0]='X';break;case 2:f.t.session[0]='X';break;case 3:f.t.thread[0]='X';break;case 4:++f.t.generation;break;case 5:++f.t.expires.wall;break;}assert(!m.Poll(f.now));}
 Fill(f);std::memset(f.t.device,'W',96);f.t.device[96]=0;assert(m.Open(f.now));unsigned total=0;while(!m.Reviewed()){char page[25];unsigned field,part,count;assert(m.PageText(page,field,part,count));assert(std::strlen(page)<=18);assert(m.Acknowledge(m.Revision(),m.Page()));++total;}assert(total>10);assert(m.Finish(m.Revision()));assert(!m.Receipt().microphone&&!m.Receipt().speaker);
 Fill(f);std::memset(f.t.device,'W',97);assert(!m.Open(f.now));Fill(f);f.t.expires.mono=f.now.mono+120001;assert(!m.Open(f.now));
 std::puts("PASS bounded model: review, independent opt-ins, exact replacement, stale revision, expiry, rollback, unavailable, reentry, wipe");
}
