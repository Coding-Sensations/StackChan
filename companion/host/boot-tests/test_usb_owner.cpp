#if !__has_include("companion/usb_owner.hpp")
#include <cstdio>
int main(){std::fputs("FAIL: bounded USB provisioning ownership is absent\n",stderr);return 1;}
#else
#include <companion/usb_owner.hpp>
#include <cassert>
#include <deque>
#include <iostream>
#include <vector>
struct SdkDriver {
 bool installed=false,cpu0=true,install_ok=true,uninstall_ok=true,read_error=false;
 unsigned installs=0,uninstalls=0,reads=0;std::deque<uint8_t> incoming;
 bool OnOwnerCore()const{return cpu0;}
 bool Installed()const{return installed;}
 bool Install(){++installs;if(!install_ok)return false;installed=true;return true;}
 bool Uninstall(){++uninstalls;if(!uninstall_ok)return false;installed=false;return true;}
 int Read(uint8_t* bytes,size_t capacity){++reads;if(read_error)return -1;size_t n=0;while(n<capacity&&!incoming.empty()){bytes[n++]=incoming.front();incoming.pop_front();}return static_cast<int>(n);}
 int WriteAck(const uint8_t*,size_t size){return static_cast<int>(size);}
 bool FlushAck(){return true;}
 void Queue(const std::vector<uint8_t>& bytes){incoming.insert(incoming.end(),bytes.begin(),bytes.end());}
};
std::vector<uint8_t> frame(){
 std::vector<uint8_t> b{'S','C','P','G',1,0,0};
 for(const char* s:{"wss://example.invalid/companion/audio","synthetic-device","synthetic-token-no-authority","synthetic-request"}){
  std::string text(s);b.push_back(text.size()>>8);b.push_back(text.size()&255);b.insert(b.end(),text.begin(),text.end());
 }
 size_t body=b.size()-7;b[5]=body>>8;b[6]=body&255;return b;
}
int main(){
 using Owner=companion::UsbProvisioningOwner<SdkDriver>;using Read=companion::ProvisioningRead;
 companion::ProvisioningFields fields;
 // A foreign installed console driver must never be read or removed.
 {SdkDriver d;d.installed=true;{Owner o(d);assert(!o.Open(0,"synthetic-device",true));assert(o.Close());}assert(d.installed&&d.reads==0&&d.uninstalls==0&&d.installs==0);}
 // Default-disabled and wrong-core consent cannot install or read USB.
 {SdkDriver d;Owner o(d);assert(!o.Open(0,"synthetic-device",false));d.cpu0=false;assert(!o.Open(0,"synthetic-device",true));assert(o.Poll(1,fields)==Read::Rejected);assert(d.installs==0&&d.reads==0);}
 {SdkDriver d;d.install_ok=false;Owner o(d);assert(!o.Open(0,"synthetic-device",true));assert(!o.OwnsDriver()&&d.uninstalls==0);}
 // Stale FIFO bytes (including a complete grant) cannot survive into consent.
 {SdkDriver d;d.Queue(frame());Owner o(d);assert(!o.Open(0,"synthetic-device",true));assert(!o.Active()&&!d.installed&&d.uninstalls==1);assert(fields.credential.empty());}
 {SdkDriver d;d.incoming.resize(4096,'x');Owner o(d);assert(!o.Open(0,"synthetic-device",true));assert(d.reads<=8&&d.uninstalls==1);}
 // Normal frame ingestion uses real receiver parsing, accepts once, retains ownership until staging receipt.
 {SdkDriver d;Owner o(d);assert(o.Open(10,"synthetic-device",true));d.Queue(frame());assert(o.Poll(11,fields)==Read::Accepted);assert(fields.credential=="synthetic-token-no-authority");assert(!o.Active()&&o.OwnsDriver()&&d.uninstalls==0);assert(o.CompleteAccepted(true,fields.request));assert(!o.OwnsDriver()&&d.uninstalls==1);assert(o.Poll(12,fields)==Read::Rejected&&fields.credential.empty());}
 // A successful receipt retains the staged outcome even if teardown fails.
 {SdkDriver d;Owner o(d);assert(o.Open(0,"synthetic-device",true));d.Queue(frame());assert(o.Poll(1,fields)==Read::Accepted);d.uninstall_ok=false;assert(o.CompleteAccepted(true,fields.request)&&o.OwnsDriver());assert(o.LastError()==companion::UsbOwnershipError::CloseFailed);d.uninstall_ok=true;assert(o.Close());}
 // Expired or wrong-core polls reject before reading queued tokens.
 {SdkDriver d;Owner o(d);assert(o.Open(0,"synthetic-device",true));d.Queue(frame());auto n=d.reads;assert(o.Poll(60000,fields)==Read::Rejected);assert(d.reads==n&&d.uninstalls==1&&fields.credential.empty());}
 {SdkDriver d;Owner o(d);assert(o.Open(0,"synthetic-device",true));d.cpu0=false;auto n=d.reads;assert(o.Poll(1,fields)==Read::Rejected);assert(d.reads==n&&d.uninstalls==0&&o.OwnsDriver());d.cpu0=true;assert(o.Close()&&d.uninstalls==1);}
 // Oversize frame is rejected, its partial credential wiped, driver closed.
 {SdkDriver d;Owner o(d);assert(o.Open(0,"synthetic-device",true));d.Queue({'S','C','P','G',1,3,0});assert(o.Poll(1,fields)==Read::Rejected&&fields.credential.empty()&&!o.OwnsDriver());}
 // An externally removed driver cannot be read or uninstalled again.
 {SdkDriver d;Owner o(d);assert(o.Open(0,"synthetic-device",true));d.installed=false;auto n=d.reads;assert(o.Poll(1,fields)==Read::Rejected);assert(d.reads==n&&d.uninstalls==0&&!o.OwnsDriver());}
 // Close failures retain ownership, block reopening and permit a safe retry.
 {SdkDriver d;Owner o(d);assert(o.Open(0,"synthetic-device",true));d.uninstall_ok=false;assert(!o.Close()&&o.OwnsDriver()&&!o.Active());assert(o.LastError()==companion::UsbOwnershipError::CloseFailed);assert(!o.Open(1,"synthetic-device",true)&&d.installs==1);d.uninstall_ok=true;assert(o.Close()&&!o.OwnsDriver());}
 {SdkDriver d;{Owner o(d);assert(o.Open(0,"synthetic-device",true));}assert(!d.installed&&d.uninstalls==1);}
 {SdkDriver d;Owner o(d);assert(o.Open(0,"synthetic-device",true));d.read_error=true;assert(o.Poll(1,fields)==Read::Rejected&&!o.Active()&&d.uninstalls==1);}
 std::cout<<"PASS USB ownership, foreign driver refusal, core affinity, stale FIFO, expiry, bounds and close failure\n";
}
#endif
