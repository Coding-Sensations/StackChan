#include <apps/network_setup/wifi_credentials.hpp>
#include <cassert>
using namespace network_setup;
int main(){
assert(Valid({"open",""}));assert(!Valid({"", "password"}));assert(!Valid({std::string(33,'a'),"password"}));
assert(Valid({std::string(32,'a'),std::string(64,'f')}));assert(!Valid({"ssid",std::string(64,'z')}));assert(!Valid({"ssid","short"}));
std::vector<Credential> list={{"first","password"},{std::string(32,'a'),std::string(64,'f')},{"open",""}},decoded={{"kept","password"}};
std::vector<uint8_t> b;assert(Encode(list,b));assert(Decode(b.data(),b.size(),decoded));assert(decoded.size()==3&&decoded[1].ssid.size()==32&&decoded[1].password.size()==64);
auto saved=decoded;for(size_t n=0;n<b.size();++n){assert(!Decode(b.data(),n,decoded));assert(decoded.size()==saved.size());}
for(size_t n=0;n<b.size();++n){b[n]^=1;assert(!Decode(b.data(),b.size(),decoded));b[n]^=1;}
assert(!Encode({{"duplicate","password"},{"duplicate","password"}},b));assert(!Encode(std::vector<Credential>(11,{"valid","password"}),b));
assert(Encode({},b)&&Decode(b.data(),b.size(),decoded)&&decoded.empty());
}
