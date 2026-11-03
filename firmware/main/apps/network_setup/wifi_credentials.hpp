#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
namespace network_setup {
struct Credential { std::string ssid, password; };
inline bool Valid(const Credential& c) {
 if(c.ssid.empty()||c.ssid.size()>32||c.ssid.find('\0')!=std::string::npos||c.password.find('\0')!=std::string::npos)return false;
 if(c.password.empty())return true;
 if(c.password.size()>=8&&c.password.size()<=63)return true;
 return c.password.size()==64&&std::all_of(c.password.begin(),c.password.end(),[](unsigned char v){return (v>='0'&&v<='9')||(v>='a'&&v<='f')||(v>='A'&&v<='F');});
}
inline uint32_t Crc(const uint8_t* data,size_t size){uint32_t c=~0u;for(size_t i=0;i<size;++i){c^=data[i];for(int j=0;j<8;++j)c=(c>>1)^((c&1)?0xedb88320u:0);}return ~c;}
inline bool Encode(const std::vector<Credential>& list,std::vector<uint8_t>& out){
 if(list.size()>10)return false;
 std::vector<uint8_t> b={'S','C','W','F',1,static_cast<uint8_t>(list.size())};
 std::vector<std::string> names;
 for(const auto& c:list){if(!Valid(c)||std::find(names.begin(),names.end(),c.ssid)!=names.end())return false;names.push_back(c.ssid);b.push_back(c.ssid.size());b.push_back(c.password.size());b.insert(b.end(),c.ssid.begin(),c.ssid.end());b.insert(b.end(),c.password.begin(),c.password.end());}
 auto crc=Crc(b.data(),b.size());for(int i=0;i<4;++i)b.push_back(crc>>(i*8));out=std::move(b);return true;
}
inline bool Decode(const uint8_t* b,size_t n,std::vector<Credential>& out){
 if(!b||n<10||n>1024||b[0]!='S'||b[1]!='C'||b[2]!='W'||b[3]!='F'||b[4]!=1||b[5]>10)return false;
 uint32_t expected=0;for(int i=0;i<4;++i)expected|=uint32_t(b[n-4+i])<<(8*i);if(Crc(b,n-4)!=expected)return false;
 std::vector<Credential> result;size_t p=6;
 for(int i=0;i<b[5];++i){if(p+2>n-4)return false;size_t a=b[p++],z=b[p++];if(a+z>n-4-p)return false;Credential c{std::string(reinterpret_cast<const char*>(b+p),a),std::string(reinterpret_cast<const char*>(b+p+a),z)};p+=a+z;if(!Valid(c))return false;for(const auto& old:result)if(old.ssid==c.ssid)return false;result.push_back(std::move(c));}
 if(p!=n-4)return false;
 out=std::move(result);return true;
}
}
