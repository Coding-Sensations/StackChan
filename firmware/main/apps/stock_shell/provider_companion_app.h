#pragma once
#include <memory>
#include <mooncake.h>
#include "provider_catalog.hpp"
namespace stock_tools {
// Honest, inert setup view. Existing StackChan/Mira keep their production factories.
std::unique_ptr<mooncake::AppAbility> MakeProviderCompanionApp(companion_modes::Provider provider);
}
