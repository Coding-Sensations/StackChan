#pragma once
#include <string>
#include <cstdint>
#include <atomic>
#include <deque>
#include <mutex>
#include <memory>
#include <functional>
#include "persistent_pairing.hpp"
class NetworkInterface;class BoundedWebSocket;
namespace companion {
inline bool PairHex(const std::string& s){if(s.size()!=32)return false;for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;return s!=std::string(32,'0');}
class NumericPairingContext {
 std::string request_,device_nonce_,host_nonce_,code_;uint32_t boot_=0;uint64_t total_=0,challenge_=0,grant_=0;bool confirmed_=false,granted_=false,audio_requested_=false;
public:
 bool Start(const std::string& r,const std::string& d,uint32_t boot,uint64_t now){Cancel();if(!PairHex(r)||!PairHex(d)||!boot||now>UINT64_MAX-120000)return false;request_=r;device_nonce_=d;boot_=boot;total_=now+120000;return true;}
 bool Active(uint64_t now)const{return total_&&now<total_&&(!grant_||now<grant_);}
 bool Challenge(const std::string& h,const std::string& code,uint32_t ms,uint64_t now,bool audio_requested=false){if(!Active(now)||!host_nonce_.empty()||!PairHex(h)||code.size()!=6||!ms||ms>60000)return false;for(char c:code)if(c<'0'||c>'9')return false;host_nonce_=h;code_=code;audio_requested_=audio_requested;challenge_=now+ms;if(challenge_>total_)challenge_=total_;return true;}
 bool Confirm(uint64_t now){if(!Active(now)||!challenge_||now>=challenge_)return false;confirmed_=true;return true;}
 bool Grant(const std::string& r,const std::string& d,const std::string& h,uint32_t ttl,uint64_t now,bool audio_requested=false){if(!Active(now)||!confirmed_||granted_||audio_requested!=audio_requested_||now>=challenge_||r!=request_||d!=device_nonce_||h!=host_nonce_||!ttl||ttl>120)return false;grant_=now+uint64_t(ttl)*1000;if(grant_>total_)grant_=total_;granted_=true;code_.clear();return true;}
 bool ChallengeExpired(uint64_t now)const{return challenge_&&!granted_&&now>=challenge_;}
 void Cancel(){request_.clear();device_nonce_.clear();host_nonce_.clear();code_.clear();boot_=0;total_=challenge_=grant_=0;confirmed_=granted_=audio_requested_=false;}
 const std::string& Request()const{return request_;}const std::string& DeviceNonce()const{return device_nonce_;}const std::string& HostNonce()const{return host_nonce_;}const std::string& Code()const{return code_;}uint32_t Boot()const{return boot_;}uint64_t GrantDeadline()const{return grant_;}bool AudioRequested()const{return audio_requested_;}bool Granted()const{return granted_;}bool Confirmed()const{return confirmed_;}
};
struct NumericGrant {std::string credential,request;uint64_t expires_at_ms=0;bool audio_requested=false;};
class NumericPairing {
 NumericPairingContext context_;std::shared_ptr<BoundedWebSocket> socket_;std::mutex queue_mutex_;std::deque<std::string> queue_;std::atomic<bool> fault_{false};bool started_=false,grant_ready_=false,host_ready_=false;NumericGrant grant_;
 bool remembered_=false,remembered_ready_=false;PairPolicy remember_policy_;std::function<PairPolicy()> policy_;std::function<bool(const PairRecord&)> save_;
 bool StartMode(NetworkInterface&,const std::string&,uint32_t,uint64_t,bool);
 bool Send(const std::string&);bool Message(const std::string&,uint64_t);
public:
 void ConfigureRemembered(std::function<PairPolicy()> policy,std::function<bool(const PairRecord&)> save){policy_=std::move(policy);save_=std::move(save);}
 bool StartRemembered(NetworkInterface&,uint32_t boot,uint64_t now);bool RememberedReady()const{return remembered_ready_;}bool Remembered()const{return remembered_;}
 ~NumericPairing();bool Start(NetworkInterface&,const std::string& request,uint32_t boot,uint64_t now);
 bool StartWireless(NetworkInterface&,uint32_t boot,uint64_t now);const std::string& Request()const{return context_.Request();}
 void Tick(uint64_t now);void Cancel();bool Confirm(uint64_t now);bool Active(uint64_t now)const;const std::string& Code()const{return context_.Code();}
 bool Confirmed()const{return context_.Confirmed();}bool AudioRequested()const{return context_.AudioRequested();}
 bool ConnectionReady()const{return host_ready_;}
 bool TakeGrant(NumericGrant&);bool Staged();bool Started()const{return started_;}
};
}
