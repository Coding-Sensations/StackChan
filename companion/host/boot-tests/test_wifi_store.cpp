#include <ssid_manager.h>
#include <apps/network_setup/wifi_credentials.hpp>
#include <map>
#include <cassert>
std::map<std::string,std::string> legacy={{"ssid","legacy"},{"password","password"},{"account","untouched"},{"calibration","untouched"}};
std::vector<uint8_t> committed,pending;
bool commit_failure=false;int writes=0;
namespace network_setup {bool gate=false,saved=false;bool CredentialPersistenceAllowed(){return gate;}void NotifyCredentialsSaved(bool value){saved=value;}bool CredentialSaveSucceeded(){return saved;}}
int main(){
auto original=legacy;auto& store=SsidManager::GetInstance();assert(store.GetSsidList().size()==1&&store.GetSsidList()[0].ssid=="legacy");
store.AddSsid("session","password");assert(writes==0&&committed.empty()&&legacy==original);
network_setup::gate=true;store.AddSsid("new","password");assert(writes==1&&network_setup::saved&&legacy==original);
std::vector<network_setup::Credential> read;assert(network_setup::Decode(committed.data(),committed.size(),read));assert(read.front().ssid=="new");
auto good=committed;commit_failure=true;store.AddSsid("failed","password");assert(!network_setup::saved&&committed==good&&legacy==original);
int old=writes;store.AddSsid(std::string(33,'x'),"password");assert(writes==old&&!network_setup::saved&&committed==good);
}
