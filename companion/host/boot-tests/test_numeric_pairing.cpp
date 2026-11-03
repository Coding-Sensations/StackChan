#if !__has_include("companion/numeric_usb.hpp")
#include <cstdio>
int main(){std::fputs("FAIL numeric pairing protocol absent\n",stderr);return 1;}
#else
#include <companion/numeric_usb.hpp>
#include <companion/numeric_pairing.h>
#include <cassert>
#include <deque>
#include <vector>
#include <iostream>
struct Driver{bool installed=false,core=true,write_ok=true,flush_ok=true;unsigned reads=0,writes=0,closes=0;std::deque<uint8_t> q;bool OnOwnerCore()const{return core;}bool Installed()const{return installed;}bool Install(){installed=true;return true;}bool Uninstall(){installed=false;++closes;return true;}int Read(uint8_t* p,size_t n){++reads;size_t i=0;while(i<n&&!q.empty()){p[i++]=q.front();q.pop_front();}return i;}int WriteAck(const uint8_t*,size_t n){++writes;return write_ok?static_cast<int>(n):static_cast<int>(n-1);}bool FlushAck(){return flush_ok;}};
std::vector<uint8_t> frame(uint32_t boot){std::vector<uint8_t>b{'S','C','N','P',1};for(unsigned i=0;i<16;++i)b.push_back(i+1);for(unsigned i=0;i<4;++i)b.push_back(boot>>(24-8*i));return b;}
int main(){using namespace companion;NumericUsbRequest request;auto b=frame(123);assert(DecodeNumericUsb(b.data(),b.size(),123,request));assert(request.request=="0102030405060708090a0b0c0d0e0f10"&&request.boot_nonce==123);assert(!DecodeNumericUsb(b.data(),b.size(),124,request));assert(!DecodeNumericUsb(b.data(),24,123,request));b.push_back(0);assert(!DecodeNumericUsb(b.data(),b.size(),123,request));
 {Driver d;NumericUsbOwner<Driver> o(d);assert(o.Open(0,123,0));auto f=frame(123);d.q.insert(d.q.end(),f.begin(),f.begin()+8);assert(o.Poll(1,request)==NumericUsbRead::Waiting);d.q.insert(d.q.end(),f.begin()+8,f.end());assert(o.Poll(2,request)==NumericUsbRead::Accepted);assert(!o.Active()&&!d.installed&&d.writes==1);assert(o.Poll(3,request)==NumericUsbRead::Rejected);}
 {Driver d;NumericUsbOwner<Driver> o(d);assert(o.Open(0,123,0));d.q.push_back('X');auto n=d.reads;assert(o.Poll(60000,request)==NumericUsbRead::Rejected&&d.reads==n);}
 {Driver d;d.installed=true;NumericUsbOwner<Driver> o(d);assert(!o.Open(0,123,0)&&d.reads==0&&d.closes==0);}
 {Driver d;NumericUsbOwner<Driver> o(d);assert(o.Open(0,123,0));auto f=frame(123);f.push_back(0);d.q.insert(d.q.end(),f.begin(),f.end());assert(o.Poll(1,request)==NumericUsbRead::Rejected);}
 for(bool flush:{false,true}){Driver d;d.write_ok=flush;d.flush_ok=!flush;NumericUsbOwner<Driver> o(d);assert(!o.Open(0,123,0)&&!o.Active()&&!d.installed);}
 {Driver d;d.q.push_back(1);NumericUsbOwner<Driver> o(d);assert(!o.Open(0,123,0)&&!d.installed);}
 {Driver d;NumericUsbOwner<Driver> o(d);assert(o.Open(0,123,0));d.installed=false;auto n=d.reads;assert(o.Poll(1,request)==NumericUsbRead::Rejected&&d.reads==n&&d.closes==0);}
 std::string a(32,'a'),h(32,'b');NumericPairingContext c;assert(c.Start(a,a,123,10));assert(!c.Grant(a,a,h,120,11));assert(!c.Challenge(h,"12345",60000,11));assert(c.Challenge(h,"123456",1000,11));assert(!c.Grant(a,a,h,120,12));assert(c.Confirm(12));assert(c.Code()=="123456"&&c.Confirmed());assert(c.Confirm(12));assert(!c.Grant(a,a,std::string(32,'c'),120,13));assert(c.Grant(a,a,h,120,13));assert(c.GrantDeadline()==120010);assert(!c.Grant(a,a,h,120,14));assert(c.Active(120009));assert(!c.Active(120010));c.Cancel();assert(!c.Active(20));
 NumericPairingContext expired;assert(expired.Start(a,a,123,0));assert(expired.Challenge(h,"654321",1,0));assert(!expired.Confirm(1));assert(!expired.Challenge(h,"654321",60001,0));
 std::cout<<"PASS numeric USB nonce/window/duplicates and physical confirmation/context/deadline\n";
}
#endif
