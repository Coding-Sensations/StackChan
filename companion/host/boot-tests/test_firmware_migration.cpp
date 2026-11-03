#include <ssid_manager.h>
#include <apps/stock_services/firmware_install_ui.h>
#include <nvs_flash.h>
#include <cassert>
std::map<std::string,std::string> legacy={{"ssid","old"},{"password","old-password"},{"ssid9","stale"},{"password9","stale-password"}};
std::vector<uint8_t> committed,pending;bool commit_failure=false;int writes=0;
namespace network_setup {bool CredentialPersistenceAllowed(){return true;}void NotifyCredentialsSaved(bool){}}
namespace stock_services {bool permission=false;bool FirmwareInstallAllowed(){return permission;}}
int main(){
 auto& manager=SsidManager::GetInstance();manager.AddSsid("new-network","new-password");
 auto blob=committed;assert(!blob.empty());
 assert(!stock_services::PrepareOriginalFirmwareWifi());assert(legacy["ssid"]=="old");
 stock_services::permission=true;commit_failure=true;
 assert(!stock_services::PrepareOriginalFirmwareWifi());assert(committed==blob);
 commit_failure=false;assert(stock_services::PrepareOriginalFirmwareWifi());
 assert(committed==blob&&legacy["ssid"]=="new-network"&&legacy["password"]=="new-password");
 assert(!legacy.count("ssid9")&&!legacy.count("password9"));
}
