#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
namespace companion {
inline bool FeatureHex(const std::string& s){return s.size()==32&&s!=std::string(32,'0')&&std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});}
inline bool FeatureScopes(const std::vector<std::string>& s){
 const std::vector<std::string> allowed={"apple.home.execute","apple.home.scenes","apple.now_playing","desktop.health","mira.forwarding.interrupt","mira.forwarding.metadata","mira.forwarding.status","network.wol"};
 return !s.empty()&&s.size()<=allowed.size()&&std::is_sorted(s.begin(),s.end())&&std::adjacent_find(s.begin(),s.end())==s.end()&&std::all_of(s.begin(),s.end(),[&](const std::string& c){return std::binary_search(allowed.begin(),allowed.end(),c);});
}
// One exact request at a time. Timeout/send failure never permits automatic retry.
class FeatureRequestState {
 std::string session_,request_,capability_;uint32_t sequence_=0;uint64_t until_=0,expires_=0;bool pending_=false,unknown_=false;
public:
 bool Open(const std::string& session,uint64_t expires,uint64_t now){Close();if(!FeatureHex(session)||expires<=now)return false;session_=session;expires_=expires;return true;}
 bool Begin(const std::string& id,const std::string& cap,uint64_t now){if(!Active(now)||pending_||unknown_||!FeatureHex(id)||sequence_==UINT32_MAX)return false;request_=id;capability_=cap;until_=expires_;pending_=true;return true;}
 bool Match(const std::string& session,const std::string& id,const std::string& cap,uint32_t seq,uint64_t now)const{return Active(now)&&pending_&&now<until_&&session==session_&&id==request_&&cap==capability_&&seq==sequence_;}
 bool Complete(bool known){if(!pending_)return false;pending_=false;unknown_=!known;++sequence_;return true;}
 bool Tick(uint64_t now){if(!session_.empty()&&(!Active(now)||(pending_&&now>=until_))){pending_=false;unknown_=true;return false;}return true;}
 void Uncertain(){pending_=false;unknown_=true;}void Close(){session_.clear();request_.clear();capability_.clear();sequence_=0;until_=expires_=0;pending_=unknown_=false;}
 bool Active(uint64_t now)const{return !session_.empty()&&now<expires_;}bool Pending()const{return pending_;}bool Unknown()const{return unknown_;}
 const std::string& Session()const{return session_;}const std::string& Request()const{return request_;}const std::string& Capability()const{return capability_;}uint32_t Sequence()const{return sequence_;}
};
}
