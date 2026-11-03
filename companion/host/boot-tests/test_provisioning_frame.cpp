#include "provisioning_frame.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <iostream>
int main(int argc,char**argv) {
    assert(argc==2); std::ifstream f(argv[1],std::ios::binary);
    std::string data((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());
    auto p=reinterpret_cast<const uint8_t*>(data.data()); companion::ProvisioningFields fields;
    assert(companion::DecodeProvisioningFrame(p,data.size(),fields)); assert(fields.device=="synthetic-provisioning");
    companion::ProvisioningWindow w;
    assert(!w.Accept(p,data.size(),0,"synthetic-provisioning",true,fields));
    w.Open(1);assert(!w.Accept(p,data.size(),2,"synthetic-provisioning",false,fields));
    assert(!w.Accept(p,data.size(),2,"other-device",true,fields));assert(fields.credential.empty());
    assert(w.Accept(p,data.size(),2,"synthetic-provisioning",true,fields));
    assert(!w.Accept(p,data.size(),3,"synthetic-provisioning",true,fields));
    w.Open(1);assert(!w.Accept(p,data.size(),60001,"synthetic-provisioning",true,fields));
    for(size_t n=0;n<data.size();++n)assert(!companion::DecodeProvisioningFrame(p,n,fields));
    auto bad=data;bad.push_back('x');assert(!companion::DecodeProvisioningFrame(reinterpret_cast<const uint8_t*>(bad.data()),bad.size(),fields));
    bad=data;bad[4]=2;assert(!companion::DecodeProvisioningFrame(reinterpret_cast<const uint8_t*>(bad.data()),bad.size(),fields));
    std::cout<<"PASS cross-language frame, truncations, expiry, consent gate, binding, replay\n";
}
