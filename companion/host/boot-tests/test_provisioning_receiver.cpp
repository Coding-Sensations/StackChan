#include "provisioning_receiver.hpp"
#include "connect_gate.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <iostream>
int main(int argc,char**argv) {
 assert(argc==2);std::ifstream f(argv[1],std::ios::binary);std::string b((std::istreambuf_iterator<char>(f)),{});
 companion::ConnectGate connect;
 connect.Request(false); assert(!connect.Ready(true));
 connect.Request(true); assert(!connect.Ready(false)); assert(connect.Ready(true));
 connect.Cancel(); // Mute leaves Wi-Fi connected; later grant must not connect.
 assert(!connect.Ready(true));
 connect.Request(true); assert(connect.Ready(true));
 connect.Cancel(); // Pairing invalidates previous Connect consent.
 assert(!connect.Ready(true));
 auto data=reinterpret_cast<const uint8_t*>(b.data());using R=companion::ProvisioningRead;
 companion::ProvisioningReceiver rx;companion::ProvisioningFields out;
 assert(!rx.Open(0,"synthetic-provisioning",false));assert(rx.Feed(data,b.size(),1,out)==R::Rejected);
 assert(rx.Open(0,"synthetic-provisioning",true));
 for(size_t i=0;i<b.size();++i)assert(rx.Feed(data+i,1,i+1,out)==(i+1==b.size()?R::Accepted:R::Waiting));
 assert(out.credential=="synthetic-token-no-authority");assert(!rx.Active());
 assert(rx.Feed(data,b.size(),200,out)==R::Rejected);assert(out.credential.empty());
 assert(rx.Open(0,"synthetic-provisioning",true));assert(rx.Feed(data,3,1,out)==R::Waiting);
 assert(rx.Feed(nullptr,0,60000,out)==R::Rejected);assert(!rx.Active());
 assert(rx.Open(0,"different-device",true));assert(rx.Feed(data,b.size(),1,out)==R::Rejected);
 assert(rx.Open(0,"synthetic-provisioning",true));std::string bad=b;bad.push_back('x');assert(rx.Feed(reinterpret_cast<const uint8_t*>(bad.data()),bad.size(),1,out)==R::Rejected);
 assert(rx.Open(0,"synthetic-provisioning",true));uint8_t huge[756]{};assert(rx.Feed(huge,sizeof(huge),1,out)==R::Rejected);
 assert(rx.Open(0,"synthetic-provisioning",true));uint8_t wrong[7]{};assert(rx.Feed(wrong,sizeof(wrong),1,out)==R::Rejected);
 assert(rx.Open(0,"synthetic-provisioning",true));rx.Close();assert(!rx.Active());
 std::cout<<"PASS receiver fragmentation, default deny, device binding, expiry, overflow, trailing bytes, close\n";
}
