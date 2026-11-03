#pragma once
#include <string>
#include <cstdint>
namespace companion {
struct FeatureUiAction {
 int id=0;std::string request,session,code,arguments;uint32_t epoch=0;
 bool PairMatches(const std::string& r,const std::string& c)const{return !request.empty()&&!code.empty()&&request==r&&code==c;}
 bool SessionMatches(const std::string& r,const std::string& s)const{return !request.empty()&&!session.empty()&&request==r&&session==s;}
 bool AudioMatches(const std::string& r,uint32_t e)const{return !request.empty()&&request==r&&epoch&&epoch==e;}
};
}
