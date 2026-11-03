#pragma once
#include <memory>
#include <mooncake.h>
namespace stock_tools {
std::unique_ptr<mooncake::AppAbility> MakeLedExpressionApp();
}
