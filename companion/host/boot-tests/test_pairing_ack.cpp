#if !__has_include("companion/provisioning_ack.hpp")
#include <cstdio>
int main(){std::fputs("FAIL: pairing acknowledgement protocol absent\n",stderr);return 1;}
#else
#include <companion/provisioning_ack.hpp>
#include <companion/usb_owner.hpp>
#include <cassert>
#include <deque>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
using companion::AckStatus;
struct Driver {
 bool installed=false,cpu0=true,write_ok=true,flush_ok=true;
 unsigned installs=0,reads=0,uninstalls=0;size_t read_limit=128;
 std::deque<uint8_t> input;std::vector<companion::AckFrame> acks;std::vector<char> events;
 bool OnOwnerCore()const{return cpu0;}bool Installed()const{return installed;}
 bool Install(){++installs;installed=true;return true;}
 bool Uninstall(){assert(cpu0&&installed);++uninstalls;events.push_back('U');installed=false;return true;}
 int Read(uint8_t* p,size_t n){assert(cpu0&&installed);++reads;size_t done=0;while(done<std::min(n,read_limit)&&!input.empty()){p[done++]=input.front();input.pop_front();}return static_cast<int>(done);}
 int WriteAck(const uint8_t* p,size_t n){assert(cpu0&&installed&&n==22);events.push_back('W');companion::AckFrame a{};std::copy(p,p+n,a.begin());acks.push_back(a);return write_ok?static_cast<int>(n):static_cast<int>(n-1);}
 bool FlushAck(){assert(cpu0&&installed);events.push_back('F');return flush_ok;}
 void Queue(const std::vector<uint8_t>& b){input.insert(input.end(),b.begin(),b.end());}
};
using Owner=companion::UsbProvisioningOwner<Driver>;using Read=companion::ProvisioningRead;
static constexpr const char* uuid="00112233445566778899aabbccddeeff";
std::vector<uint8_t> Frame(const std::string& request=uuid){
 std::vector<uint8_t> b{'S','C','P','G',1,0,0};
 for(const auto& s:std::vector<std::string>{"wss://192.168.42.10:18443/companion/audio","test-device","never-an-authorized-secret",request}){b.push_back(s.size()>>8);b.push_back(s.size()&255);b.insert(b.end(),s.begin(),s.end());}
 auto n=b.size()-7;b[5]=n>>8;b[6]=n&255;return b;
}
void Header(const companion::AckFrame& a,AckStatus status){assert(a.size()==22);assert(a[0]=='S'&&a[1]=='C'&&a[2]=='P'&&a[3]=='S'&&a[4]==1&&a[5]==static_cast<uint8_t>(status));}
void Empty(const companion::AckFrame& a){assert(std::all_of(a.begin()+6,a.end(),[](uint8_t n){return n==0;}));}
void ClosedAfterAck(const Driver& d){assert(d.events.size()>=3);auto n=d.events.size();assert(d.events[n-3]=='W'&&d.events[n-2]=='F'&&d.events[n-1]=='U');assert(!d.installed);}
Read Feed(Owner& o,Driver&,companion::ProvisioningFields& f){Read r=Read::Waiting;for(unsigned i=0;i<100&&r==Read::Waiting;++i)r=o.Poll(11+i,f);return r;}
int main(){
 auto ready=companion::EncodeReadyAck(0x12345678,64);Header(ready,AckStatus::Ready);
 const std::vector<uint8_t> payload{0x12,0x34,0x56,0x78,0,0,0,64,0,0,0,0,0,0,0,0};assert(std::equal(payload.begin(),payload.end(),ready.begin()+6));
 auto stage=companion::EncodeStagedAck(uuid);Header(stage,AckStatus::Staged);for(unsigned i=0;i<16;++i)assert(stage[6+i]==i*17);
 assert(companion::EncodeStagedAck("00112233445566778899AABBCCDDEEFF")==stage);
 for(const auto& s:std::vector<std::string>{"legacy-request","","00112233445566778899aabbccddeefg"}){auto a=companion::EncodeStagedAck(s);Header(a,AckStatus::Staged);Empty(a);}
 for(auto status:{AckStatus::Rejected,AckStatus::Expired}){auto a=companion::EncodeStatusAck(status);Header(a,status);Empty(a);}
 companion::ProvisioningFields fields;
 for(auto values:std::vector<std::pair<uint32_t,uint32_t>>{{0,0},{1,65}}){Driver d;Owner o(d);assert(!o.Open(10,"test-device",true,values.first,values.second));assert(d.acks.empty()&&d.installs==0);}
 {Driver d;Owner o(d);assert(!o.CompleteAccepted(true,uuid));assert(d.acks.empty());}
 // Fragmented real parser: staging ACK only after caller commits the accepted grant.
 {Driver d;Owner o(d);assert(o.Open(10,"test-device",true,0x12345678,64));assert(d.acks.size()==1&&d.acks[0]==ready);d.read_limit=4;d.Queue(Frame());assert(Feed(o,d,fields)==Read::Accepted);assert(fields.request==uuid&&o.OwnsDriver()&&d.acks.size()==1&&d.uninstalls==0);assert(o.CompleteAccepted(true,fields.request));assert(d.acks.back()==stage);ClosedAfterAck(d);auto n=d.acks.size();assert(o.Poll(200,fields)==Read::Rejected&&fields.credential.empty());assert(d.acks.size()==n);}
 // Accepted-but-not-staged is rejected; no token or URI occurs in ACK payload.
 {Driver d;Owner o(d);assert(o.Open(10,"test-device",true,9,0));d.Queue(Frame());assert(Feed(o,d,fields)==Read::Accepted);assert(o.CompleteAccepted(false,fields.request));Header(d.acks.back(),AckStatus::Rejected);Empty(d.acks.back());ClosedAfterAck(d);}
 // Invalid and duplicate/coalesced grants fail closed, never emit Staged.
 for(bool duplicate:{false,true}){Driver d;Owner o(d);assert(o.Open(10,"test-device",true));auto b=Frame();if(duplicate){auto second=b;b.insert(b.end(),second.begin(),second.end());}else b[0]='X';d.Queue(b);assert(Feed(o,d,fields)==Read::Rejected);assert(fields.credential.empty());Header(d.acks.back(),AckStatus::Rejected);Empty(d.acks.back());ClosedAfterAck(d);}
 {Driver d;Owner o(d);assert(o.Open(10,"test-device",true));d.Queue(Frame());auto n=d.reads;assert(o.Poll(60010,fields)==Read::Rejected);assert(d.reads==n);Header(d.acks.back(),AckStatus::Expired);Empty(d.acks.back());ClosedAfterAck(d);}
 // Another core/driver is never touched, including ACK writes.
 {Driver d;d.cpu0=false;Owner o(d);assert(!o.Open(10,"test-device",true));assert(d.reads==0&&d.installs==0&&d.acks.empty());}
 {Driver d;d.installed=true;Owner o(d);assert(!o.Open(10,"test-device",true));assert(d.reads==0&&d.uninstalls==0&&d.acks.empty());}
 {Driver d;Owner o(d);assert(o.Open(10,"test-device",true));d.cpu0=false;auto n=d.acks.size();assert(o.Poll(11,fields)==Read::Rejected);assert(d.acks.size()==n&&d.uninstalls==0);d.cpu0=true;assert(o.Close());}
 // Reset/lost-driver state has no ACK or reads; reopening requires a fresh window/nonce.
 {Driver d;Owner o(d);assert(o.Open(10,"test-device",true,11,0));d.installed=false;auto n=d.reads;assert(o.Poll(11,fields)==Read::Rejected);assert(d.reads==n&&d.acks.size()==1&&d.uninstalls==0);assert(o.Open(20,"test-device",true,22,0));assert(d.acks.back()==companion::EncodeReadyAck(22,0));assert(o.Close());}
 // Stale FIFO cannot become a staged grant or Ready signal for that window.
 {Driver d;d.Queue(Frame());Owner o(d);assert(!o.Open(10,"test-device",true));for(auto a:d.acks)assert(a[5]!=static_cast<uint8_t>(AckStatus::Ready)&&a[5]!=static_cast<uint8_t>(AckStatus::Staged));assert(!o.OwnsDriver());}
 // Exact Ready and terminal ACK write/flush failures never report success.
 for(bool flush_failure:{false,true}){Driver d;d.write_ok=flush_failure;d.flush_ok=!flush_failure;Owner o(d);assert(!o.Open(10,"test-device",true));assert(!d.installed&&!o.OwnsDriver());}
 for(bool flush_failure:{false,true}){Driver d;Owner o(d);assert(o.Open(10,"test-device",true));d.Queue(Frame());assert(Feed(o,d,fields)==Read::Accepted);d.write_ok=flush_failure;d.flush_ok=!flush_failure;assert(!o.CompleteAccepted(true,fields.request));assert(!d.installed&&!o.OwnsDriver());}
 std::cout<<"PASS exact pairing ACK protocol, staged ownership, bounds, failures, expiry and fresh windows\n";
}
#endif
