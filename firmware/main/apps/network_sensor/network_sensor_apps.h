#pragma once
#include <mooncake.h>
#include <memory>
namespace stock_tools {
std::unique_ptr<mooncake::AppAbility> MakeProximityApp();
std::unique_ptr<mooncake::AppAbility> MakeNetworkToolsApp();
}
