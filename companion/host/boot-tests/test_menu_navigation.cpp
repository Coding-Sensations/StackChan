#include <apps/stock_shell/navigation.h>
#include <apps/stock_shell/menu_policy.hpp>
#include <esp_system.h>
#include <cassert>
esp_reset_reason_t reset_reason=ESP_RST_SW;
namespace stock_services {std::optional<Feature> active;std::optional<Feature> CurrentFeature(){return active;}}
int main(){
 using namespace stock_shell;using stock_services::Feature;
 static_assert(LauncherNativeFeatures.size()==5);
 assert((LauncherNativeFeatures==std::array<Feature,5>{Feature::Avatar,Feature::EspNow,Feature::AppCenter,Feature::EzData,Feature::Dance}));
 for(auto feature:{Feature::Servo,Feature::Microphone,Feature::Rgb,Feature::Account,Feature::FirmwareCheck})assert(IsSetupFeature(feature));
 for(auto feature:LauncherNativeFeatures)assert(!IsSetupFeature(feature));assert(!IsSetupFeature(Feature::Onboarding));
 SetSetupContext(true);SaveSetupReturnForReset();assert(ConsumeSetupReturn());assert(!ConsumeSetupReturn());
 SetSetupContext(false);SaveSetupReturnForReset();assert(!ConsumeSetupReturn());
 stock_services::active=Feature::Microphone;SaveSetupReturnForReset();assert(ConsumeSetupReturn());assert(stock_services::active==Feature::Microphone);
 stock_services::active=Feature::Avatar;SaveSetupReturnForReset();assert(!ConsumeSetupReturn());
 stock_services::active.reset();SetSetupContext(true);SaveSetupReturnForReset();reset_reason=ESP_RST_POWERON;assert(!ConsumeSetupReturn());reset_reason=ESP_RST_SW;assert(!ConsumeSetupReturn());
 SetSetupContext(false);SaveSetupReturnForReset();assert(!ConsumeSetupReturn());
}
