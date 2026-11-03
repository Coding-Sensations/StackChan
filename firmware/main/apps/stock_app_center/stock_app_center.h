#pragma once
#include <mooncake.h>
#include <memory>
#include <string>
#include <vector>

namespace stock_app_center {
// Supply only applications actually installed and usable in this shell.
struct InstalledApp {
    int app_id;
    std::string name;
    std::string description;
};
std::unique_ptr<mooncake::AppAbility> MakeApp(std::vector<InstalledApp> installed = {});
// Becomes available in onClose; consume after shell.update(), then open the ID.
int ConsumeRequestedAppId();
}
