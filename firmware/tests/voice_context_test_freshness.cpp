#include "review.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
using namespace muse_review;
struct Fake:Source {
 Target target{};Time now{1000,2000};Model* model=nullptr;unsigned now_calls=0;unsigned trigger_now=0;int read_hook=0,now_hook=0;
 Fake(){std::strcpy(target.device,"exact-device");std::strcpy(target.call,"exact-call");std::strcpy(target.session,"returned-session");std::strcpy(target.thread,"returned-thread");target.generation=9;target.expires={1100,2100};}
 void Change(int hook){switch(hook){case 1:now.mono=1100;break;case 2:now.wall=2100;break;case 3:now.mono=999;break;case 4:now.wall=1999;break;case 5:model->Cancel();break;case 6:now={1100,2100};break;}}
 Time Now()override{++now_calls;if(now_calls==trigger_now){int h=now_hook;now_hook=0;Change(h);}return now;}
 bool Read(Target& t)override{t=target;int h=read_hook;read_hook=0;Change(h);return true;}
};
static void Complete(Model&m){while(!m.Reviewed())assert(m.Acknowledge(m.Revision(),m.Page()));assert(m.OptIn(m.Revision(),true,true));assert(m.Finish(m.Revision()));assert(m.Receipt().valid);}
int main(int argc,char**argv){assert(argc==2);int c=std::atoi(argv[1]);Fake f;Model m(f);f.model=&m;
 if(c<5){f.read_hook=c+1;assert(!m.Open(f.now));assert(!m.Active()&&!m.Receipt().valid);}
 else if(c<10){assert(m.Open(f.now));Complete(m);f.read_hook=c-4;assert(!m.Receipt().valid);assert(!m.Active());}
 else if(c<15){assert(m.Open(f.now));Complete(m);f.trigger_now=f.now_calls+2;f.now_hook=c-9;assert(!m.Receipt().valid);assert(!m.Active());}
 else if(c<19){assert(m.Open(f.now));f.read_hook=c-14;auto revision=m.Revision();assert(!m.Acknowledge(revision,m.Page()));assert(!m.Active()&&!m.Receipt().valid);}
 else if(c<24){assert(m.Open(f.now));Complete(m);f.trigger_now=f.now_calls+1;f.now_hook=c-18;assert(!m.Receipt().valid);assert(!m.Active());}
 else if(c<29){assert(m.Open(f.now));f.read_hook=c-23;assert(!m.Poll(f.now));assert(!m.Active()&&!m.Receipt().valid);}
 else if(c==29){f.read_hook=6;assert(!m.Open(f.now));assert(!m.Active()&&!m.Receipt().valid);}
 else if(c==30){assert(m.Open(f.now));Complete(m);f.read_hook=6;assert(!m.Receipt().valid);assert(!m.Active());}
 else if(c==31){assert(m.Open(f.now));f.read_hook=6;assert(!m.Poll(f.now));assert(!m.Active());}
 else if(c==32){assert(m.Open(f.now));f.read_hook=6;assert(!m.Acknowledge(m.Revision(),m.Page()));assert(!m.Active());}
 else {f.trigger_now=1;f.now_hook=c-32;assert(!m.Open(f.now));assert(!m.Active()&&!m.Receipt().valid);}
 std::printf("PASS freshness case %d: callout expiry/rollback/cancel invalidates review and receipt\n",c);
}
