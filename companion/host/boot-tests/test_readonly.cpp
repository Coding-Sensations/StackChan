#include <settings.h>
#include <cassert>
#include <companion/audio_owner.hpp>
#include <companion/settings_policy.hpp>
namespace network_setup { bool gate=false; bool CredentialPersistenceAllowed(){return gate;} }
int writes=0,commits=0,opens=0,closes=0,last_mode=-1;
int main(){
// This fixture holds in-memory custody only; no hardware or worker is opened.
companion::AudioLease stock_lease;
{Settings s("board",true);assert(last_mode==NVS_READONLY);
s.SetString("uuid","new");s.SetInt("volume",10);s.SetBool("boot",true);s.EraseKey("pairing");s.EraseAll();
assert(s.GetString("missing","kept")=="kept");assert(s.GetInt("missing",42)==42);assert(s.GetBool("missing",true));}
assert(opens==1 && closes==1 && writes==0 && commits==0);
{Settings s("mqtt",true);
 s.SetString("endpoint","blocked");assert(s.GetString("endpoint","none")=="none");
 assert(companion::AcquireAudio(companion::AudioOwner::Stock,companion::AudioPurpose::StockRuntime,1,stock_lease));
 s.SetString("endpoint","session-only");s.SetInt("port",8883);
 assert(s.GetString("endpoint")=="session-only"&&s.GetInt("port")==8883);
 Settings board("board",true);board.SetString("uuid","still-blocked");
 assert(board.GetString("uuid","kept")=="kept");
 assert(companion::ReleaseAudio(stock_lease,true));
 s.SetString("endpoint","blocked-after-stop");assert(s.GetString("endpoint")=="session-only");
}
assert(writes==0&&commits==0&&opens==closes);
companion::SetSettingsScope(companion::SettingsScope::Timezone);
{Settings s("system",true);assert(last_mode==NVS_READWRITE);s.SetString("tz","GMT0");s.SetString("unrelated","blocked");}
assert(writes==1&&commits==1);
companion::SetSettingsScope(companion::SettingsScope::Volume);
{Settings s("audio",true);s.SetInt("output_volume",50);s.SetInt("gain",8);}
assert(writes==2&&commits==2);
companion::SetSettingsScope(companion::SettingsScope::None);
{Settings s("audio",true);s.SetInt("output_volume",80);}
assert(writes==2&&commits==2);
network_setup::gate=true;
{Settings s("app_config",true);s.SetBool("is_configed",true);s.SetString("unrelated","blocked");}
assert(writes==3&&commits==3);
network_setup::gate=false;
{Settings s("app_config",true);s.SetBool("is_configed",false);}
assert(writes==3&&commits==3&&opens==closes);
companion::SetSettingsScope(companion::SettingsScope::StockRuntime);
{Settings s("board",true);s.SetString("uuid","no-owner");}
assert(writes==3&&commits==3);
assert(companion::AcquireAudio(companion::AudioOwner::Stock,companion::AudioPurpose::StockRuntime,2,stock_lease));
{Settings s("board",true);s.SetString("uuid","persistent");s.SetString("other","blocked");}
{Settings s("mqtt",true);s.SetString("endpoint","persisted");assert(s.GetString("endpoint","nvs-default")=="nvs-default");}
{Settings s("audio",true);s.SetInt("output_volume",50);s.SetInt("gain",8);}
{Settings s("display",true);s.SetInt("brightness",70);}
{Settings s("assets",true);s.SetString("download_url","https://example.com/assets");s.EraseKey("download_url");}
assert(writes==9&&commits==8&&opens==closes);
assert(companion::ReleaseAudio(stock_lease,true));
companion::SetSettingsScope(companion::SettingsScope::None);

}
