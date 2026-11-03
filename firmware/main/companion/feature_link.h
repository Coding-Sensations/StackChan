#pragma once
#include "feature_protocol.hpp"
#include <companion/numeric_pairing.h>
#include <companion/bounded_web_socket.h>
#include <atomic>
#include <deque>
#include <mutex>
#include <memory>
#include <optional>
class NetworkInterface;
namespace companion {
class FeatureLink;
class FeatureAudioGrant {
 std::string credential_,request_;uint64_t deadline_=0;FeatureLink* source_=nullptr;
 friend class FeatureLink;friend bool StageFeatureAudioGrant(FeatureLink&,uint64_t);
 FeatureAudioGrant(std::string credential,std::string request,uint64_t deadline,FeatureLink* source):credential_(std::move(credential)),request_(std::move(request)),deadline_(deadline),source_(source){}
public:
 FeatureAudioGrant(const FeatureAudioGrant&)=delete;FeatureAudioGrant& operator=(const FeatureAudioGrant&)=delete;
 FeatureAudioGrant(FeatureAudioGrant&& g):credential_(std::move(g.credential_)),request_(std::move(g.request_)),deadline_(g.deadline_),source_(g.source_){g.source_=nullptr;g.deadline_=0;}
 FeatureAudioGrant& operator=(FeatureAudioGrant&&)=delete;
 ~FeatureAudioGrant(){volatile char* b=credential_.empty()?nullptr:&credential_[0];for(size_t n=0;n<credential_.size();++n)b[n]=0;}
 const std::string& Request()const{return request_;}uint64_t Deadline()const{return deadline_;}
};
struct FeatureResource {std::string id,label;};
class FeatureLink {
 NumericPairingContext pair_;FeatureRequestState requests_;std::vector<std::string> scopes_;
 std::shared_ptr<BoundedWebSocket> socket_,pair_socket_;std::mutex mutex_;std::deque<std::string> queue_;std::atomic<bool> fault_{false};
 bool granted_=false,ready_=false,feature_socket_=false,audio_selected_=false,audio_taken_=false,host_ready_=false;std::vector<std::string> grant_scopes_;std::string audio_credential_;std::string credential_,notice_,result_;NetworkInterface* network_=nullptr;
 std::vector<FeatureResource> volumes_,targets_;std::string mira_ref_;uint32_t mira_epoch_=0;
 bool Send(const std::string&);bool Message(const std::string&,uint64_t);bool OpenFeature();
public:
 ~FeatureLink(){Close();}
 bool StartPair(NetworkInterface&,const std::string& request,uint32_t boot,const std::vector<std::string>& scopes,uint64_t now,bool audio=false);
 bool Confirm(uint64_t now);void Tick(uint64_t now);void Close();
 bool Request(const std::string& capability,const std::string& arguments,uint64_t now);
 const std::string& Code()const{return pair_.Code();}const std::string& Notice()const{return notice_;}const std::string& Result()const{return result_;}
 bool Ready(uint64_t now)const{return ready_&&!fault_&&requests_.Active(now)&&!requests_.Unknown();}bool Pending()const{return requests_.Pending();}
 bool AudioSelected()const{return audio_selected_;}
 bool Stopped()const{return !socket_&&!pair_socket_&&!ready_&&!granted_&&!feature_socket_&&!requests_.Pending();}
 bool AudioAuthorityCurrent(uint64_t now)const{return audio_selected_&&host_ready_&&granted_&&!fault_&&pair_.Active(now)&&pair_.Confirmed()&&socket_&&pair_socket_;}
 bool MatchesAudioGrant(const std::string& request,uint64_t deadline,uint64_t now)const{return AudioAuthorityCurrent(now)&&request==pair_.Request()&&deadline==pair_.GrantDeadline();}
 std::optional<FeatureAudioGrant> TakeAudioGrant(uint64_t now);
 const std::string& PairRequest()const{return pair_.Request();}
 const std::string& SessionId()const{return requests_.Session();}
 bool Pairing(uint64_t now)const{return socket_&&!feature_socket_&&pair_.Active(now);}
 const std::vector<FeatureResource>& Volumes()const{return volumes_;}const std::vector<FeatureResource>& Targets()const{return targets_;}
 const std::string& MiraReference()const{return mira_ref_;}uint32_t MiraEpoch()const{return mira_epoch_;}
 const std::vector<std::string>& Scopes()const{return scopes_;}
};
}
