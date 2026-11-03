#pragma once
#include <string>
#include <map>
#include <cctype>
namespace companion {
inline std::string lower(std::string s){for(auto& c:s)c=std::tolower(static_cast<unsigned char>(c));return s;}
inline std::string trim(std::string s){auto a=s.find_first_not_of(" \t");if(a==std::string::npos)return {};return s.substr(a,s.find_last_not_of(" \t")-a+1);}
inline bool valid_upgrade(const std::string& response,const std::string& expected) {
    if(response.size()>8192 || response.rfind("HTTP/1.1 101 ",0)!=0 || response.find("\r\n\r\n")==std::string::npos)return false;
    std::map<std::string,std::string> headers;
    size_t pos=response.find("\r\n")+2;
    while(pos<response.size()) {
        auto end=response.find("\r\n",pos);if(end==std::string::npos)return false;
        if(end==pos)break;
        auto line=response.substr(pos,end-pos);auto colon=line.find(':');
        if(colon==std::string::npos || line[0]==' ' || line[0]=='\t')return false;
        auto name=lower(line.substr(0,colon));if(headers.count(name))return false;
        headers[name]=trim(line.substr(colon+1));pos=end+2;
    }
    if(headers["sec-websocket-accept"]!=expected || lower(headers["upgrade"])!="websocket" || headers.count("sec-websocket-extensions") || headers.count("sec-websocket-protocol"))return false;
    auto connection=lower(headers["connection"]);size_t start=0;
    while(start<=connection.size()) {auto end=connection.find(',',start);if(trim(connection.substr(start,end-start))=="upgrade")return true;if(end==std::string::npos)break;start=end+1;}
    return false;
}
}
