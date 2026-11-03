#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
namespace companion::survey {
constexpr size_t MaxAccessPoints=32,MaxSamples=64;
struct Ap {std::array<char,129> ssid{};std::array<uint8_t,6> bssid{};uint8_t channel=0,security=0;int8_t rssi=0;};
enum class Phase {Idle,Scanning,Complete,NotReady,Busy,Failed,Stopping,CleanupFailed};
struct Snapshot {Phase phase=Phase::Idle;std::array<Ap,MaxAccessPoints> aps{};size_t count=0;uint32_t generation=0;uint64_t observed_ms=0;bool truncated=false;};
inline const char* Label(Phase p){switch(p){case Phase::Idle:return "Tap Scan to survey nearby APs";case Phase::Scanning:return "Scanning nearby 2.4 GHz APs";case Phase::Complete:return "Scan complete; samples stay in RAM";case Phase::NotReady:return "Shared Wi-Fi unavailable; restart needed";case Phase::Busy:return "Wi-Fi connection/voice is busy";case Phase::Failed:return "Scan failed; no sample collected";case Phase::Stopping:return "Stopping owned scan";case Phase::CleanupFailed:return "Scan cleanup pending; retry Back";}return "Scan unavailable";}
// Driver Start uses only the existing station owner's initialized radio. No
// credentials, traffic capture, RF location estimation or independent Wi-Fi init.
template<class Driver> class Controller {
 Driver& driver_;Snapshot value_{};uint64_t deadline_=0;bool custody_=false,cleanup_retry_required_=false;
public:
 explicit Controller(Driver& d):driver_(d){}
 const Snapshot& Get()const{return value_;}bool OwnsScan()const{return custody_;}
 bool Start(uint64_t now){
  if(custody_)return false;
  const auto result=driver_.Start();
  if(result!=Phase::Scanning){value_.phase=result;value_.count=0;return false;}
  custody_=true;cleanup_retry_required_=false;value_.phase=Phase::Scanning;value_.count=0;value_.truncated=false;deadline_=now+8000;return true;
 }
 bool Cancel(){
  if(!custody_)return true;
  cleanup_retry_required_=false;
  if(driver_.Done()){value_.phase=Phase::Stopping;return false;}
  if(!driver_.Stop()){value_.phase=Phase::CleanupFailed;return false;}
  value_.phase=Phase::Stopping;return false; // Completion still owned; no guessed closure.
 }
 void Poll(uint64_t now){
  if(!custody_||cleanup_retry_required_)return;
  if(driver_.Done()){
   const bool canceled=value_.phase==Phase::Stopping||value_.phase==Phase::CleanupFailed;
   size_t count=0;bool truncated=false;
   const bool success=driver_.Success()&&!canceled&&driver_.Read(value_.aps,count,truncated);
   if(!driver_.Clear()){value_.phase=Phase::CleanupFailed;cleanup_retry_required_=true;return;}
   custody_=false;value_.count=success?count:0;value_.truncated=success&&truncated;
   value_.phase=canceled?Phase::Idle:success?Phase::Complete:Phase::Failed;
   if(success){++value_.generation;value_.observed_ms=now;}return;
  }
  if(value_.phase==Phase::Scanning&&now>=deadline_)Cancel();
 }
};
struct Sample {std::array<char,25> location{};std::array<uint8_t,6> bssid{};int8_t rssi=0;uint8_t channel=0;uint32_t generation=0;uint64_t observed_ms=0;};
class Samples {
 std::array<Sample,MaxSamples> values_{};size_t count_=0;uint32_t scan_prior_generation_=0;std::string scan_location_;
public:
 size_t Count()const{return count_;}const Sample& At(size_t i)const{return values_.at(i);}
 static bool ValidLocation(const std::string& name){if(name.empty()||name.size()>24)return false;for(auto c:name)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c==' '||c=='_'||c=='-'||c=='.'))return false;return name.find_first_not_of(' ')!=std::string::npos;}
 void BeginScan(const std::string& location,uint32_t previous_generation){scan_location_=location;scan_prior_generation_=previous_generation;}
 bool Record(const std::string& location,const Snapshot& current,size_t selected){
  if(!ValidLocation(location)||current.phase!=Phase::Complete||selected>=current.count||!current.generation||current.generation==scan_prior_generation_||location!=scan_location_||count_==MaxSamples)return false;
  for(size_t i=0;i<count_;++i)if(values_[i].generation==current.generation&&values_[i].bssid==current.aps[selected].bssid&&location==values_[i].location.data())return false;
  auto& s=values_[count_++];std::memcpy(s.location.data(),location.data(),location.size());s.bssid=current.aps[selected].bssid;s.rssi=current.aps[selected].rssi;s.channel=current.aps[selected].channel;s.generation=current.generation;s.observed_ms=current.observed_ms;return true;
 }
 bool Trend(const std::array<uint8_t,6>& bssid,int& delta)const{
  const Sample* newest=nullptr;for(size_t i=count_;i>0;--i)if(values_[i-1].bssid==bssid){if(!newest)newest=&values_[i-1];else{delta=int(newest->rssi)-int(values_[i-1].rssi);return true;}}return false;
 }
};
}
